// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/heap/heap_stats_collector.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

constexpr size_t kNoMarkedBytes = 0;

}  // namespace

// =============================================================================
// ThreadHeapStatsCollector. ===================================================
// =============================================================================

TEST(ThreadHeapStatsCollectorTest, InitialEmpty) {
  ThreadHeapStatsCollector stats_collector;
  stats_collector.NotifyMarkingStarted(BlinkGC::GCReason::kForcedGCForTesting);
  for (int i = 0; i < ThreadHeapStatsCollector::kNumScopeIds; i++) {
    EXPECT_EQ(base::TimeDelta(), stats_collector.current().scope_data[i]);
  }
  stats_collector.NotifyMarkingCompleted(kNoMarkedBytes);
  stats_collector.NotifySweepingCompleted();
}

TEST(ThreadHeapStatsCollectorTest, IncreaseScopeTime) {
  ThreadHeapStatsCollector stats_collector;
  stats_collector.NotifyMarkingStarted(BlinkGC::GCReason::kForcedGCForTesting);
  stats_collector.IncreaseScopeTime(
      ThreadHeapStatsCollector::kIncrementalMarkingStep,
      base::TimeDelta::FromMilliseconds(1));
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(1),
            stats_collector.current()
                .scope_data[ThreadHeapStatsCollector::kIncrementalMarkingStep]);
  stats_collector.NotifyMarkingCompleted(kNoMarkedBytes);
  stats_collector.NotifySweepingCompleted();
}

TEST(ThreadHeapStatsCollectorTest, StopMovesCurrentToPrevious) {
  ThreadHeapStatsCollector stats_collector;
  stats_collector.NotifyMarkingStarted(BlinkGC::GCReason::kForcedGCForTesting);
  stats_collector.IncreaseScopeTime(
      ThreadHeapStatsCollector::kIncrementalMarkingStep,
      base::TimeDelta::FromMilliseconds(1));
  stats_collector.NotifyMarkingCompleted(kNoMarkedBytes);
  stats_collector.NotifySweepingCompleted();
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(1),
            stats_collector.previous()
                .scope_data[ThreadHeapStatsCollector::kIncrementalMarkingStep]);
}

TEST(ThreadHeapStatsCollectorTest, StopResetsCurrent) {
  ThreadHeapStatsCollector stats_collector;
  stats_collector.NotifyMarkingStarted(BlinkGC::GCReason::kForcedGCForTesting);
  stats_collector.IncreaseScopeTime(
      ThreadHeapStatsCollector::kIncrementalMarkingStep,
      base::TimeDelta::FromMilliseconds(1));
  stats_collector.NotifyMarkingCompleted(kNoMarkedBytes);
  stats_collector.NotifySweepingCompleted();
  EXPECT_EQ(base::TimeDelta(),
            stats_collector.current()
                .scope_data[ThreadHeapStatsCollector::kIncrementalMarkingStep]);
}

TEST(ThreadHeapStatsCollectorTest, StartStop) {
  ThreadHeapStatsCollector stats_collector;
  EXPECT_FALSE(stats_collector.is_started());
  stats_collector.NotifyMarkingStarted(BlinkGC::GCReason::kForcedGCForTesting);
  EXPECT_TRUE(stats_collector.is_started());
  stats_collector.NotifyMarkingCompleted(kNoMarkedBytes);
  stats_collector.NotifySweepingCompleted();
  EXPECT_FALSE(stats_collector.is_started());
}

TEST(ThreadHeapStatsCollectorTest, ScopeToString) {
  EXPECT_STREQ("BlinkGC.IncrementalMarkingStartMarking",
               ThreadHeapStatsCollector::ToString(
                   ThreadHeapStatsCollector::kIncrementalMarkingStartMarking));
}

TEST(ThreadHeapStatsCollectorTest, UpdateReason) {
  ThreadHeapStatsCollector stats_collector;
  stats_collector.NotifyMarkingStarted(BlinkGC::GCReason::kForcedGCForTesting);
  stats_collector.NotifyMarkingCompleted(kNoMarkedBytes);
  stats_collector.UpdateReason(BlinkGC::GCReason::kForcedGCForTesting);
  stats_collector.NotifySweepingCompleted();
  EXPECT_EQ(BlinkGC::GCReason::kForcedGCForTesting,
            stats_collector.previous().reason);
}

