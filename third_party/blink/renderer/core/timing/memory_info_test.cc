/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/timing/memory_info.h"

#include "base/test/test_mock_time_task_runner.h"
#include "base/time/default_tick_clock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

TEST(MemoryInfo, quantizeMemorySize) {
  test::TaskEnvironment task_environment;
  EXPECT_EQ(10000000u, QuantizeMemorySize(1024));
  EXPECT_EQ(10000000u, QuantizeMemorySize(1024 * 1024));
  EXPECT_EQ(410000000u, QuantizeMemorySize(389472983));
  EXPECT_EQ(39600000u, QuantizeMemorySize(38947298));
  EXPECT_EQ(29400000u, QuantizeMemorySize(28947298));
  EXPECT_EQ(19300000u, QuantizeMemorySize(18947298));
  EXPECT_EQ(14300000u, QuantizeMemorySize(13947298));
  EXPECT_EQ(10000000u, QuantizeMemorySize(3894729));
  EXPECT_EQ(10000000u, QuantizeMemorySize(389472));
  EXPECT_EQ(10000000u, QuantizeMemorySize(38947));
  EXPECT_EQ(10000000u, QuantizeMemorySize(3894));
  EXPECT_EQ(10000000u, QuantizeMemorySize(389));
  EXPECT_EQ(10000000u, QuantizeMemorySize(38));
  EXPECT_EQ(10000000u, QuantizeMemorySize(3));
  EXPECT_EQ(10000000u, QuantizeMemorySize(1));
  EXPECT_EQ(10000000u, QuantizeMemorySize(0));
  // Rounding differences between OS's may affect the precise value of the last
  // bucket.
  EXPECT_LE(3760000000u,
            QuantizeMemorySize(std::numeric_limits<size_t>::max()));
  EXPECT_GT(4000000000u,
            QuantizeMemorySize(std::numeric_limits<size_t>::max()));
}

static constexpr int kModForBucketizationCheck = 100000;

class MemoryInfoTest : public testing::Test {
 protected:
  void CheckValues(MemoryInfo* info, MemoryInfo::Precision precision) {
    // Check that used <= total <= limit.

    // TODO(npm): add a check usedJSHeapSize <= totalJSHeapSize once it always
    // holds. See https://crbug.com/849322
    EXPECT_LE(info->totalJSHeapSize(), info->jsHeapSizeLimit());
    if (precision == MemoryInfo::Precision::kBucketized) {
      // Check that the bucketized values are heavily rounded.
      EXPECT_EQ(0u, info->totalJSHeapSize() % kModForBucketizationCheck);
      EXPECT_EQ(0u, info->usedJSHeapSize() % kModForBucketizationCheck);
      EXPECT_EQ(0u, info->jsHeapSizeLimit() % kModForBucketizationCheck);
    } else {
      // Check that the precise values are not heavily rounded.
      // Note: these checks are potentially flaky but in practice probably never
      // flaky. If this is noticed to be flaky, disable test and assign bug to
      // npm@.
      EXPECT_NE(0u, info->totalJSHeapSize() % kModForBucketizationCheck);
      EXPECT_NE(0u, info->usedJSHeapSize() % kModForBucketizationCheck);
      EXPECT_NE(0u, info->jsHeapSizeLimit() % kModForBucketizationCheck);
    }
  }

  void CheckEqual(MemoryInfo* info, MemoryInfo* info2) {
    EXPECT_EQ(info2->totalJSHeapSize(), info->totalJSHeapSize());
    EXPECT_EQ(info2->usedJSHeapSize(), info->usedJSHeapSize());
    EXPECT_EQ(info2->jsHeapSizeLimit(), info->jsHeapSizeLimit());
  }
  test::TaskEnvironment task_environment_;
};

struct MemoryInfoTestScopedMockTime {
  MemoryInfoTestScopedMockTime(MemoryInfo::Precision precision) {
    MemoryInfo::SetTickClockForTestingForCurrentThread(
        test_task_runner_->GetMockTickClock());
  }

