// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/memory_coordinator/browser_memory_consumer_registry.h"

#include <memory>
#include <utility>

#include "base/memory_coordinator/mock_memory_consumer.h"
#include "base/memory_coordinator/traits.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

using ConsumerInfo = BrowserMemoryConsumerRegistry::ConsumerInfo;

MATCHER_P3(HasConsumer, child_process_id, consumer_id, traits, "") {
  return arg.child_process_id == child_process_id &&
         arg.consumer_id == consumer_id && arg.traits == traits;
}

constexpr base::MemoryConsumerTraits kTestTraits1{
    .estimated_memory_usage =
        base::MemoryConsumerTraits::EstimatedMemoryUsage::kSmall,
    .release_memory_cost = base::MemoryConsumerTraits::ReleaseMemoryCost::
        kFreesPagesWithoutTraversal,
    .execution_type = base::MemoryConsumerTraits::ExecutionType::kSynchronous};
constexpr base::MemoryConsumerTraits kTestTraits2{
    .estimated_memory_usage =
        base::MemoryConsumerTraits::EstimatedMemoryUsage::kMedium,
    .release_memory_cost = base::MemoryConsumerTraits::ReleaseMemoryCost::
        kFreesPagesWithoutTraversal,
    .execution_type = base::MemoryConsumerTraits::ExecutionType::kSynchronous};
constexpr base::MemoryConsumerTraits kTestTraits3{
    .estimated_memory_usage =
        base::MemoryConsumerTraits::EstimatedMemoryUsage::kLarge,
    .release_memory_cost =
        base::MemoryConsumerTraits::ReleaseMemoryCost::kRequiresTraversal,
    .execution_type = base::MemoryConsumerTraits::ExecutionType::kSynchronous};

}  // namespace

class BrowserMemoryConsumerRegistryTest : public testing::Test {
 protected:
  BrowserMemoryConsumerRegistry& browser_registry() { return registry_; }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  BrowserMemoryConsumerRegistry registry_;
};

TEST_F(BrowserMemoryConsumerRegistryTest, LocalConsumer) {
  base::MockMemoryConsumer consumer;

  // Add the consumer.
  browser_registry().AddMemoryConsumer("consumer", kTestTraits1, &consumer);
  ASSERT_EQ(browser_registry().size(), 1u);

  ConsumerInfo& consumer_info = *browser_registry().begin();

  // Notify the consumer.
  EXPECT_CALL(consumer, OnReleaseMemory());
  consumer_info.consumer.ReleaseMemory();
  testing::Mock::VerifyAndClearExpectations(&consumer);

  // Remove the consumer.
  browser_registry().RemoveMemoryConsumer("consumer", &consumer);
  EXPECT_EQ(browser_registry().size(), 0u);
}

TEST_F(BrowserMemoryConsumerRegistryTest, RemoteConsumer) {
  static constexpr char kConsumerId[] = "consumer";
  static constexpr base::MemoryConsumerTraits kTraits = kTestTraits1;

  base::MockMemoryConsumer consumer;

  // Add the consumer.
  browser_registry().AddMemoryConsumerFromChildProcess(
      kConsumerId, kTraits, PROCESS_TYPE_RENDERER, ChildProcessId(23),
      &consumer);
  ASSERT_EQ(browser_registry().size(), 1u);

  ConsumerInfo& consumer_info = *browser_registry().begin();

  // Verify the consumer's properties.
  EXPECT_EQ(consumer_info.child_process_id, ChildProcessId(23));
  EXPECT_EQ(consumer_info.consumer_id, kConsumerId);
  EXPECT_EQ(consumer_info.traits, kTraits);

  // Notify the consumer.
  EXPECT_CALL(consumer, OnReleaseMemory());
  consumer_info.consumer.ReleaseMemory();
  testing::Mock::VerifyAndClearExpectations(&consumer);

  // Remove the consumer.
  browser_registry().RemoveMemoryConsumerFromChildProcess(
      kConsumerId, ChildProcessId(23), &consumer);
  EXPECT_EQ(browser_registry().size(), 0u);
}

std::string CreateConsumerId(int id) {
  return "consumer" + base::NumberToString(id);
}

