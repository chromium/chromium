// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/scheduler/browser_ui_thread_scheduler.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "content/common/features.h"
#include "content/public/browser/browser_thread.h"
#include "partition_alloc/buildflags.h"
#include "partition_alloc/extended_api.h"
#include "partition_alloc/partition_alloc_for_testing.h"
#include "partition_alloc/scheduler_loop_quarantine_support.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

using StrictMockTask =
    testing::StrictMock<base::MockCallback<base::RepeatingCallback<void()>>>;

base::OnceClosure RunOnDestruction(base::OnceClosure task) {
  return base::BindOnce(
      [](std::unique_ptr<base::ScopedClosureRunner>) {},
      std::make_unique<base::ScopedClosureRunner>(std::move(task)));
}

base::OnceClosure PostOnDestruction(
    scoped_refptr<base::SingleThreadTaskRunner> task_queue,
    base::OnceClosure task) {
  return RunOnDestruction(base::BindOnce(
      [](base::OnceClosure task,
         scoped_refptr<base::SingleThreadTaskRunner> task_queue) {
        task_queue->PostTask(FROM_HERE, std::move(task));
      },
      std::move(task), task_queue));
}

TEST(BrowserUIThreadSchedulerTest, DestructorPostChainDuringShutdown) {
  auto browser_ui_thread_scheduler_ =
      std::make_unique<BrowserUIThreadScheduler>();
  browser_ui_thread_scheduler_->GetHandle()->OnStartupComplete();
  auto task_queue =
      browser_ui_thread_scheduler_->GetHandle()->GetBrowserTaskRunner(
          BrowserUIThreadScheduler::QueueType::kDefault);

  bool run = false;
  task_queue->PostTask(
      FROM_HERE,
      PostOnDestruction(
          task_queue,
          PostOnDestruction(task_queue,
                            RunOnDestruction(base::BindOnce(
                                [](bool* run) { *run = true; }, &run)))));

  EXPECT_FALSE(run);
  browser_ui_thread_scheduler_.reset();

  EXPECT_TRUE(run);
}

TEST(BrowserUIThreadSchedulerTest,
     PrioritizeMainFrameNavigationNetworkResponse) {
  auto scheduler = BrowserUIThreadScheduler::CreateForTesting();
  scheduler->GetHandle()->OnStartupComplete();

  auto default_tq = scheduler->GetHandle()->GetBrowserTaskRunner(
      BrowserUIThreadScheduler::QueueType::kDefault);
  auto nav_tq = scheduler->GetHandle()->GetBrowserTaskRunner(
      BrowserUIThreadScheduler::QueueType::kMainFrameNavigationNetworkResponse);

  base::RunLoop run_loop;
  std::vector<int> order;
  default_tq->PostTask(FROM_HERE, base::BindLambdaForTesting([&]() {
                         order.push_back(1);
                         if (order.size() == 2u) {
                           run_loop.Quit();
                         }
                       }));
  nav_tq->PostTask(FROM_HERE, base::BindLambdaForTesting([&]() {
                     order.push_back(2);
                     if (order.size() == 2u) {
                       run_loop.Quit();
                     }
                   }));

  run_loop.Run();

  ASSERT_EQ(order.size(), 2u);
  EXPECT_EQ(order[0], 2);
  EXPECT_EQ(order[1], 1);
}