TEST(ThreadHeapStatsCollectorTest, InitialEstimatedObjectSize) {
  ThreadHeapStatsCollector stats_collector;
  stats_collector.NotifyMarkingStarted(BlinkGC::GCReason::kForcedGCForTesting);
  EXPECT_EQ(0u, stats_collector.object_size_in_bytes());
  stats_collector.NotifyMarkingCompleted(kNoMarkedBytes);
  stats_collector.NotifySweepingCompleted();
}

TEST(ThreadHeapStatsCollectorTest, EstimatedObjectSizeNoMarkedBytes) {
  ThreadHeapStatsCollector stats_collector;
  stats_collector.NotifyMarkingStarted(BlinkGC::GCReason::kForcedGCForTesting);
  stats_collector.IncreaseAllocatedObjectSizeForTesting(512);
  EXPECT_EQ(512u, stats_collector.object_size_in_bytes());
  stats_collector.NotifyMarkingCompleted(kNoMarkedBytes);
  stats_collector.NotifySweepingCompleted();
}

TEST(ThreadHeapStatsCollectorTest, EstimatedObjectSizeWithMarkedBytes) {
  ThreadHeapStatsCollector stats_collector;
  stats_collector.NotifyMarkingStarted(BlinkGC::GCReason::kForcedGCForTesting);
  stats_collector.NotifyMarkingCompleted(128);
  stats_collector.NotifySweepingCompleted();
  stats_collector.NotifyMarkingStarted(BlinkGC::GCReason::kForcedGCForTesting);
  stats_collector.NotifyMarkingCompleted(kNoMarkedBytes);
  stats_collector.IncreaseAllocatedObjectSizeForTesting(512);
  EXPECT_EQ(640u, stats_collector.object_size_in_bytes());
  stats_collector.NotifySweepingCompleted();
}

TEST(ThreadHeapStatsCollectorTest,
     EstimatedObjectSizeDoNotCountCurrentlyMarkedBytes) {
  ThreadHeapStatsCollector stats_collector;
  stats_collector.NotifyMarkingStarted(BlinkGC::GCReason::kForcedGCForTesting);
  stats_collector.NotifyMarkingCompleted(128);
  stats_collector.NotifySweepingCompleted();
  stats_collector.NotifyMarkingStarted(BlinkGC::GCReason::kForcedGCForTesting);
  stats_collector.NotifyMarkingCompleted(128);
  // Currently marked bytes should not account to the estimated object size.
  stats_collector.IncreaseAllocatedObjectSizeForTesting(512);
  EXPECT_EQ(640u, stats_collector.object_size_in_bytes());
  stats_collector.NotifySweepingCompleted();
}

TEST(ThreadHeapStatsCollectorTest, PreInitializedEstimatedMarkingTime) {
  // Checks that a marking time estimate can be retrieved before the first
  // garbage collection triggers.
  ThreadHeapStatsCollector stats_collector;
  stats_collector.NotifyMarkingStarted(BlinkGC::GCReason::kForcedGCForTesting);
  EXPECT_LT(0u, stats_collector.estimated_marking_time_in_seconds());
  stats_collector.NotifyMarkingCompleted(kNoMarkedBytes);
  stats_collector.NotifySweepingCompleted();
}

TEST(ThreadHeapStatsCollectorTest, EstimatedMarkingTime1) {
  ThreadHeapStatsCollector stats_collector;
  stats_collector.NotifyMarkingStarted(BlinkGC::GCReason::kForcedGCForTesting);
  stats_collector.IncreaseScopeTime(
      ThreadHeapStatsCollector::kAtomicPauseMarkTransitiveClosure,
      base::TimeDelta::FromSeconds(1));
  stats_collector.NotifyMarkingCompleted(1024);
  stats_collector.NotifySweepingCompleted();
  stats_collector.NotifyMarkingStarted(BlinkGC::GCReason::kForcedGCForTesting);
  EXPECT_DOUBLE_EQ(1.0, stats_collector.estimated_marking_time_in_seconds());
  stats_collector.NotifyMarkingCompleted(kNoMarkedBytes);
  stats_collector.NotifySweepingCompleted();
}

