// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/memory_coordinator/last_resort_gc_policy.h"

#include "base/memory_coordinator/mock_memory_consumer.h"
#include "base/memory_coordinator/traits.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "content/child/memory_coordinator/child_memory_coordinator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

using ::testing::InSequence;
using ::testing::Mock;
using ::testing::Test;

namespace {

constexpr base::MemoryConsumerTraits kTraitsWithReleaseGC(
    base::MemoryConsumerTraits::EstimatedMemoryUsage::kSmall,
    base::MemoryConsumerTraits::ReleaseMemoryCost::kRequiresTraversal,
    base::MemoryConsumerTraits::InformationRetention::kLossy,
    base::MemoryConsumerTraits::ExecutionType::kSynchronous,
    base::MemoryConsumerTraits::ReleaseGCReferences::kYes);

constexpr base::MemoryConsumerTraits kTraitsWithoutReleaseGC(
    base::MemoryConsumerTraits::EstimatedMemoryUsage::kSmall,
    base::MemoryConsumerTraits::ReleaseMemoryCost::kRequiresTraversal,
    base::MemoryConsumerTraits::InformationRetention::kLossy,
    base::MemoryConsumerTraits::ExecutionType::kSynchronous,
    base::MemoryConsumerTraits::ReleaseGCReferences::kNo);

}  // namespace

class LastResortGCPolicyTest : public Test {
 protected:
  LastResortGCPolicyTest() = default;
  ~LastResortGCPolicyTest() override = default;

  void FastForwardBy(base::TimeDelta delta) {
    task_environment_.FastForwardBy(delta);
  }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(LastResortGCPolicyTest, Enabled_NoTrait) {
  ChildMemoryCoordinator coordinator;
  LastResortGCPolicy policy{coordinator};

  // Create the consumer with the `ReleaseGCReferences::kNo` trait.
  base::RegisteredMockMemoryConsumer consumer("Consumer",
                                              kTraitsWithoutReleaseGC);

  // Does not get notified.
  EXPECT_CALL(consumer, OnUpdateMemoryLimit()).Times(0);
  EXPECT_CALL(consumer, OnReleaseMemory()).Times(0);

  policy.OnV8HeapLastResortGC();
}

TEST_F(LastResortGCPolicyTest, Enabled_NoTimer) {
  ChildMemoryCoordinator coordinator;
  LastResortGCPolicy policy{coordinator};

  // Create the consumer with the `ReleaseGCReferences::kYes` trait.
  base::RegisteredMockMemoryConsumer consumer("Consumer", kTraitsWithReleaseGC);

  InSequence s;

  // In order, the memory limit is reduced to zero, then a memory release is
  // requested, and finally the memory limit is restored immediately because
  // there is no delay.
  EXPECT_CALL(consumer, OnUpdateMemoryLimit()).WillOnce([&]() {
    EXPECT_EQ(consumer.memory_limit(), 0);
  });
  EXPECT_CALL(consumer, OnReleaseMemory());
  EXPECT_CALL(consumer, OnUpdateMemoryLimit()).WillOnce([&]() {
    EXPECT_EQ(consumer.memory_limit(), 100);
  });

  policy.OnV8HeapLastResortGC();
}

TEST_F(LastResortGCPolicyTest, Enabled_Timer) {
  static constexpr int kTestRestoreLimitSeconds = 120;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kMemoryCoordinatorLastResortGC,
      {{"restore_limit_seconds",
        base::NumberToString(kTestRestoreLimitSeconds)}});

  ChildMemoryCoordinator coordinator;
  LastResortGCPolicy policy{coordinator};

  // Create the consumer with the `ReleaseGCReferences::kYes` trait.
  base::RegisteredMockMemoryConsumer consumer("Consumer", kTraitsWithReleaseGC);

  InSequence s;

  // In order, the memory limit is reduced to zero, then a memory release is
  // requested. After the correct delay, the memory limit is restored
  // immediately because there is no delay.
  EXPECT_CALL(consumer, OnUpdateMemoryLimit()).WillOnce([&]() {
    EXPECT_EQ(consumer.memory_limit(), 0);
  });
  EXPECT_CALL(consumer, OnReleaseMemory());

  policy.OnV8HeapLastResortGC();
  Mock::VerifyAndClearExpectations(&consumer);

  EXPECT_CALL(consumer, OnUpdateMemoryLimit()).Times(0);
  FastForwardBy(base::Seconds(kTestRestoreLimitSeconds - 1));
  Mock::VerifyAndClearExpectations(&consumer);

  EXPECT_CALL(consumer, OnUpdateMemoryLimit()).WillOnce([&]() {
    EXPECT_EQ(consumer.memory_limit(), 100);
  });
  FastForwardBy(base::Seconds(1));
}

TEST_F(LastResortGCPolicyTest, Persistence) {
  static constexpr int kTestRestoreLimitSeconds = 120;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kMemoryCoordinatorLastResortGC,
      {{"restore_limit_seconds",
        base::NumberToString(kTestRestoreLimitSeconds)}});

  ChildMemoryCoordinator coordinator;
  LastResortGCPolicy policy{coordinator};

  policy.OnV8HeapLastResortGC();

  // Create the consumer AFTER the last resort GC event.
  base::MockMemoryConsumer consumer;

  // It should inherit the limit that was set (0% limit) upon registration
  // without OnUpdateMemoryLimit notification.
  EXPECT_CALL(consumer, OnUpdateMemoryLimit()).Times(0);

  base::MemoryConsumerRegistration registration(
      "Consumer", kTraitsWithReleaseGC, &consumer);
  EXPECT_EQ(consumer.memory_limit(), 0);
}

}  // namespace content