TEST(BrowserUIThreadSchedulerTest, PreemptionByUserInputUnderContention) {
  // 1. Baseline: QueueType::kNavigationNetworkResponse (Priority 2).
  // When posted before UserInput tasks (Priority 1) while the UI thread is
  // busy, the subsequent UserInput tasks preempt it and execute first.
  {
    auto scheduler = BrowserUIThreadScheduler::CreateForTesting();
    scheduler->GetHandle()->OnStartupComplete();

    auto default_tq = scheduler->GetHandle()->GetBrowserTaskRunner(
        BrowserUIThreadScheduler::QueueType::kDefault);
    auto user_input_tq = scheduler->GetHandle()->GetBrowserTaskRunner(
        BrowserUIThreadScheduler::QueueType::kUserInput);
    auto nav_tq = scheduler->GetHandle()->GetBrowserTaskRunner(
        BrowserUIThreadScheduler::QueueType::kNavigationNetworkResponse);

    base::RunLoop run_loop;
    std::vector<std::string> order;

    default_tq->PostTask(
        FROM_HERE, base::BindLambdaForTesting([&]() {
          nav_tq->PostTask(FROM_HERE, base::BindLambdaForTesting([&]() {
                             order.push_back("NavigationNetworkResponse");
                             if (order.size() == 3u) {
                               run_loop.Quit();
                             }
                           }));
          user_input_tq->PostTask(FROM_HERE, base::BindLambdaForTesting([&]() {
                                    order.push_back("UserInput1");
                                    if (order.size() == 3u) {
                                      run_loop.Quit();
                                    }
                                  }));
          user_input_tq->PostTask(FROM_HERE, base::BindLambdaForTesting([&]() {
                                    order.push_back("UserInput2");
                                    if (order.size() == 3u) {
                                      run_loop.Quit();
                                    }
                                  }));
        }));

    run_loop.Run();

    ASSERT_EQ(order.size(), 3u);
    EXPECT_EQ(order[0], "UserInput1");
    EXPECT_EQ(order[1], "UserInput2");
    EXPECT_EQ(order[2], "NavigationNetworkResponse");
  }

  // 2. Treatment: QueueType::kMainFrameNavigationNetworkResponse (Priority 1).
  // When posted before UserInput tasks (Priority 1) while the UI thread is
  // busy, it is in the same highest-priority tier and is NOT preempted by later
  // UserInput tasks.
  {
    auto scheduler = BrowserUIThreadScheduler::CreateForTesting();
    scheduler->GetHandle()->OnStartupComplete();

    auto default_tq = scheduler->GetHandle()->GetBrowserTaskRunner(
        BrowserUIThreadScheduler::QueueType::kDefault);
    auto user_input_tq = scheduler->GetHandle()->GetBrowserTaskRunner(
        BrowserUIThreadScheduler::QueueType::kUserInput);
    auto nav_tq = scheduler->GetHandle()->GetBrowserTaskRunner(
        BrowserUIThreadScheduler::QueueType::
            kMainFrameNavigationNetworkResponse);

    base::RunLoop run_loop;
    std::vector<std::string> order;

    default_tq->PostTask(
        FROM_HERE, base::BindLambdaForTesting([&]() {
          nav_tq->PostTask(
              FROM_HERE, base::BindLambdaForTesting([&]() {
                order.push_back("MainFrameNavigationNetworkResponse");
                if (order.size() == 3u) {
                  run_loop.Quit();
                }
              }));
          user_input_tq->PostTask(FROM_HERE, base::BindLambdaForTesting([&]() {
                                    order.push_back("UserInput1");
                                    if (order.size() == 3u) {
                                      run_loop.Quit();
                                    }
                                  }));
          user_input_tq->PostTask(FROM_HERE, base::BindLambdaForTesting([&]() {
                                    order.push_back("UserInput2");
                                    if (order.size() == 3u) {
                                      run_loop.Quit();
                                    }
                                  }));
        }));

    run_loop.Run();

    ASSERT_EQ(order.size(), 3u);
    EXPECT_EQ(order[0], "MainFrameNavigationNetworkResponse");
    EXPECT_EQ(order[1], "UserInput1");
    EXPECT_EQ(order[2], "UserInput2");
  }
}

TEST(BrowserUIThreadSchedulerTest,
     MainFramePreemptsSubframeNavigationResponse) {
  // Verifies that when a subframe/hidden response (kNavigationNetworkResponse,
  // Priority 2) is posted first while UI thread is busy, and a primary main
  // frame response (kMainFrameNavigationNetworkResponse, Priority 1) is posted
  // afterwards, the main frame response preempts the subframe response and runs
  // first.
  auto scheduler = BrowserUIThreadScheduler::CreateForTesting();
  scheduler->GetHandle()->OnStartupComplete();

  auto default_tq = scheduler->GetHandle()->GetBrowserTaskRunner(
      BrowserUIThreadScheduler::QueueType::kDefault);
  auto subframe_nav_tq = scheduler->GetHandle()->GetBrowserTaskRunner(
      BrowserUIThreadScheduler::QueueType::kNavigationNetworkResponse);
  auto main_frame_nav_tq = scheduler->GetHandle()->GetBrowserTaskRunner(
      BrowserUIThreadScheduler::QueueType::kMainFrameNavigationNetworkResponse);

  base::RunLoop run_loop;
  std::vector<std::string> order;

  default_tq->PostTask(FROM_HERE, base::BindLambdaForTesting([&]() {
                         subframe_nav_tq->PostTask(
                             FROM_HERE, base::BindLambdaForTesting([&]() {
                               order.push_back("SubframeResponse");
                               if (order.size() == 2u) {
                                 run_loop.Quit();
                               }
                             }));
                         main_frame_nav_tq->PostTask(
                             FROM_HERE, base::BindLambdaForTesting([&]() {
                               order.push_back("MainFrameResponse");
                               if (order.size() == 2u) {
                                 run_loop.Quit();
                               }
                             }));
                       }));

  run_loop.Run();

  ASSERT_EQ(order.size(), 2u);
  // Main frame response (Priority 1) preempts earlier-posted subframe response
  // (Priority 2).
  EXPECT_EQ(order[0], "MainFrameResponse");
  EXPECT_EQ(order[1], "SubframeResponse");
}

