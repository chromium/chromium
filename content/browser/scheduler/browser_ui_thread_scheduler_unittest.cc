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

class BrowserUIThreadSchedulerLoopQuarantineTest : public testing::Test {
 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      features::
          kPartitionAllocSchedulerLoopQuarantineTaskObserverForBrowserUIThread};
};

TEST_F(BrowserUIThreadSchedulerLoopQuarantineTest,
       TestAllocationGetPurgedFromQuarantineAfterTaskCompletion) {
#if defined(MEMORY_TOOL_REPLACES_ALLOCATOR)
  GTEST_SKIP() << "This test does not work with memory tools.";
#elif !PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) || \
    !PA_CONFIG(THREAD_CACHE_SUPPORTED)
  GTEST_SKIP() << "This test requires PA-E and ThreadCache.";
#else
  std::unique_ptr<BrowserUIThreadScheduler> browser_ui_thread_scheduler =
      BrowserUIThreadScheduler::CreateForTesting();
  browser_ui_thread_scheduler->GetHandle()->OnStartupComplete();

  // Pick up a queue.
  auto task_queue =
      browser_ui_thread_scheduler->GetHandle()->GetBrowserTaskRunner(
          BrowserUIThreadScheduler::QueueType::kUserBlocking);

  // Prepare PA root for testing.
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
