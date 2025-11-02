// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/scheduler/browser_ui_thread_scheduler.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/allocator/partition_alloc_support.h"
#include "base/allocator/scheduler_loop_quarantine_config.h"
#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "content/common/features.h"
#include "content/public/browser/browser_thread.h"
#include "partition_alloc/bucket_lookup.h"
#include "partition_alloc/extended_api.h"
#include "partition_alloc/partition_alloc_for_testing.h"
#include "partition_alloc/scheduler_loop_quarantine_runtime_stats.h"
#include "partition_alloc/scheduler_loop_quarantine_support.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !defined(MEMORY_TOOL_REPLACES_ALLOCATOR) &&    \
    PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) && \
    PA_CONFIG(THREAD_CACHE_SUPPORTED) && !PA_BUILDFLAG(IS_WIN)
namespace partition_alloc::internal {
// We need to redeclare the storage to use this variable.
const PA_COMPONENT_EXPORT(PARTITION_ALLOC) size_t
    SchedulerLoopQuarantineRuntimeStats::kMaxTimesToTrack;
}  // namespace partition_alloc::internal
#endif

namespace content {
namespace {
using ::base::Bucket;
using ::base::BucketsAre;
using ::testing::Eq;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

using RuntimeStats =
    ::partition_alloc::internal::SchedulerLoopQuarantineRuntimeStats;

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

TEST_F(BrowserUIThreadSchedulerLoopQuarantineTest,
       SchedulerLoopQuarantineRuntimeStatsAreReported) {
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
  opts.scheduler_loop_quarantine_thread_local_config.enable_zapping = true;
  opts.scheduler_loop_quarantine_thread_local_config.branch_capacity_in_bytes =
      4096;
  opts.scheduler_loop_quarantine_thread_local_config
      .enable_quarantine_runtime_stats = true;
  partition_alloc::PartitionAllocatorForTesting allocator(opts);
  partition_alloc::PartitionRoot& root = *allocator.root();

  // Disables ThreadCache for the default allocator and enables it for the
  // testing allocator.
  partition_alloc::internal::ThreadCacheProcessScopeForTesting tcache_scope(
      &root);

  partition_alloc::internal::
      ScopedSchedulerLoopQuarantineBranchAccessorForTesting branch_accessor(
          &root);

  std::vector<void*> ptrs;
  task_queue->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        for (int i = 0; i < RuntimeStats::kMaxTimesToTrack + 2; ++i) {
          ptrs.push_back(root.Alloc(70));
          EXPECT_FALSE(branch_accessor.IsQuarantined(ptrs.back()));
        }
      }));

  base::test::TestFuture<void> free_future;
  absl::flat_hash_map<std::string, std::vector<base::Bucket>> samples;
  task_queue->PostTaskAndReply(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        // This resets any allocations that were seen, so that the histograms
        // will be just our allocations.
        root.ReconfigureSchedulerLoopQuarantineForCurrentThread(
            opts.scheduler_loop_quarantine_thread_local_config);
        for (void* ptr : ptrs) {
          EXPECT_FALSE(branch_accessor.IsQuarantined(ptr));
          root.Free<
              partition_alloc::internal::FreeFlags::kSchedulerLoopQuarantine>(
              ptr);
          EXPECT_TRUE(branch_accessor.IsQuarantined(ptr));
        }
        // Report the first round of metrics, by wrapping it around we avoid
        // other allocations and UMA metrics. The interval should be long
        // enough that we don't see any other metrics reported. Choose the
        // timeout duration.
        base::HistogramTester current_samples;
        base::allocator::StartSchedulerLoopPeriodicStatsReporting(
            base::Seconds(300), &root);
        samples = current_samples.GetAllSamplesForPrefix(
            "Memory.Browser.PartitionAlloc.SchedulerLoopQuarantine.Stats.");
      }),
      free_future.GetCallback());
  EXPECT_TRUE(free_future.Wait());

  size_t bucket_index =
      partition_alloc::BucketIndexLookup::GetIndexForDenserBuckets((70));
  auto& stats = root.GetStatsForSchedulerLoopQuarantineForCurrentThread();
  for (size_t i = 0; i < stats.purge_buckets().size(); ++i) {
    if (i != bucket_index + 1) {
      EXPECT_FALSE(stats.purge_buckets()[i].valid())
          << i << " vs " << bucket_index + 1;
    } else {
      EXPECT_TRUE(stats.purge_buckets()[i].valid())
          << i << " vs " << bucket_index + 1;
    }
  }

  // The task should be reposted.
  EXPECT_THAT(
      samples,
      ::testing::IsSupersetOf(
          {Pair("Memory.Browser.PartitionAlloc.SchedulerLoopQuarantine.Stats."
                "CycleCount.0B.512B",
                BucketsAre(Bucket(1, /*count=*/1))),
           Pair("Memory.Browser.PartitionAlloc.SchedulerLoopQuarantine.Stats."
                "PausedCount.0B.512B",
                BucketsAre(Bucket(0, /*count=*/1)))}));
  std::vector<std::string> actual_metrics = {};
  for (auto& [name, buckets] : samples) {
    actual_metrics.push_back(name);
    size_t num_samples = 0;
    for (auto& bucket : buckets) {
      num_samples += bucket.count;
    }
    if (name.find("CycleCount") != std::string::npos ||
        name.find("PausedCount") != std::string::npos) {
      EXPECT_EQ(num_samples, 1u);
    } else {
      // We don't know the actual performance numbers just ensure we got the
      // right number of reports.
      EXPECT_EQ(num_samples, RuntimeStats::kMaxTimesToTrack);
    }
  }
  EXPECT_THAT(actual_metrics,
              UnorderedElementsAre(
                  Eq("Memory.Browser.PartitionAlloc.SchedulerLoopQuarantine."
                     "Stats.TotalTime.0B.512B"),
                  Eq("Memory.Browser.PartitionAlloc.SchedulerLoopQuarantine."
                     "Stats.PurgeTime.0B.512B"),
                  Eq("Memory.Browser.PartitionAlloc.SchedulerLoopQuarantine."
                     "Stats.ZapTime.0B.512B"),
                  Eq("Memory.Browser.PartitionAlloc.SchedulerLoopQuarantine."
                     "Stats.CycleCount.0B.512B"),
                  Eq("Memory.Browser.PartitionAlloc.SchedulerLoopQuarantine."
                     "Stats.PausedCount.0B.512B")));
#endif
}

}  // namespace

}  // namespace content