  ~MemoryInfoTestScopedMockTime() {
    // MemoryInfo creates a HeapSizeCache object which lives in the current
    // thread. This means that it will be shared by all the tests when
    // executed sequentially. We must ensure that it ends up in a consistent
    // state after each test execution.
    MemoryInfo::SetTickClockForTestingForCurrentThread(
        base::DefaultTickClock::GetInstance());
  }

  void AdvanceClock(base::TimeDelta delta) {
    test_task_runner_->FastForwardBy(delta);
  }

  scoped_refptr<base::TestMockTimeTaskRunner> test_task_runner_ =
      base::MakeRefCounted<base::TestMockTimeTaskRunner>();
};

TEST_F(MemoryInfoTest, Bucketized) {
  V8TestingScope scope;
  v8::Isolate* isolate = scope.GetIsolate();
  // The vector is used to keep the objects
  // allocated alive even if GC happens. In practice, the objects only get GC'd
  // after we go out of V8TestingScope. But having them in a vector makes it
  // impossible for GC to clear them up unexpectedly early.
  v8::LocalVector<v8::ArrayBuffer> objects(isolate);

  MemoryInfoTestScopedMockTime mock_time(MemoryInfo::Precision::kBucketized);
  MemoryInfo* bucketized_memory =
      MakeGarbageCollected<MemoryInfo>(MemoryInfo::Precision::kBucketized);

  // Check that the values are monotone and rounded.
  CheckValues(bucketized_memory, MemoryInfo::Precision::kBucketized);

  // Advance the clock for a minute. Not enough to make bucketized value
  // recalculate. Also allocate some memory.
  mock_time.AdvanceClock(base::Minutes(1));
  objects.push_back(v8::ArrayBuffer::New(isolate, 100));

  MemoryInfo* bucketized_memory2 =
      MakeGarbageCollected<MemoryInfo>(MemoryInfo::Precision::kBucketized);
  // The old bucketized values must be equal to the new bucketized values.
  CheckEqual(bucketized_memory, bucketized_memory2);

  // TODO(npm): The bucketized MemoryInfo is very hard to change reliably. One
  // option is to do something such as:
  // for (int i = 0; i < kNumArrayBuffersForLargeAlloc; i++)
  //   objects.push_back(v8::ArrayBuffer::New(isolate, 1));
  // Here, kNumArrayBuffersForLargeAlloc should be strictly greater than 200000
  // (test failed on Windows with this value). Creating a single giant
  // ArrayBuffer does not seem to work, so instead a lot of small ArrayBuffers
  // are used. For now we only test that values are still rounded after adding
  // some memory.
  for (int i = 0; i < 10; i++) {
    // Advance the clock for another thirty minutes, enough to make the
    // bucketized value recalculate.
    mock_time.AdvanceClock(base::Minutes(30));
    objects.push_back(v8::ArrayBuffer::New(isolate, 100));
    MemoryInfo* bucketized_memory3 =
        MakeGarbageCollected<MemoryInfo>(MemoryInfo::Precision::kBucketized);
    CheckValues(bucketized_memory3, MemoryInfo::Precision::kBucketized);
    // The limit should remain unchanged.
    EXPECT_EQ(bucketized_memory3->jsHeapSizeLimit(),
              bucketized_memory->jsHeapSizeLimit());
  }
}

