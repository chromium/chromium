// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/memory_coordinator/browser_memory_consumer_registry.h"

#include <string>
#include <vector>

#include "base/memory_coordinator/mock_memory_consumer.h"
#include "base/memory_coordinator/traits.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/task_environment.h"
#include "content/common/memory_coordinator/memory_consumer_group_controller.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::Test;
using ::testing::UnorderedElementsAre;

struct ConsumerEntry {
  std::string consumer_id;
  base::MemoryConsumerTraits traits;
  ProcessType process_type;
  ChildProcessId child_process_id;
  base::RegisteredMemoryConsumer consumer;
};

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

class BrowserMemoryConsumerRegistryTest : public Test,
                                          public MemoryConsumerGroupController {
 protected:
  BrowserMemoryConsumerRegistry& browser_registry() { return registry_.Get(); }

  std::vector<ConsumerEntry>& entries() { return entries_; }

  // MemoryConsumerGroupController:
  void OnConsumerGroupAdded(std::string_view consumer_id,
                            base::MemoryConsumerTraits traits,
                            ProcessType process_type,
                            ChildProcessId child_process_id,
                            base::RegisteredMemoryConsumer consumer) override {
    entries_.push_back({std::string(consumer_id), traits, process_type,
                        child_process_id, consumer});
  }

  void OnConsumerGroupRemoved(std::string_view consumer_id,
                              ChildProcessId child_process_id) override {
    std::erase_if(entries_, [&](const auto& entry) {
      return entry.consumer_id == consumer_id &&
             entry.child_process_id == child_process_id;
    });
  }

 private:
  base::test::TaskEnvironment task_environment_;
  base::ScopedMemoryConsumerRegistry<BrowserMemoryConsumerRegistry> registry_{
      *this};
  std::vector<ConsumerEntry> entries_;
};

TEST_F(BrowserMemoryConsumerRegistryTest, LocalConsumer) {
  base::MockMemoryConsumer consumer;

  // Add the consumer.
  browser_registry().AddMemoryConsumer("consumer", kTestTraits1, &consumer);
  ASSERT_EQ(entries().size(), 1u);

  ConsumerEntry& entry = entries().front();

  // Notify the consumer.
  EXPECT_CALL(consumer, OnReleaseMemory());
  entry.consumer.ReleaseMemory();
  Mock::VerifyAndClearExpectations(&consumer);

  // Remove the consumer.
  browser_registry().RemoveMemoryConsumer("consumer", &consumer);
  EXPECT_EQ(entries().size(), 0u);
}