TEST(ThreadHeapStatsCollectorTest, EstimatedMarkingTime2) {
  ThreadHeapStatsCollector stats_collector;
  stats_collector.NotifyMarkingStarted(BlinkGC::GCReason::kForcedGCForTesting);
  stats_collector.IncreaseScopeTime(
      ThreadHeapStatsCollector::kAtomicPauseMarkTransitiveClosure,
      base::TimeDelta::FromSeconds(1));
  stats_collector.NotifyMarkingCompleted(1024);
  stats_collector.NotifySweepingCompleted();
  stats_collector.NotifyMarkingStarted(BlinkGC::GCReason::kForcedGCForTesting);
  stats_collector.IncreaseAllocatedObjectSizeForTesting(512);
  EXPECT_DOUBLE_EQ(1.5, stats_collector.estimated_marking_time_in_seconds());
  stats_collector.NotifyMarkingCompleted(kNoMarkedBytes);
  stats_collector.NotifySweepingCompleted();
}

TEST(ThreadHeapStatsCollectorTest, SubMilliSecondMarkingTime) {
  ThreadHeapStatsCollector stats_collector;
  stats_collector.NotifyMarkingStarted(BlinkGC::GCReason::kForcedGCForTesting);
  stats_collector.IncreaseScopeTime(
      ThreadHeapStatsCollector::kIncrementalMarkingStartMarking,
      base::TimeDelta::FromMillisecondsD(.5));
  stats_collector.NotifyMarkingCompleted(kNoMarkedBytes);
  EXPECT_DOUBLE_EQ(0.5,
                   stats_collector.marking_time_so_far().InMillisecondsF());
  stats_collector.NotifySweepingCompleted();
}

TEST(ThreadHeapStatsCollectorTest, AllocatedSpaceInBytesInitialZero) {
  ThreadHeapStatsCollector stats_collector;
  EXPECT_EQ(0u, stats_collector.allocated_space_bytes());
  stats_collector.NotifyMarkingStarted(BlinkGC::GCReason::kForcedGCForTesting);
  EXPECT_EQ(0u, stats_collector.allocated_space_bytes());
  stats_collector.NotifyMarkingCompleted(kNoMarkedBytes);
  EXPECT_EQ(0u, stats_collector.allocated_space_bytes());
  stats_collector.NotifySweepingCompleted();
  EXPECT_EQ(0u, stats_collector.allocated_space_bytes());
}

TEST(ThreadHeapStatsCollectorTest, AllocatedSpaceInBytesIncrease) {
  ThreadHeapStatsCollector stats_collector;
  stats_collector.IncreaseAllocatedSpace(1024);
  EXPECT_EQ(1024u, stats_collector.allocated_space_bytes());
}

TEST(ThreadHeapStatsCollectorTest, AllocatedSpaceInBytesDecrease) {
  ThreadHeapStatsCollector stats_collector;
  stats_collector.IncreaseAllocatedSpace(1024);
  stats_collector.DecreaseAllocatedSpace(1024);
  EXPECT_EQ(0u, stats_collector.allocated_space_bytes());
}

// =============================================================================
// ThreadHeapStatsCollector::Event. ============================================
// =============================================================================

TEST(ThreadHeapStatsCollectorTest, EventPrevGCMarkedObjectSize) {
  ThreadHeapStatsCollector stats_collector;
  stats_collector.NotifyMarkingStarted(BlinkGC::GCReason::kForcedGCForTesting);
  stats_collector.NotifyMarkingCompleted(1024);
  stats_collector.NotifySweepingCompleted();
  EXPECT_EQ(1024u, stats_collector.previous().marked_bytes);
}