TEST_F(MemoryInfoTest, Precise) {
  V8TestingScope scope;
  v8::Isolate* isolate = scope.GetIsolate();
  v8::LocalVector<v8::ArrayBuffer> objects(isolate);

  MemoryInfoTestScopedMockTime mock_time(MemoryInfo::Precision::kPrecise);
  MemoryInfo* precise_memory =
      MakeGarbageCollected<MemoryInfo>(MemoryInfo::Precision::kPrecise);
  // Check that the precise values are monotone and not heavily rounded.
  CheckValues(precise_memory, MemoryInfo::Precision::kPrecise);

  // Advance the clock for a nanosecond, which should not be enough to make the
  // precise value recalculate.
  mock_time.AdvanceClock(base::Nanoseconds(1));
  // Allocate an object in heap and keep it in a vector to make sure that it
  // does not get accidentally GC'd. This single ArrayBuffer should be enough to
  // be noticed by the used heap size in the precise MemoryInfo case.
  objects.push_back(v8::ArrayBuffer::New(isolate, 100));
  MemoryInfo* precise_memory2 =
      MakeGarbageCollected<MemoryInfo>(MemoryInfo::Precision::kPrecise);
  // The old precise values must be equal to the new precise values.
  CheckEqual(precise_memory, precise_memory2);

  for (int i = 0; i < 10; i++) {
    // Advance the clock for another thirty seconds, enough to make the precise
    // values be recalculated. Also allocate another object.
    mock_time.AdvanceClock(base::Seconds(30));
    objects.push_back(v8::ArrayBuffer::New(isolate, 100));

    MemoryInfo* new_precise_memory =
        MakeGarbageCollected<MemoryInfo>(MemoryInfo::Precision::kPrecise);

    CheckValues(new_precise_memory, MemoryInfo::Precision::kPrecise);
    // The old precise used heap size must be different from the new one.
    EXPECT_NE(new_precise_memory->usedJSHeapSize(),
              precise_memory->usedJSHeapSize());
    // The limit should remain unchanged.
    EXPECT_EQ(new_precise_memory->jsHeapSizeLimit(),
              precise_memory->jsHeapSizeLimit());
    // Update |precise_memory| to be the newest MemoryInfo thus far.
    precise_memory = new_precise_memory;
  }
}

TEST_F(MemoryInfoTest, FlagEnabled) {
  ScopedPreciseMemoryInfoForTest precise_memory_info(true);
  V8TestingScope scope;
  v8::Isolate* isolate = scope.GetIsolate();
  v8::LocalVector<v8::ArrayBuffer> objects(isolate);

  // Using MemoryInfo::Precision::Bucketized to ensure that the runtime-enabled
  // flag overrides the Precision passed onto the method.
  MemoryInfo* precise_memory =
      MakeGarbageCollected<MemoryInfo>(MemoryInfo::Precision::kBucketized);
  // Check that the precise values are monotone and not heavily rounded.
  CheckValues(precise_memory, MemoryInfo::Precision::kPrecise);

  // Allocate an object in heap and keep it in a vector to make sure that it
  // does not get accidentally GC'd. This single ArrayBuffer should be enough to
  // be noticed by the used heap size immediately since the
  // PreciseMemoryInfoEnabled flag is on.
  objects.push_back(v8::ArrayBuffer::New(isolate, 100));
  MemoryInfo* precise_memory2 =
      MakeGarbageCollected<MemoryInfo>(MemoryInfo::Precision::kBucketized);
  CheckValues(precise_memory2, MemoryInfo::Precision::kPrecise);
  // The old precise JS heap size value must NOT be equal to the new value.
  EXPECT_NE(precise_memory2->usedJSHeapSize(),
            precise_memory->usedJSHeapSize());
}

TEST_F(MemoryInfoTest, ZeroTime) {
  // In this test, we make sure that even if the current base::TimeTicks() value
  // is very close to 0, we still obtain memory information from the first call
  // to MemoryInfo::Create.
  MemoryInfoTestScopedMockTime mock_time(MemoryInfo::Precision::kPrecise);
  mock_time.AdvanceClock(base::Microseconds(100));
  V8TestingScope scope;
  v8::Isolate* isolate = scope.GetIsolate();
  v8::LocalVector<v8::ArrayBuffer> objects(isolate);
  objects.push_back(v8::ArrayBuffer::New(isolate, 100));

  MemoryInfo* precise_memory =
      MakeGarbageCollected<MemoryInfo>(MemoryInfo::Precision::kPrecise);
  CheckValues(precise_memory, MemoryInfo::Precision::kPrecise);
  EXPECT_LT(0u, precise_memory->usedJSHeapSize());
  EXPECT_LT(0u, precise_memory->totalJSHeapSize());
  EXPECT_LT(0u, precise_memory->jsHeapSizeLimit());
}

}  // namespace blink
