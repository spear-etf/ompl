/*********************************************************************
* Software License Agreement (BSD License)
*
*  Copyright (c) 2017, Rice University
*  All rights reserved.
*
*  Redistribution and use in source and binary forms, with or without
*  modification, are permitted provided that the following conditions
*  are met:
*
*   * Redistributions of source code must retain the above copyright
*     notice, this list of conditions and the following disclaimer.
*   * Redistributions in binary form must reproduce the above
*     copyright notice, this list of conditions and the following
*     disclaimer in the documentation and/or other materials provided
*     with the distribution.
*   * Neither the name of the Rice University nor the names of its
*     contributors may be used to endorse or promote products derived
*     from this software without specific prior written permission.
*
*  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
*  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
*  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
*  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
*  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
*  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
*  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
*  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
*  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
*  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
*  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
*  POSSIBILITY OF SUCH DAMAGE.
*********************************************************************/

/* Author: Zachary Kingston */

#include <ompl/tools/benchmark/Benchmark.h>
#include "ConstrainedPlanningCommon.h"

const double memory_limit = 2048;
const double update_interval = 0.1;
const bool progress = false;
const bool save_output = false;
const bool use_threads = false;
const bool simplify = true;

/** Print usage information. Does not return. */
void usage(const char *const progname)
{
    std::cout << "Usage: " << progname << " -c <problem> -p <planner> -s <space> -t <timelimit> -w <sleep> -o\n";
    printProblems();
    printPlanners();
    exit(0);
}


enum SPACE
{
    ATLAS,
    PROJECTED,
    NULLSPACE
};