TEST(BrowserUIThreadSchedulerTest, ConcurrentMainFrameNavigationsFIFO) {
  auto scheduler = BrowserUIThreadScheduler::CreateForTesting();
  scheduler->GetHandle()->OnStartupComplete();

  auto default_tq = scheduler->GetHandle()->GetBrowserTaskRunner(
      BrowserUIThreadScheduler::QueueType::kDefault);
  auto main_frame_nav_tq = scheduler->GetHandle()->GetBrowserTaskRunner(
      BrowserUIThreadScheduler::QueueType::kMainFrameNavigationNetworkResponse);

  base::RunLoop run_loop;
  std::vector<int> order;

  default_tq->PostTask(FROM_HERE, base::BindLambdaForTesting([&]() {
                         for (int i = 1; i <= 3; ++i) {
                           main_frame_nav_tq->PostTask(
                               FROM_HERE, base::BindLambdaForTesting([&, i]() {
                                 order.push_back(i);
                                 if (order.size() == 3u) {
                                   run_loop.Quit();
                                 }
                               }));
                         }
                       }));

  run_loop.Run();

  ASSERT_EQ(order.size(), 3u);
  EXPECT_EQ(order[0], 1);
  EXPECT_EQ(order[1], 2);
  EXPECT_EQ(order[2], 3);
}

TEST(BrowserUIThreadSchedulerTest,
     MixedContentionUserInputMainFrameAndSubframe) {
  auto scheduler = BrowserUIThreadScheduler::CreateForTesting();
  scheduler->GetHandle()->OnStartupComplete();

  auto default_tq = scheduler->GetHandle()->GetBrowserTaskRunner(
      BrowserUIThreadScheduler::QueueType::kDefault);
  auto subframe_nav_tq = scheduler->GetHandle()->GetBrowserTaskRunner(
      BrowserUIThreadScheduler::QueueType::kNavigationNetworkResponse);
  auto user_input_tq = scheduler->GetHandle()->GetBrowserTaskRunner(
      BrowserUIThreadScheduler::QueueType::kUserInput);
  auto main_frame_nav_tq = scheduler->GetHandle()->GetBrowserTaskRunner(
      BrowserUIThreadScheduler::QueueType::kMainFrameNavigationNetworkResponse);

  base::RunLoop run_loop;
  std::vector<std::string> order;

  default_tq->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        subframe_nav_tq->PostTask(FROM_HERE, base::BindLambdaForTesting([&]() {
                                    order.push_back("SubframeResponse");
                                    if (order.size() == 3u) {
                                      run_loop.Quit();
                                    }
                                  }));
        user_input_tq->PostTask(FROM_HERE, base::BindLambdaForTesting([&]() {
                                  order.push_back("UserInput");
                                  if (order.size() == 3u) {
                                    run_loop.Quit();
                                  }
                                }));
        main_frame_nav_tq->PostTask(FROM_HERE,
                                    base::BindLambdaForTesting([&]() {
                                      order.push_back("MainFrameResponse");
                                      if (order.size() == 3u) {
                                        run_loop.Quit();
                                      }
                                    }));
      }));

  run_loop.Run();

  ASSERT_EQ(order.size(), 3u);
  // UserInput and MainFrameResponse are both Priority 1 (FIFO between them).
  // SubframeResponse is Priority 2 (runs after all Priority 1 tasks).
  EXPECT_EQ(order[0], "UserInput");
  EXPECT_EQ(order[1], "MainFrameResponse");
  EXPECT_EQ(order[2], "SubframeResponse");
}