TEST(ThreadHeapStatsCollectorTest,
     EventMarkingTimeFromIncrementalStandAloneGC) {
  ThreadHeapStatsCollector stats_collector;
  stats_collector.NotifyMarkingStarted(BlinkGC::GCReason::kForcedGCForTesting);
  stats_collector.IncreaseScopeTime(
      ThreadHeapStatsCollector::kIncrementalMarkingStartMarking,
      base::TimeDelta::FromMilliseconds(7));
  stats_collector.IncreaseScopeTime(
      ThreadHeapStatsCollector::kIncrementalMarkingStep,
      base::TimeDelta::FromMilliseconds(2));
  stats_collector.IncreaseScopeTime(
      ThreadHeapStatsCollector::kAtomicPauseMarkTransitiveClosure,
      base::TimeDelta::FromMilliseconds(4));
  stats_collector.NotifyMarkingCompleted(kNoMarkedBytes);
  stats_collector.NotifySweepingCompleted();
  EXPECT_DOUBLE_EQ(13.0,
                   stats_collector.previous().marking_time().InMillisecondsF());
}

TEST(ThreadHeapStatsCollectorTest, EventMarkingTimeFromIncrementalUnifiedGC) {
  ThreadHeapStatsCollector stats_collector;
  stats_collector.NotifyMarkingStarted(BlinkGC::GCReason::kForcedGCForTesting);
  stats_collector.IncreaseScopeTime(
      ThreadHeapStatsCollector::kIncrementalMarkingStartMarking,
      base::TimeDelta::FromMilliseconds(7));
  stats_collector.IncreaseScopeTime(
      ThreadHeapStatsCollector::kIncrementalMarkingStep,
      base::TimeDelta::FromMilliseconds(2));
  stats_collector.IncreaseScopeTime(
      ThreadHeapStatsCollector::kUnifiedMarkingStep,
      base::TimeDelta::FromMilliseconds(1));
  stats_collector.IncreaseScopeTime(
      ThreadHeapStatsCollector::kAtomicPauseMarkPrologue,
      base::TimeDelta::FromMilliseconds(3));
  stats_collector.IncreaseScopeTime(
      ThreadHeapStatsCollector::kAtomicPauseMarkTransitiveClosure,
      base::TimeDelta::FromMilliseconds(2));
  stats_collector.IncreaseScopeTime(
      ThreadHeapStatsCollector::kAtomicPauseMarkEpilogue,
      base::TimeDelta::FromMilliseconds(1));
  stats_collector.NotifyMarkingCompleted(kNoMarkedBytes);
  stats_collector.NotifySweepingCompleted();
  EXPECT_DOUBLE_EQ(16.0,
                   stats_collector.previous().marking_time().InMillisecondsF());
}

TEST(ThreadHeapStatsCollectorTest, EventMarkingTime) {
  ThreadHeapStatsCollector stats_collector;
  stats_collector.NotifyMarkingStarted(BlinkGC::GCReason::kForcedGCForTesting);
  stats_collector.IncreaseScopeTime(
      ThreadHeapStatsCollector::kIncrementalMarkingStep,
      base::TimeDelta::FromMilliseconds(2));
  stats_collector.IncreaseScopeTime(
      ThreadHeapStatsCollector::kAtomicPauseMarkTransitiveClosure,
      base::TimeDelta::FromMilliseconds(11));
  stats_collector.NotifyMarkingCompleted(kNoMarkedBytes);
  stats_collector.NotifySweepingCompleted();
  EXPECT_DOUBLE_EQ(13.0,
                   stats_collector.previous().marking_time().InMillisecondsF());
}

TEST(ThreadHeapStatsCollectorTest, EventAtomicMarkingTime) {
  ThreadHeapStatsCollector stats_collector;
  stats_collector.NotifyMarkingStarted(BlinkGC::GCReason::kForcedGCForTesting);
  stats_collector.IncreaseScopeTime(
      ThreadHeapStatsCollector::kAtomicPauseMarkPrologue,
      base::TimeDelta::FromMilliseconds(5));
  stats_collector.IncreaseScopeTime(
      ThreadHeapStatsCollector::kAtomicPauseMarkTransitiveClosure,
      base::TimeDelta::FromMilliseconds(3));
  stats_collector.IncreaseScopeTime(
      ThreadHeapStatsCollector::kAtomicPauseMarkEpilogue,
      base::TimeDelta::FromMilliseconds(1));
  stats_collector.NotifyMarkingCompleted(kNoMarkedBytes);
  stats_collector.NotifySweepingCompleted();
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(9),
            stats_collector.previous().atomic_marking_time());
}