int main(int argc, char **argv)
{
    int c;
    opterr = 0;

    const char *plannerName = "RRTConnect";
    const char *problem = "sphere";
    const char *space = "projected";
    char *output = nullptr;

    double artificalSleep = 0.0;
    double planningTime = 5.0;
    bool tb = true;
    bool printSpace = false;
    unsigned int runs = 100;

    unsigned int links = 5;
    unsigned int chains = 2;

    while ((c = getopt(argc, argv, "yg:c:r:p:s:w:ot:n:i:ax:f:")) != -1)
    {
        switch (c)
        {
            case 'f':
                output = optarg;
                break;
            case 'r':
                runs = atoi(optarg);
                break;
            case 'y':
                printSpace = true;
                break;

            case 'c':
                problem = optarg;
                break;

            case 'g':
                chains = atoi(optarg);
                break;

            case 'a':
                tb = false;
                break;

            case 'p':
                plannerName = optarg;
                break;

            case 's':
                space = optarg;
                break;

            case 'w':
                artificalSleep = atof(optarg);
                break;

            case 't':
                planningTime = atof(optarg);
                break;

            case 'n':
                links = atoi(optarg);
                break;

            default:
                usage(argv[0]);
                break;
        }
    }

    enum SPACE spaceType = PROJECTED;

    if (std::strcmp("atlas", space) == 0)
        spaceType = ATLAS;
    else if (std::strcmp("projected", space) == 0)
        spaceType = PROJECTED;
    else if (std::strcmp("null", space) == 0)
        spaceType = NULLSPACE;
    else
    {
        std::cout << "Invalid constrained state space." << std::endl;
        usage(argv[0]);
    }

    Eigen::VectorXd x, y;
    ompl::base::StateValidityCheckerFn isValid;
    ompl::base::Constraint *constraint = parseProblem(problem, x, y, isValid, artificalSleep, links, chains);

    if (!constraint)
    {
        std::cout << "Invalid problem." << std::endl;
        usage(argv[0]);
    }

    printf("Constrained Planning Benchmarking: \n"
           "  Benchmarking in `%s' state space with `%s' for `%s' problem.\n"
           "  Ambient Dimension: %u   CoDimension: %u\n"
           "  Timeout: %3.2fs   Artifical Delay: %3.2fs\n",
           space, plannerName, problem, constraint->getAmbientDimension(), constraint->getCoDimension(), planningTime,
           artificalSleep);

    ompl::base::ConstrainedStateSpacePtr css;
    ompl::geometric::SimpleSetupPtr ss;
    ompl::base::SpaceInformationPtr si;

    double range = 1;

    std::string newName = "+";
    switch (spaceType)
    {
        case ATLAS:
        {
            ompl::base::AtlasStateSpacePtr atlas(
                new ompl::base::AtlasStateSpace(constraint->getAmbientSpace(), constraint));

            // atlas->setExploration(0.6);
            atlas->setRho(0.5);         // default is 0.1
            atlas->setAlpha(M_PI / 8);  // default is pi/16
            atlas->setEpsilon(0.2);     // default is 0.2
            atlas->setSeparate(tb);

            ss = ompl::geometric::SimpleSetupPtr(new ompl::geometric::SimpleSetup(atlas));
            si = ss->getSpaceInformation();
            si->setValidStateSamplerAllocator(avssa);

            atlas->setSpaceInformation(si);

            // The atlas needs some place to start sampling from. We will make start and goal charts.
            ompl::base::AtlasChart *startChart = atlas->anchorChart(x);
            ompl::base::AtlasChart *goalChart = atlas->anchorChart(y);

            ompl::base::ScopedState<> start(atlas);
            ompl::base::ScopedState<> goal(atlas);
            start->as<ompl::base::AtlasStateSpace::StateType>()->setRealState(x, startChart);
            goal->as<ompl::base::AtlasStateSpace::StateType>()->setRealState(y, goalChart);

            ss->setStartAndGoalStates(start, goal);
            newName += "A";

            css = atlas;
            break;
        }

        case PROJECTED:
        {
            ompl::base::ProjectedStateSpacePtr proj(
                new ompl::base::ProjectedStateSpace(constraint->getAmbientSpace(), constraint));
            ss = ompl::geometric::SimpleSetupPtr(new ompl::geometric::SimpleSetup(proj));
            si = ss->getSpaceInformation();
            si->setValidStateSamplerAllocator(pvssa);

            proj->setSpaceInformation(si);

            // The proj needs some place to start sampling from. We will make start
            // and goal charts.
            ompl::base::ScopedState<> start(proj);
            ompl::base::ScopedState<> goal(proj);
            start->as<ompl::base::ProjectedStateSpace::StateType>()->setRealState(x);
            goal->as<ompl::base::ProjectedStateSpace::StateType>()->setRealState(y);
            ss->setStartAndGoalStates(start, goal);
            newName += "P";

            css = proj;
            break;
        }

        case NULLSPACE:
        {
            ompl::base::NullspaceStateSpacePtr proj(
                new ompl::base::NullspaceStateSpace(constraint->getAmbientSpace(), constraint));

            ss = ompl::geometric::SimpleSetupPtr(new ompl::geometric::SimpleSetup(proj));
            si = ss->getSpaceInformation();
            si->setValidStateSamplerAllocator(pvssa);

            proj->setSpaceInformation(si);

            // The proj needs some place to start sampling from. We will make start
            // and goal charts.
            ompl::base::ScopedState<> start(proj);
            ompl::base::ScopedState<> goal(proj);
            start->as<ompl::base::NullspaceStateSpace::StateType>()->setRealState(x);
            goal->as<ompl::base::NullspaceStateSpace::StateType>()->setRealState(y);
            ss->setStartAndGoalStates(start, goal);
            newName += "N";

            css = proj;
            break;
        }
    }

    ss->setStateValidityChecker(isValid);

    // Choose the planner.
    ompl::base::PlannerPtr planner(parsePlanner(plannerName, si, range));
    if (!planner)
    {
        std::cout << "Invalid planner." << std::endl;
        usage(argv[0]);
    }

    planner->setName(planner->getName() + newName);
    ss->setPlanner(planner);

    css->registerProjection("sphere", ompl::base::ProjectionEvaluatorPtr(new SphereProjection(css)));
    css->registerProjection("chain", ompl::base::ProjectionEvaluatorPtr(new ChainProjection(css, 3, links)));
    css->registerProjection("stewart", ompl::base::ProjectionEvaluatorPtr(new StewartProjection(css, links, chains)));

    // Bounds
    double bound = 20;
    if (strcmp(problem, "chain") == 0)
        bound = links;

    try
    {
        if (strcmp(plannerName, "KPIECE1") == 0)
            planner->as<ompl::geometric::KPIECE1>()->setProjectionEvaluator(problem);
        else if (strcmp(plannerName, "BKPIECE1") == 0)
            planner->as<ompl::geometric::BKPIECE1>()->setProjectionEvaluator(problem);
        else if (strcmp(plannerName, "LBKPIECE1") == 0)
            planner->as<ompl::geometric::LBKPIECE1>()->setProjectionEvaluator(problem);
        else if (strcmp(plannerName, "ProjEST") == 0)
            planner->as<ompl::geometric::ProjEST>()->setProjectionEvaluator(problem);
        else if (strcmp(plannerName, "PDST") == 0)
            planner->as<ompl::geometric::PDST>()->setProjectionEvaluator(problem);
        else if (strcmp(plannerName, "SBL") == 0)
            planner->as<ompl::geometric::SBL>()->setProjectionEvaluator(problem);
        else if (strcmp(plannerName, "STRIDE") == 0)
            planner->as<ompl::geometric::STRIDE>()->setProjectionEvaluator(problem);
    }
    catch (std::exception &e)
    {
    }

    ompl::base::RealVectorBounds bounds(css->getAmbientDimension());
    bounds.setLow(-bound);
    bounds.setHigh(bound);

    css->as<ompl::base::RealVectorStateSpace>()->setBounds(bounds);

    ss->setup();

    if (printSpace)
        ss->print(std::cout);

    ompl::tools::Benchmark bench(*ss, problem);

    bench.addExperimentParameter("ambient_dimension", "INTEGER", std::to_string(css->getAmbientDimension()));
    bench.addExperimentParameter("manifold_dimension", "INTEGER", std::to_string(css->getManifoldDimension()));
    bench.addExperimentParameter("co_dimension", "INTEGER", std::to_string(constraint->getCoDimension()));
    bench.addExperimentParameter("collision_check_time", "REAL", std::to_string(artificalSleep));

    if (strcmp(problem, "chain") == 0)
    {
        bench.addExperimentParameter("links", "INTEGER", std::to_string(links));
    }
    else if (strcmp(problem, "stewart") == 0)
    {
        bench.addExperimentParameter("links", "INTEGER", std::to_string(links));
        bench.addExperimentParameter("chains", "INTEGER", std::to_string(chains));
    }

    const ompl::tools::Benchmark::Request request(planningTime, memory_limit, runs, update_interval, progress,
                                                  save_output, use_threads, simplify);

    bench.addPlanner(planner);

    bench.setPreRunEvent([&](const ompl::base::PlannerPtr &planner) {
        static unsigned int run = 1;
        std::cout << planner->getName() << " run " << run++ << "\n";

        if (spaceType == ATLAS)
            planner->getSpaceInformation()->getStateSpace()->as<ompl::base::AtlasStateSpace>()->clear();
        else
            planner->getSpaceInformation()->getStateSpace()->as<ompl::base::ConstrainedStateSpace>()->clear();

        planner->clear();
    });

    bench.benchmark(request);

    if (output == nullptr)
    {
        std::string file = planner->getName() + "_on_" + std::string(problem) + ".log";
        bench.saveResultsToFile(file.c_str());
    }
    else
        bench.saveResultsToFile(output);
}