TEST_F(BrowserMemoryConsumerRegistryTest, RemoteConsumer) {
  static constexpr char kConsumerId[] = "consumer";
  static constexpr base::MemoryConsumerTraits kTraits = kTestTraits1;

  base::MockMemoryConsumer consumer;

  // Add the consumer.
  browser_registry().AddMemoryConsumerFromChildProcess(
      kConsumerId, kTraits, PROCESS_TYPE_RENDERER, ChildProcessId(23),
      &consumer);
  ASSERT_EQ(entries().size(), 1u);

  ConsumerEntry& entry = entries().front();

  // Verify the consumer's properties.
  EXPECT_EQ(entry.child_process_id, ChildProcessId(23));
  EXPECT_EQ(entry.consumer_id, kConsumerId);
  EXPECT_EQ(entry.traits, kTraits);

  // Notify the consumer.
  EXPECT_CALL(consumer, OnReleaseMemory());
  entry.consumer.ReleaseMemory();
  Mock::VerifyAndClearExpectations(&consumer);

  // Remove the consumer.
  browser_registry().RemoveMemoryConsumerFromChildProcess(
      kConsumerId, ChildProcessId(23), &consumer);
  EXPECT_EQ(entries().size(), 0u);
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
  ASSERT_EQ(entries().size(), 3u);

  // Verify the consumers' properties.
  EXPECT_THAT(entries(),
              UnorderedElementsAre(
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

  for (auto& entry : entries()) {
    entry.consumer.ReleaseMemory();
  }
  Mock::VerifyAndClearExpectations(&consumers[0].consumer);
  Mock::VerifyAndClearExpectations(&consumers[1].consumer);
  Mock::VerifyAndClearExpectations(&consumers[2].consumer);

  // Remove consumers.
  for (auto& data : consumers) {
    browser_registry().RemoveMemoryConsumerFromChildProcess(
        data.consumer_id, data.child_process_id, &data.consumer);
  }
  EXPECT_EQ(entries().size(), 0u);
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
  ASSERT_EQ(entries().size(), 3u);

  // Verify the consumers' properties.
  EXPECT_THAT(
      entries(),
      UnorderedElementsAre(
          HasConsumer(ChildProcessId(1), kTestConsumerId, kTestTraits),
          HasConsumer(ChildProcessId(2), kTestConsumerId, kTestTraits),
          HasConsumer(ChildProcessId(3), kTestConsumerId, kTestTraits)));

  // Notify the consumers.
  for (auto& data : consumers) {
    EXPECT_CALL(data.consumer, OnReleaseMemory());
  }

  for (auto& entry : entries()) {
    entry.consumer.ReleaseMemory();
  }
  Mock::VerifyAndClearExpectations(&consumers[0].consumer);
  Mock::VerifyAndClearExpectations(&consumers[1].consumer);
  Mock::VerifyAndClearExpectations(&consumers[2].consumer);

  // Remove consumers.
  for (auto& data : consumers) {
    browser_registry().RemoveMemoryConsumerFromChildProcess(
        kTestConsumerId, data.child_process_id, &data.consumer);
  }
  EXPECT_EQ(entries().size(), 0u);
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
  ASSERT_EQ(entries().size(), 3u);

  // Verify the consumers' properties.
  EXPECT_THAT(entries(),
              UnorderedElementsAre(
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

  for (auto& entry : entries()) {
    entry.consumer.ReleaseMemory();
  }
  for (auto& data : consumers) {
    Mock::VerifyAndClearExpectations(&data.consumer);
  }

  // Remove consumers.
  for (auto& data : consumers) {
    browser_registry().RemoveMemoryConsumerFromChildProcess(
        data.consumer_id, kChildProcessId, &data.consumer);
  }
  EXPECT_EQ(entries().size(), 0u);
}

TEST_F(BrowserMemoryConsumerRegistryTest, NewConsumerGetsCurrentLimit) {
  NiceMock<base::MockMemoryConsumer> consumer1;
  base::MockMemoryConsumer consumer2;
  static constexpr char kConsumerId[] = "consumer";

  // Add the first consumer.
  browser_registry().AddMemoryConsumer(kConsumerId, kTestTraits1, &consumer1);
  ASSERT_EQ(entries().size(), 1u);

  // Update the group's limit to 70%.
  entries().front().consumer.UpdateMemoryLimit(70);
  EXPECT_EQ(consumer1.memory_limit(), 70);

  // Add the second consumer to the same group. It should immediately get 70%.
  EXPECT_CALL(consumer2, OnUpdateMemoryLimit());
  browser_registry().AddMemoryConsumer(kConsumerId, kTestTraits1, &consumer2);
  EXPECT_EQ(consumer2.memory_limit(), 70);

  browser_registry().RemoveMemoryConsumer(kConsumerId, &consumer1);
  browser_registry().RemoveMemoryConsumer(kConsumerId, &consumer2);
}

TEST_F(BrowserMemoryConsumerRegistryTest, TestHelpers) {
  base::MockMemoryConsumer consumer;
  browser_registry().AddMemoryConsumer("consumer", kTestTraits1, &consumer);

  EXPECT_CALL(consumer, OnUpdateMemoryLimit());
  BrowserMemoryConsumerRegistry::NotifyUpdateMemoryLimitForTesting(42);
  EXPECT_EQ(consumer.memory_limit(), 42);
  Mock::VerifyAndClearExpectations(&consumer);

  EXPECT_CALL(consumer, OnReleaseMemory());
  BrowserMemoryConsumerRegistry::NotifyReleaseMemoryForTesting();
  Mock::VerifyAndClearExpectations(&consumer);

  browser_registry().RemoveMemoryConsumer("consumer", &consumer);
}

}  // namespace content