TEST(ThreadHeapStatsCollectorTest, EventAtomicPause) {
  ThreadHeapStatsCollector stats_collector;
  stats_collector.NotifyMarkingStarted(BlinkGC::GCReason::kForcedGCForTesting);
  stats_collector.IncreaseScopeTime(
      ThreadHeapStatsCollector::kAtomicPauseMarkTransitiveClosure,
      base::TimeDelta::FromMilliseconds(17));
  stats_collector.NotifyMarkingCompleted(kNoMarkedBytes);
  stats_collector.IncreaseScopeTime(
      ThreadHeapStatsCollector::kAtomicPauseSweepAndCompact,
      base::TimeDelta::FromMilliseconds(15));
  stats_collector.NotifySweepingCompleted();
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(32),
            stats_collector.previous().atomic_pause_time());
}

TEST(ThreadHeapStatsCollectorTest, EventMarkingTimePerByteInS) {
  ThreadHeapStatsCollector stats_collector;
  stats_collector.NotifyMarkingStarted(BlinkGC::GCReason::kForcedGCForTesting);
  stats_collector.IncreaseScopeTime(
      ThreadHeapStatsCollector::kAtomicPauseMarkTransitiveClosure,
      base::TimeDelta::FromSeconds(1));
  stats_collector.NotifyMarkingCompleted(1000);
  stats_collector.NotifySweepingCompleted();
  EXPECT_DOUBLE_EQ(
      .001, stats_collector.previous().marking_time_in_bytes_per_second());
}

TEST(ThreadHeapStatsCollectorTest, EventSweepingTime) {
  ThreadHeapStatsCollector stats_collector;
  stats_collector.NotifyMarkingStarted(BlinkGC::GCReason::kForcedGCForTesting);
  stats_collector.NotifyMarkingCompleted(kNoMarkedBytes);
  stats_collector.IncreaseScopeTime(ThreadHeapStatsCollector::kLazySweepInIdle,
                                    base::TimeDelta::FromMilliseconds(1));
  stats_collector.IncreaseScopeTime(ThreadHeapStatsCollector::kLazySweepInIdle,
                                    base::TimeDelta::FromMilliseconds(2));
  stats_collector.IncreaseScopeTime(ThreadHeapStatsCollector::kLazySweepInIdle,
                                    base::TimeDelta::FromMilliseconds(3));
  stats_collector.IncreaseScopeTime(
      ThreadHeapStatsCollector::kLazySweepOnAllocation,
      base::TimeDelta::FromMilliseconds(4));
  stats_collector.IncreaseScopeTime(ThreadHeapStatsCollector::kCompleteSweep,
                                    base::TimeDelta::FromMilliseconds(5));
  stats_collector.NotifySweepingCompleted();
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(15),
            stats_collector.previous().sweeping_time());
}

TEST(ThreadHeapStatsCollectorTest, EventCompactionFreedBytes) {
  ThreadHeapStatsCollector stats_collector;
  stats_collector.NotifyMarkingStarted(BlinkGC::GCReason::kForcedGCForTesting);
  stats_collector.NotifyMarkingCompleted(kNoMarkedBytes);
  stats_collector.IncreaseCompactionFreedSize(512);
  stats_collector.NotifySweepingCompleted();
  EXPECT_EQ(512u, stats_collector.previous().compaction_freed_bytes);
}

TEST(ThreadHeapStatsCollectorTest, EventCompactionFreedPages) {
  ThreadHeapStatsCollector stats_collector;
  stats_collector.NotifyMarkingStarted(BlinkGC::GCReason::kForcedGCForTesting);
  stats_collector.NotifyMarkingCompleted(kNoMarkedBytes);
  stats_collector.IncreaseCompactionFreedPages(3);
  stats_collector.NotifySweepingCompleted();
  EXPECT_EQ(3u, stats_collector.previous().compaction_freed_pages);
}

