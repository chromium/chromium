// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/scheduler_loop_quarantine_task_observer.h"

#include <memory>

#include "base/allocator/partition_alloc_features.h"
#include "base/strings/escape.h"
#include "base/strings/string_util.h"
#include "base/task/current_thread.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "content/common/features.h"
#include "content/public/test/browser_task_environment.h"
#include "partition_alloc/buildflags.h"
#include "partition_alloc/extended_api.h"
#include "partition_alloc/partition_alloc_for_testing.h"
#include "partition_alloc/partition_root.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {
// We currently only test the browser UI thread quarantine, to minimize the
// impact if params get changed we only do the bar minimum config the test
// needs.
constexpr char kQuarantineConfigJson[] = R"(
  {
    "browser": {
      "main": {
        "enable-quarantine":true,
        "enable-zapping":true,
        "branch-capacity-in-bytes":524288
      }
    }
  })";
// The finch jsons requires there be no spaces or new lines for some reason
// (encoded or otherwise).
std::string GetQuarantineConfigJson() {
  std::string config_json;
  CHECK(base::RemoveChars(kQuarantineConfigJson, " \n", &config_json));
  return base::EscapeQueryParamValue(config_json,
                                     /*use_plus=*/false);
}
}  // namespace

class SchedulerLoopQuarantineTaskObserverTest : public ::testing::Test {
 protected:
  SchedulerLoopQuarantineTaskObserverTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
    feature_list_.InitWithFeaturesAndParameters(
        {{base::features::kPartitionAllocSchedulerLoopQuarantine,
          std::map<std::string, std::string> {
            { base::features::kPartitionAllocSchedulerLoopQuarantineConfig.name,
              GetQuarantineConfigJson() }
          }},
         { base::features::kPartitionAllocWithAdvancedChecks,
           {} }},
        // Disable preloading `kPrewarm` because it causes the browser to load a
        // webpage before each test starts (during SetUp) which breaks our
        // tests.
        {});
#else
    feature_list_.InitWithFeatures(
        {},
        // Disable preloading `kPrewarm` because it causes the browser to load a
        // webpage before each test starts (during SetUp) which breaks our
        // tests.
        {});
#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
    // Ensure even when USE_PARTITION_ALLOC_AS_MALLOC is false we use the
    // function and that the string parses properly.
    EXPECT_TRUE(!GetQuarantineConfigJson().empty());
  }
  BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(SchedulerLoopQuarantineTaskObserverTest, QuarantinePausesBetweenTasks) {
#if !PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  // SchedulerLoopQuarantine requires USE_PARTITION_ALLOC_AS_MALLOC.
  GTEST_SKIP();
#else
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
      ScopedSchedulerLoopQuarantineBranchAccessorForTesting branch(&root);

  EXPECT_EQ(0, branch.PausedCount());

  // This is the observer under test.
  auto slq_task_observer =
      std::make_unique<SchedulerLoopQuarantineTaskObserver>();
  base::CurrentThread::Get().AddTaskObserver(slq_task_observer.get());
  // We always install the observer so that we can test the feature regardless
  // of if the observer feature is installed or not, but if it is installed the
  // TaskEnvironment will have one additional paused count (functionally the
  // same behaviour though).
  const int kExpectedPausedCount =
      base::FeatureList::IsEnabled(
          features::
              kPartitionAllocSchedulerLoopQuarantineTaskObserverForBrowserUIThread)
          ? 2
          : 1;

  // Initially the branch isn't paused and no tasks are pending, this is just
  // ensuring consistent starting state.
  EXPECT_EQ(0u, task_environment_.GetPendingMainThreadTaskCount());
  EXPECT_EQ(0, branch.PausedCount());

  // We post a task (which doesn't run yet), which will check that the branch
  // isn't paused while executing that task.
  {
    base::RunLoop loop;
    task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE, base::BindLambdaForTesting([&]() {
          EXPECT_EQ(0, branch.PausedCount());
          loop.Quit();
        }));
    // Ensure the task is posted, but we haven't paused the branch yet.
    EXPECT_EQ(0, branch.PausedCount());
    EXPECT_EQ(1u, task_environment_.GetPendingMainThreadTaskCount());
    // During this test the PausedCount should decrease and the expectation
    // above will be tested.
    loop.Run();
    // After the task the PausedCount should be incremented again, since we want
    // to exclude in-between tasks.
    EXPECT_EQ(0u, task_environment_.GetPendingMainThreadTaskCount());
    EXPECT_EQ(kExpectedPausedCount, branch.PausedCount());
  }

  // Now we check again since the first time it was zero before the task.
  {
    base::RunLoop loop;
    task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE, base::BindLambdaForTesting([&]() {
          EXPECT_EQ(0, branch.PausedCount());
          loop.Quit();
        }));

    EXPECT_EQ(kExpectedPausedCount, branch.PausedCount());
    EXPECT_EQ(1u, task_environment_.GetPendingMainThreadTaskCount());
    // Again during this test the PausedCount should decrease.
    loop.Run();
    // After the task the PausedCount should be incremented again.
    EXPECT_EQ(kExpectedPausedCount, branch.PausedCount());
    EXPECT_EQ(0u, task_environment_.GetPendingMainThreadTaskCount());
  }
  // Clean up the observer to prevent dangling references.
  base::CurrentThread::Get().RemoveTaskObserver(slq_task_observer.get());
#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
}

}  // namespace content