TEST(BrowserUIThreadSchedulerTest,
     StarvationResistanceUnderContinuousUserInput) {
  auto measure_steps_until_run =
      [](BrowserUIThreadScheduler::QueueType nav_queue_type) -> int {
    auto scheduler = BrowserUIThreadScheduler::CreateForTesting();
    scheduler->GetHandle()->OnStartupComplete();

    auto user_input_tq = scheduler->GetHandle()->GetBrowserTaskRunner(
        BrowserUIThreadScheduler::QueueType::kUserInput);
    auto nav_tq = scheduler->GetHandle()->GetBrowserTaskRunner(nav_queue_type);

    base::RunLoop run_loop;
    int input_task_count = 0;
    int nav_executed_at_step = -1;
    bool nav_completed = false;

    base::RepeatingClosure post_input_task;
    post_input_task = base::BindLambdaForTesting([&]() {
      input_task_count++;
      if (!nav_completed && input_task_count < 20) {
        user_input_tq->PostTask(FROM_HERE, post_input_task);
      }
    });

    user_input_tq->PostTask(FROM_HERE, post_input_task);

    nav_tq->PostTask(FROM_HERE, base::BindLambdaForTesting([&]() {
                       nav_completed = true;
                       nav_executed_at_step = input_task_count;
                       run_loop.Quit();
                     }));

    run_loop.Run();
    return nav_executed_at_step;
  };

  int baseline_step = measure_steps_until_run(
      BrowserUIThreadScheduler::QueueType::kNavigationNetworkResponse);
  int treatment_step = measure_steps_until_run(
      BrowserUIThreadScheduler::QueueType::kMainFrameNavigationNetworkResponse);

  // Treatment runs immediately on step 1 alongside user input.
  EXPECT_EQ(treatment_step, 1);
  // Baseline is starved until the input stream terminates (step 20).
  EXPECT_GE(baseline_step, 20);
}

class BrowserUIThreadSchedulerLoopQuarantineTest : public testing::Test {
 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      features::
          kPartitionAllocSchedulerLoopQuarantineTaskObserverForBrowserUIThread};
};

TEST_F(BrowserUIThreadSchedulerLoopQuarantineTest,
       TestAllocationGetPurgedFromQuarantineAfterTaskCompletion) {
#if PA_BUILDFLAG(MEMORY_TOOL_REPLACES_ALLOCATOR)
  GTEST_SKIP() << "This test does not work with memory tools.";
#elif !PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) || \
    !PA_CONFIG(THREAD_CACHE_SUPPORTED)
  GTEST_SKIP() << "This test requires PA-E and ThreadCache.";
#else
  // Prepare PA root for testing.
  // IMPORTANT: The root (and thus the SchedulerLoopQuarantineBranch) must be
  // declared first in this test before the BrowserUIThreadScheduler so that
  // when it gets destroyed in reverse order the branch is still a valid
  // pointer.
  partition_alloc::PartitionOptions opts;
  opts.scheduler_loop_quarantine_thread_local_config.enable_quarantine = true;
  opts.scheduler_loop_quarantine_thread_local_config.branch_capacity_in_bytes =
      4096;
  partition_alloc::PartitionAllocatorForTesting allocator(opts);
  partition_alloc::PartitionRoot& root = *allocator.root();

  // Disables ThreadCache for the default allocator and enables it for the
  // testing allocator.
  partition_alloc::internal::ThreadCacheProcessScopeForTesting tcache_scope(
      &root);

  partition_alloc::internal::
      ScopedSchedulerLoopQuarantineBranchAccessorForTesting branch_accessor(
          &root);

  std::unique_ptr<BrowserUIThreadScheduler> browser_ui_thread_scheduler =
      BrowserUIThreadScheduler::CreateForTesting();
  browser_ui_thread_scheduler->GetHandle()->OnStartupComplete();

  // Pick up a queue.
  auto task_queue =
      browser_ui_thread_scheduler->GetHandle()->GetBrowserTaskRunner(
          BrowserUIThreadScheduler::QueueType::kUserBlocking);

  void* ptr = root.Alloc(16);

  base::test::TestFuture<void> future;
  task_queue->PostTaskAndReply(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        EXPECT_FALSE(branch_accessor.IsQuarantined(ptr));
        root.Free<
            partition_alloc::internal::FreeFlags::kSchedulerLoopQuarantine>(
            ptr);
        EXPECT_TRUE(branch_accessor.IsQuarantined(ptr));
      }),
      future.GetCallback());
  EXPECT_TRUE(future.Wait());

  // `ptr` must not be in the quarantine as the scheduler finished its loop.
  EXPECT_FALSE(branch_accessor.IsQuarantined(ptr));
#endif
}

}  // namespace

}  // namespace content