TEST(ThreadHeapStatsCollectorTest, EventInitialEstimatedLiveObjectRate) {
  ThreadHeapStatsCollector stats_collector;
  stats_collector.NotifyMarkingStarted(BlinkGC::GCReason::kForcedGCForTesting);
  stats_collector.NotifyMarkingCompleted(128);
  stats_collector.NotifySweepingCompleted();
  EXPECT_DOUBLE_EQ(0.0, stats_collector.previous().live_object_rate);
}

TEST(ThreadHeapStatsCollectorTest,
     EventEstimatedLiveObjectRateSameMarkedBytes) {
  ThreadHeapStatsCollector stats_collector;
  stats_collector.NotifyMarkingStarted(BlinkGC::GCReason::kForcedGCForTesting);
  stats_collector.NotifyMarkingCompleted(128);
  stats_collector.NotifySweepingCompleted();
  stats_collector.NotifyMarkingStarted(BlinkGC::GCReason::kForcedGCForTesting);
  stats_collector.NotifyMarkingCompleted(128);
  stats_collector.NotifySweepingCompleted();
  EXPECT_DOUBLE_EQ(1.0, stats_collector.previous().live_object_rate);
}

TEST(ThreadHeapStatsCollectorTest,
     EventEstimatedLiveObjectRateHalfMarkedBytes) {
  ThreadHeapStatsCollector stats_collector;
  stats_collector.NotifyMarkingStarted(BlinkGC::GCReason::kForcedGCForTesting);
  stats_collector.NotifyMarkingCompleted(256);
  stats_collector.NotifySweepingCompleted();
  stats_collector.NotifyMarkingStarted(BlinkGC::GCReason::kForcedGCForTesting);
  stats_collector.NotifyMarkingCompleted(128);
  stats_collector.NotifySweepingCompleted();
  EXPECT_DOUBLE_EQ(0.5, stats_collector.previous().live_object_rate);
}

TEST(ThreadHeapStatsCollectorTest, EventEstimatedLiveObjectRateNoMarkedBytes) {
  ThreadHeapStatsCollector stats_collector;
  stats_collector.NotifyMarkingStarted(BlinkGC::GCReason::kForcedGCForTesting);
  stats_collector.NotifyMarkingCompleted(256);
  stats_collector.NotifySweepingCompleted();
  stats_collector.NotifyMarkingStarted(BlinkGC::GCReason::kForcedGCForTesting);
  stats_collector.NotifySweepingCompleted();
  EXPECT_DOUBLE_EQ(0.0, stats_collector.previous().live_object_rate);
}

TEST(ThreadHeapStatsCollectorTest,
     EventEstimatedLiveObjectRateWithAllocatedBytes1) {
  ThreadHeapStatsCollector stats_collector;
  stats_collector.NotifyMarkingStarted(BlinkGC::GCReason::kForcedGCForTesting);
  stats_collector.NotifyMarkingCompleted(128);
  stats_collector.NotifySweepingCompleted();
  stats_collector.IncreaseAllocatedObjectSize(128);
  stats_collector.NotifyMarkingStarted(BlinkGC::GCReason::kForcedGCForTesting);
  stats_collector.NotifyMarkingCompleted(128);
  stats_collector.NotifySweepingCompleted();
  EXPECT_DOUBLE_EQ(.5, stats_collector.previous().live_object_rate);
}

TEST(ThreadHeapStatsCollectorTest,
     EventEstimatedLiveObjectRateWithAllocatedBytes2) {
  ThreadHeapStatsCollector stats_collector;
  stats_collector.NotifyMarkingStarted(BlinkGC::GCReason::kForcedGCForTesting);
  stats_collector.NotifyMarkingCompleted(kNoMarkedBytes);
  stats_collector.NotifySweepingCompleted();
  stats_collector.IncreaseAllocatedObjectSize(128);
  stats_collector.NotifyMarkingStarted(BlinkGC::GCReason::kForcedGCForTesting);
  stats_collector.NotifyMarkingCompleted(128);
  stats_collector.NotifySweepingCompleted();
  EXPECT_DOUBLE_EQ(1.0, stats_collector.previous().live_object_rate);
}

