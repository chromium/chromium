// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/controller/crash_memory_metrics_reporter_impl.h"

#include <algorithm>
#include <thread>

#include "base/atomicops.h"
#include "base/byte_size.h"
#include "base/containers/span.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/oom_intervention/oom_intervention_types.h"
#include "third_party/blink/public/web/web_memory_features.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace blink {

class CrashMemoryMetricsReporterImplTest : public testing::Test {
 protected:
  explicit CrashMemoryMetricsReporterImplTest(
      bool enable_visibility_gate = true) {
#if BUILDFLAG(IS_ANDROID)
    scoped_feature_list_.InitWithFeatureState(
        features::kGateMemoryMetricsOnVisibility, enable_visibility_gate);
#endif
  }

  void SetUp() override {
    CrashMemoryMetricsReporterImpl::Instance().ResetForTesting();
    shared_memory_region_ =
        base::UnsafeSharedMemoryRegion::Create(sizeof(OomInterventionMetrics));
    ASSERT_TRUE(shared_memory_region_.IsValid());
    shared_memory_mapping_ = shared_memory_region_.Map();
    ASSERT_TRUE(shared_memory_mapping_.IsValid());

    CrashMemoryMetricsReporterImpl::Instance().SetSharedMemory(
        shared_memory_region_.Duplicate());
  }

  void TearDown() override {
    CrashMemoryMetricsReporterImpl::Instance().ResetForTesting();
  }

  OomInterventionMetrics GetMetricsFromSharedMemory() const {
    OomInterventionMetrics metrics;
    base::subtle::RelaxedAtomicWriteMemcpy(
        base::byte_span_from_ref(metrics),
        shared_memory_mapping_.GetMemoryAsSpan<uint8_t>());
    return metrics;
  }

  void ClearSharedMemory() {
    std::ranges::fill(shared_memory_mapping_.GetMemoryAsSpan<uint8_t>(), 0);
  }

  test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::UnsafeSharedMemoryRegion shared_memory_region_;
  base::WritableSharedMemoryMapping shared_memory_mapping_;

 private:
#if BUILDFLAG(IS_ANDROID)
  base::test::ScopedFeatureList scoped_feature_list_;
#endif
};

namespace {

TEST_F(CrashMemoryMetricsReporterImplTest, OnOOMCallbackOnMainThread) {
  ASSERT_TRUE(IsMainThread());
  EXPECT_EQ(0u, GetMetricsFromSharedMemory().allocation_failed);

  CrashMemoryMetricsReporterImpl::OnOOMCallback();

  EXPECT_EQ(1u, GetMetricsFromSharedMemory().allocation_failed);
}

TEST_F(CrashMemoryMetricsReporterImplTest, OnOOMCallbackOnWorkerThread) {
  ASSERT_TRUE(IsMainThread());
  EXPECT_EQ(0u, GetMetricsFromSharedMemory().allocation_failed);

  std::thread worker_thread([]() {
    EXPECT_FALSE(IsMainThread());
    CrashMemoryMetricsReporterImpl::OnOOMCallback();
  });
  worker_thread.join();

  EXPECT_EQ(1u, GetMetricsFromSharedMemory().allocation_failed);
}

#if BUILDFLAG(IS_ANDROID)
TEST_F(CrashMemoryMetricsReporterImplTest, DoesNotSampleWhenHidden) {
  ASSERT_TRUE(IsMainThread());
  EXPECT_EQ(base::ByteSize(0),
            GetMetricsFromSharedMemory().current_available_memory);

  test::RunDelayedTasks(base::Seconds(1));

  EXPECT_EQ(base::ByteSize(0),
            GetMetricsFromSharedMemory().current_available_memory);

  CrashMemoryMetricsReporterImpl::OnOOMCallback();
  EXPECT_EQ(1u, GetMetricsFromSharedMemory().allocation_failed);
}

TEST_F(CrashMemoryMetricsReporterImplTest,
       SamplesWhenVisibleAndStopsWhenHidden) {
  ASSERT_TRUE(IsMainThread());
  EXPECT_EQ(base::ByteSize(0),
            GetMetricsFromSharedMemory().current_available_memory);

  // Becoming visible should immediately sample memory state without waiting
  // for the 1s timer tick.
  WebMemoryFeatures::SetRendererVisible(true);
  EXPECT_GT(GetMetricsFromSharedMemory().current_available_memory,
            base::ByteSize(0));

  // Clear shared memory and advance 1s to verify the 1 Hz timer is running.
  // Note: CrashMemoryMetricsReporterImpl's timer runs on NonWakingTaskRunner(),
  // so test::RunDelayedTasks is needed to schedule a waking task at +1s (which
  // MOCK_TIME fast-forwards to immediately) so SequenceManager flushes ripe
  // non-waking tasks.
  ClearSharedMemory();
  EXPECT_EQ(base::ByteSize(0),
            GetMetricsFromSharedMemory().current_available_memory);
  test::RunDelayedTasks(base::Seconds(1));
  EXPECT_GT(GetMetricsFromSharedMemory().current_available_memory,
            base::ByteSize(0));

  // Becoming hidden should stop the 1 Hz timer.
  WebMemoryFeatures::SetRendererVisible(false);
  ClearSharedMemory();
  test::RunDelayedTasks(base::Seconds(1));
  EXPECT_EQ(base::ByteSize(0),
            GetMetricsFromSharedMemory().current_available_memory);
}

TEST_F(CrashMemoryMetricsReporterImplTest, VisibleBeforeSetSharedMemory) {
  ASSERT_TRUE(IsMainThread());
  CrashMemoryMetricsReporterImpl::Instance().ResetForTesting();
  ClearSharedMemory();

  // Becoming visible before SetSharedMemory is called should not crash or write
  // to shared memory yet, and once SetSharedMemory is called it should sample
  // immediately and start the 1 Hz timer.
  WebMemoryFeatures::SetRendererVisible(true);
  EXPECT_EQ(base::ByteSize(0),
            GetMetricsFromSharedMemory().current_available_memory);

  CrashMemoryMetricsReporterImpl::Instance().SetSharedMemory(
      shared_memory_region_.Duplicate());
  EXPECT_GT(GetMetricsFromSharedMemory().current_available_memory,
            base::ByteSize(0));

  ClearSharedMemory();
  test::RunDelayedTasks(base::Seconds(1));
  EXPECT_GT(GetMetricsFromSharedMemory().current_available_memory,
            base::ByteSize(0));
}

class CrashMemoryMetricsReporterImplDisabledFeatureTest
    : public CrashMemoryMetricsReporterImplTest {
 public:
  CrashMemoryMetricsReporterImplDisabledFeatureTest()
      : CrashMemoryMetricsReporterImplTest(
            /*enable_visibility_gate=*/false) {}
};

TEST_F(CrashMemoryMetricsReporterImplDisabledFeatureTest,
       SamplesWhenHiddenIfFeatureDisabled) {
  ASSERT_TRUE(IsMainThread());
  EXPECT_EQ(base::ByteSize(0),
            GetMetricsFromSharedMemory().current_available_memory);

  test::RunDelayedTasks(base::Seconds(1));
  EXPECT_GT(GetMetricsFromSharedMemory().current_available_memory,
            base::ByteSize(0));
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace
}  // namespace blink