// Simulate having multiple child processes with a single MemoryConsumer in
// each.
TEST_F(BrowserMemoryConsumerRegistryTest, MultipleChildConsumers) {
  struct ConsumerData {
    base::MockMemoryConsumer consumer;
    std::string consumer_id;
    base::MemoryConsumerTraits traits;
    ChildProcessId child_process_id;
  } consumers[] = {
      {{}, CreateConsumerId(0), kTestTraits1, ChildProcessId(1)},
      {{}, CreateConsumerId(1), kTestTraits2, ChildProcessId(2)},
      {{}, CreateConsumerId(2), kTestTraits3, ChildProcessId(3)},
  };

  // Add consumers.
  for (auto& data : consumers) {
    browser_registry().AddMemoryConsumerFromChildProcess(
        data.consumer_id, data.traits, PROCESS_TYPE_RENDERER,
        data.child_process_id, &data.consumer);
  }
  ASSERT_EQ(browser_registry().size(), 3u);

  // Verify the consumers' properties.
  EXPECT_THAT(browser_registry(),
              testing::UnorderedElementsAre(
                  HasConsumer(ChildProcessId(1), consumers[0].consumer_id,
                              consumers[0].traits),
                  HasConsumer(ChildProcessId(2), consumers[1].consumer_id,
                              consumers[1].traits),
                  HasConsumer(ChildProcessId(3), consumers[2].consumer_id,
                              consumers[2].traits)));

  // Notify the consumers.
  for (auto& data : consumers) {
    EXPECT_CALL(data.consumer, OnReleaseMemory());
  }

  for (auto& consumer_info : browser_registry()) {
    consumer_info.consumer.ReleaseMemory();
  }
  testing::Mock::VerifyAndClearExpectations(&consumers[0].consumer);
  testing::Mock::VerifyAndClearExpectations(&consumers[1].consumer);
  testing::Mock::VerifyAndClearExpectations(&consumers[2].consumer);

  // Remove consumers.
  for (auto& data : consumers) {
    browser_registry().RemoveMemoryConsumerFromChildProcess(
        data.consumer_id, data.child_process_id, &data.consumer);
  }
  EXPECT_EQ(browser_registry().size(), 0u);
}

// Simulate having multiple child processes that each host a consumer with the
// same ID.
TEST_F(BrowserMemoryConsumerRegistryTest, SameConsumerIdDifferentChild) {
  static constexpr char kTestConsumerId[] = "Test consumer id";
  static constexpr base::MemoryConsumerTraits kTestTraits = kTestTraits1;

  struct ConsumerData {
    base::MockMemoryConsumer consumer;
    ChildProcessId child_process_id;
  } consumers[] = {
      {{}, ChildProcessId(1)},
      {{}, ChildProcessId(2)},
      {{}, ChildProcessId(3)},
  };

  // Add consumers.
  for (auto& data : consumers) {
    browser_registry().AddMemoryConsumerFromChildProcess(
        kTestConsumerId, kTestTraits, PROCESS_TYPE_RENDERER,
        data.child_process_id, &data.consumer);
  }
  ASSERT_EQ(browser_registry().size(), 3u);

  // Verify the consumers' properties.
  EXPECT_THAT(
      browser_registry(),
      testing::UnorderedElementsAre(
          HasConsumer(ChildProcessId(1), kTestConsumerId, kTestTraits),
          HasConsumer(ChildProcessId(2), kTestConsumerId, kTestTraits),
          HasConsumer(ChildProcessId(3), kTestConsumerId, kTestTraits)));

  // Notify the consumers.
  for (auto& data : consumers) {
    EXPECT_CALL(data.consumer, OnReleaseMemory());
  }

  for (auto& consumer_info : browser_registry()) {
    consumer_info.consumer.ReleaseMemory();
  }
  testing::Mock::VerifyAndClearExpectations(&consumers[0].consumer);
  testing::Mock::VerifyAndClearExpectations(&consumers[1].consumer);
  testing::Mock::VerifyAndClearExpectations(&consumers[2].consumer);

  // Remove consumers.
  for (auto& data : consumers) {
    browser_registry().RemoveMemoryConsumerFromChildProcess(
        kTestConsumerId, data.child_process_id, &data.consumer);
  }
  EXPECT_EQ(browser_registry().size(), 0u);
}

// Simulate having multiple consumers with different IDs within the same child
// process.
TEST_F(BrowserMemoryConsumerRegistryTest, MultipleConsumersSameChild) {
  const ChildProcessId kChildProcessId(42);

  struct ConsumerData {
    base::MockMemoryConsumer consumer;
    std::string consumer_id;
    base::MemoryConsumerTraits traits;
  } consumers[] = {
      {{}, "consumer1", kTestTraits1},
      {{}, "consumer2", kTestTraits2},
      {{}, "consumer3", kTestTraits3},
  };

  // Add consumers.
  for (auto& data : consumers) {
    browser_registry().AddMemoryConsumerFromChildProcess(
        data.consumer_id, data.traits, PROCESS_TYPE_RENDERER, kChildProcessId,
        &data.consumer);
  }
  ASSERT_EQ(browser_registry().size(), 3u);

  // Verify the consumers' properties.
  EXPECT_THAT(browser_registry(),
              testing::UnorderedElementsAre(
                  HasConsumer(kChildProcessId, consumers[0].consumer_id,
                              consumers[0].traits),
                  HasConsumer(kChildProcessId, consumers[1].consumer_id,
                              consumers[1].traits),
                  HasConsumer(kChildProcessId, consumers[2].consumer_id,
                              consumers[2].traits)));

  // Notify the consumers.
  for (auto& data : consumers) {
    EXPECT_CALL(data.consumer, OnReleaseMemory());
  }

  for (auto& consumer_info : browser_registry()) {
    consumer_info.consumer.ReleaseMemory();
  }
  for (auto& data : consumers) {
    testing::Mock::VerifyAndClearExpectations(&data.consumer);
  }

  // Remove consumers.
  for (auto& data : consumers) {
    browser_registry().RemoveMemoryConsumerFromChildProcess(
        data.consumer_id, kChildProcessId, &data.consumer);
  }
  EXPECT_EQ(browser_registry().size(), 0u);
}

}  // namespace content