TEST(ThreadHeapStatsCollectorTest,
     EventEstimatedLiveObjectRateWithAllocatedBytes3) {
  ThreadHeapStatsCollector stats_collector;
  stats_collector.NotifyMarkingStarted(BlinkGC::GCReason::kForcedGCForTesting);
  stats_collector.NotifyMarkingCompleted(kNoMarkedBytes);
  stats_collector.NotifySweepingCompleted();
  EXPECT_DOUBLE_EQ(0, stats_collector.previous().live_object_rate);
}

TEST(ThreadHeapStatsCollectorTest,
     EventEstimatedLiveObjectRateWithAllocatedBytes4) {
  ThreadHeapStatsCollector stats_collector;
  stats_collector.NotifyMarkingStarted(BlinkGC::GCReason::kForcedGCForTesting);
  stats_collector.NotifyMarkingCompleted(128);
  stats_collector.NotifySweepingCompleted();
  stats_collector.NotifyMarkingStarted(BlinkGC::GCReason::kForcedGCForTesting);
  stats_collector.NotifyMarkingCompleted(kNoMarkedBytes);
  stats_collector.NotifySweepingCompleted();
  EXPECT_DOUBLE_EQ(0, stats_collector.previous().live_object_rate);
}

TEST(ThreadHeapStatsCollectorTest, EventAllocatedSpaceBeforeSweeping1) {
  ThreadHeapStatsCollector stats_collector;
  stats_collector.NotifyMarkingStarted(BlinkGC::GCReason::kForcedGCForTesting);
  stats_collector.IncreaseAllocatedSpace(1024);
  stats_collector.NotifyMarkingCompleted(kNoMarkedBytes);
  stats_collector.IncreaseAllocatedSpace(2048);
  stats_collector.NotifySweepingCompleted();
  EXPECT_EQ(
      1024u,
      stats_collector.previous().allocated_space_in_bytes_before_sweeping);
}

TEST(ThreadHeapStatsCollectorTest, EventAllocatedSpaceBeforeSweeping2) {
  ThreadHeapStatsCollector stats_collector;
  stats_collector.NotifyMarkingStarted(BlinkGC::GCReason::kForcedGCForTesting);
  stats_collector.IncreaseAllocatedSpace(1024);
  stats_collector.NotifyMarkingCompleted(kNoMarkedBytes);
  stats_collector.DecreaseAllocatedSpace(1024);
  stats_collector.NotifySweepingCompleted();
  EXPECT_EQ(
      1024u,
      stats_collector.previous().allocated_space_in_bytes_before_sweeping);
}

// =============================================================================
// ThreadHeapStatsObserver. ====================================================
// =============================================================================

namespace {

class MockThreadHeapStatsObserver : public ThreadHeapStatsObserver {
 public:
  MOCK_METHOD1(IncreaseAllocatedSpace, void(size_t));
  MOCK_METHOD1(DecreaseAllocatedSpace, void(size_t));
  MOCK_METHOD1(ResetAllocatedObjectSize, void(size_t));
  MOCK_METHOD1(IncreaseAllocatedObjectSize, void(size_t));
  MOCK_METHOD1(DecreaseAllocatedObjectSize, void(size_t));
};

void FakeGC(ThreadHeapStatsCollector* stats_collector, size_t marked_bytes) {
  stats_collector->NotifyMarkingStarted(BlinkGC::GCReason::kForcedGCForTesting);
  stats_collector->NotifyMarkingCompleted(marked_bytes);
  stats_collector->NotifySweepingCompleted();
}

}  // namespace

TEST(ThreadHeapStatsCollectorTest, RegisterUnregisterObserver) {
  ThreadHeapStatsCollector stats_collector;
  MockThreadHeapStatsObserver observer;
  stats_collector.RegisterObserver(&observer);
  stats_collector.UnregisterObserver(&observer);
}

TEST(ThreadHeapStatsCollectorTest, ObserveAllocatedSpace) {
  ThreadHeapStatsCollector stats_collector;
  MockThreadHeapStatsObserver observer;
  stats_collector.RegisterObserver(&observer);
  EXPECT_CALL(observer, IncreaseAllocatedSpace(1024));
  stats_collector.IncreaseAllocatedSpace(1024);
  EXPECT_CALL(observer, DecreaseAllocatedSpace(1024));
  stats_collector.DecreaseAllocatedSpace(1024);
  stats_collector.UnregisterObserver(&observer);
}

