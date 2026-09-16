// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/controller/crash_memory_metrics_reporter_impl.h"

#include <thread>

#include "base/atomicops.h"
#include "base/containers/span.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/oom_intervention/oom_intervention_types.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace blink {

class CrashMemoryMetricsReporterImplTest : public testing::Test {
 protected:
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

  base::UnsafeSharedMemoryRegion shared_memory_region_;
  base::WritableSharedMemoryMapping shared_memory_mapping_;
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

}  // namespace
}  // namespace blink