TEST(ThreadHeapStatsCollectorTest, ObserveResetAllocatedObjectSize) {
  ThreadHeapStatsCollector stats_collector;
  MockThreadHeapStatsObserver observer;
  stats_collector.RegisterObserver(&observer);
  EXPECT_CALL(observer, ResetAllocatedObjectSize(2048));
  FakeGC(&stats_collector, 2048);
  stats_collector.UnregisterObserver(&observer);
}

TEST(ThreadHeapStatsCollectorTest, ObserveAllocatedObjectSize) {
  ThreadHeapStatsCollector stats_collector;
  MockThreadHeapStatsObserver observer;
  stats_collector.RegisterObserver(&observer);
  EXPECT_CALL(observer, IncreaseAllocatedObjectSize(1024));
  stats_collector.IncreaseAllocatedObjectSizeForTesting(1024);
  EXPECT_CALL(observer, DecreaseAllocatedObjectSize(1024));
  stats_collector.DecreaseAllocatedObjectSizeForTesting(1024);
  stats_collector.UnregisterObserver(&observer);
}

namespace {

class ObserverTriggeringGC final : public ThreadHeapStatsObserver {
 public:
  explicit ObserverTriggeringGC(ThreadHeapStatsCollector* stats_collector)
      : stats_collector_(stats_collector) {}

  void IncreaseAllocatedObjectSize(size_t bytes) final {
    increase_call_count++;
    increased_size_bytes_ += bytes;
    if (increase_call_count == 1) {
      FakeGC(stats_collector_, bytes);
    }
  }

  void ResetAllocatedObjectSize(size_t marked) final {
    reset_call_count++;
    marked_bytes_ = marked;
  }

  // Mock out the rest to trigger warnings if used.
  MOCK_METHOD1(IncreaseAllocatedSpace, void(size_t));
  MOCK_METHOD1(DecreaseAllocatedSpace, void(size_t));
  MOCK_METHOD1(DecreaseAllocatedObjectSize, void(size_t));

  size_t marked_bytes() const { return marked_bytes_; }
  size_t increased_size_bytes() const { return increased_size_bytes_; }

  size_t increase_call_count = 0;
  size_t reset_call_count = 0;

 private:
  ThreadHeapStatsCollector* const stats_collector_;
  size_t marked_bytes_ = 0;
  size_t increased_size_bytes_ = 0;
};

}  // namespace

TEST(ThreadHeapStatsCollectorTest, ObserverTriggersGC) {
  ThreadHeapStatsCollector stats_collector;
  ObserverTriggeringGC gc_observer(&stats_collector);
  MockThreadHeapStatsObserver mock_observer;
  // Internal detail: First registered observer is also notified first.
  stats_collector.RegisterObserver(&gc_observer);
  stats_collector.RegisterObserver(&mock_observer);

  // mock_observer is notified after triggering GC. This means that it should
  // see the reset call with the fully marked size (as gc_observer fakes a GC
  // with that size).
  EXPECT_CALL(mock_observer, ResetAllocatedObjectSize(1024));
  // Since the GC clears counters, it should see an increase call with a delta
  // of zero bytes.
  EXPECT_CALL(mock_observer, IncreaseAllocatedObjectSize(0));

  // Trigger scenario.
  stats_collector.IncreaseAllocatedObjectSizeForTesting(1024);

  // gc_observer sees both calls exactly once.
  EXPECT_EQ(1u, gc_observer.increase_call_count);
  EXPECT_EQ(1u, gc_observer.reset_call_count);
  // gc_observer sees the increased bytes and the reset call with the fully
  // marked size.
  EXPECT_EQ(1024u, gc_observer.increased_size_bytes());
  EXPECT_EQ(1024u, gc_observer.marked_bytes());

  stats_collector.UnregisterObserver(&gc_observer);
  stats_collector.UnregisterObserver(&mock_observer);
}

}  // namespace blink
