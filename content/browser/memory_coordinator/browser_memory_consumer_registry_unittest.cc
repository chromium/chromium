// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/memory_coordinator/browser_memory_consumer_registry.h"

#include <memory>
#include <utility>

#include "base/barrier_closure.h"
#include "base/memory_coordinator/mock_memory_consumer.h"
#include "base/memory_coordinator/traits.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "content/child/memory_coordinator/child_memory_consumer_registry.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

using ConsumerInfo = BrowserMemoryConsumerRegistry::ConsumerInfo;

MATCHER_P3(HasConsumer, child_process_id, consumer_id, traits, "") {
  return arg.child_process_id == child_process_id &&
         arg.consumer_id == consumer_id && arg.traits == traits;
}

const base::MemoryConsumerTraits kTestTraits1{
    .estimated_memory_usage =
        base::MemoryConsumerTraits::EstimatedMemoryUsage::kSmall,
    .release_memory_cost = base::MemoryConsumerTraits::ReleaseMemoryCost::
        kFreesPagesWithoutTraversal,
    .execution_type = base::MemoryConsumerTraits::ExecutionType::kSynchronous};
const base::MemoryConsumerTraits kTestTraits2{
    .estimated_memory_usage =
        base::MemoryConsumerTraits::EstimatedMemoryUsage::kMedium,
    .release_memory_cost = base::MemoryConsumerTraits::ReleaseMemoryCost::
        kFreesPagesWithoutTraversal,
    .execution_type = base::MemoryConsumerTraits::ExecutionType::kSynchronous};
const base::MemoryConsumerTraits kTestTraits3{
    .estimated_memory_usage =
        base::MemoryConsumerTraits::EstimatedMemoryUsage::kLarge,
    .release_memory_cost =
        base::MemoryConsumerTraits::ReleaseMemoryCost::kRequiresTraversal,
    .execution_type = base::MemoryConsumerTraits::ExecutionType::kSynchronous};

}  // namespace

class BrowserMemoryConsumerRegistryTest : public testing::Test {
 protected:
  BrowserMemoryConsumerRegistry& browser_registry() {
    return browser_registry_;
  }

  std::unique_ptr<ChildMemoryConsumerRegistry> CreateChildRegistry(
      ChildProcessId child_process_id) {
    mojo::PendingRemote<mojom::BrowserMemoryConsumerRegistry> pending_remote;

    // Assign the receiver endpoint to the browser registry.
    browser_registry_.Bind(PROCESS_TYPE_RENDERER, child_process_id,
                           pending_remote.InitWithNewPipeAndPassReceiver());

    // Create a child registry with the remote endpoint.
    auto child_registry = std::make_unique<ChildMemoryConsumerRegistry>(
        std::move(pending_remote));

    return child_registry;
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;

  BrowserMemoryConsumerRegistry browser_registry_;
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
}

// Simulates adding a consumer in a child process, and calling its
// ReleaseMemory() method. We must wait between all steps since everything
// happens asynchronously through a mojo connection.
TEST_F(BrowserMemoryConsumerRegistryTest, RemoteConsumer) {
  auto child_registry = CreateChildRegistry(ChildProcessId(23));

  const std::string KConsumerId("consumer");
  const base::MemoryConsumerTraits kTraits = kTestTraits1;

  base::MockMemoryConsumer consumer;

  // Add the consumer.
  child_registry->AddMemoryConsumer(KConsumerId, kTraits, &consumer);
  EXPECT_TRUE(
      base::test::RunUntil([&]() { return browser_registry().size() == 1u; }));

  ConsumerInfo& consumer_info = *browser_registry().begin();

  // Verify the consumer's properties.
  EXPECT_EQ(consumer_info.child_process_id, ChildProcessId(23));
  EXPECT_EQ(consumer_info.consumer_id, KConsumerId);
  EXPECT_EQ(consumer_info.traits, kTraits);

  // Notify the consumer.
  base::test::TestFuture<void> future;
  EXPECT_CALL(consumer, OnReleaseMemory()).WillOnce([&]() {
    future.SetValue();
  });
  consumer_info.consumer.ReleaseMemory();
  EXPECT_TRUE(future.Wait());

  // Remove the consumer.
  child_registry->RemoveMemoryConsumer(KConsumerId, &consumer);
  EXPECT_TRUE(
      base::test::RunUntil([&]() { return browser_registry().size() == 0u; }));
}

std::string CreateConsumerId(int id) {
  return "consumer" + base::NumberToString(id);
}

// Simulate having multiple child processes with a single MemoryConsumer in
// each.
TEST_F(BrowserMemoryConsumerRegistryTest, MultipleChildRegistries) {
  struct ChildRegistryAndConsumer {
    std::unique_ptr<ChildMemoryConsumerRegistry> child_registry;
    base::MockMemoryConsumer consumer;
    std::string consumer_id;
    base::MemoryConsumerTraits traits;
  } child_registry_and_consumers[] = {
      {CreateChildRegistry(ChildProcessId(0)),
       {},
       CreateConsumerId(0),
       kTestTraits1},
      {CreateChildRegistry(ChildProcessId(1)),
       {},
       CreateConsumerId(1),
       kTestTraits2},
      {CreateChildRegistry(ChildProcessId(2)),
       {},
       CreateConsumerId(2),
       kTestTraits3},
  };

  // Add consumers.
  for (auto& [child_registry, consumer, consumer_id, traits] :
       child_registry_and_consumers) {
    child_registry->AddMemoryConsumer(consumer_id, traits, &consumer);
  }
  EXPECT_TRUE(
      base::test::RunUntil([&]() { return browser_registry().size() == 3u; }));

  // Verify the consumers' properties.
  EXPECT_THAT(browser_registry(),
              testing::UnorderedElementsAre(
                  HasConsumer(ChildProcessId(0),
                              child_registry_and_consumers[0].consumer_id,
                              child_registry_and_consumers[0].traits),
                  HasConsumer(ChildProcessId(1),
                              child_registry_and_consumers[1].consumer_id,
                              child_registry_and_consumers[1].traits),
                  HasConsumer(ChildProcessId(2),
                              child_registry_and_consumers[2].consumer_id,
                              child_registry_and_consumers[2].traits)));

  // Notify the consumers.
  base::test::TestFuture<void> future;
  auto barrier_closure = base::BarrierClosure(3, future.GetCallback());
  for (auto& [child_registry, consumer, consumer_id, traits] :
       child_registry_and_consumers) {
    EXPECT_CALL(consumer, OnReleaseMemory()).WillOnce([&]() {
      barrier_closure.Run();
    });
  }

  for (auto& consumer_info : browser_registry()) {
    consumer_info.consumer.ReleaseMemory();
  }
  EXPECT_TRUE(future.Wait());

  // Remove consumers.
  for (auto& [child_registry, consumer, consumer_id, traits] :
       child_registry_and_consumers) {
    child_registry->RemoveMemoryConsumer(consumer_id, &consumer);
  }
  EXPECT_TRUE(
      base::test::RunUntil([&]() { return browser_registry().size() == 0u; }));
}

// Simulate having one child process with multiple MemoryConsumers.
TEST_F(BrowserMemoryConsumerRegistryTest, MultipleChildConsumersSameRegistry) {
  auto child_registry = CreateChildRegistry(ChildProcessId(23));

  struct ChildConsumers {
    std::string consumer_id;
    base::MemoryConsumerTraits traits;
    base::MockMemoryConsumer consumer;
  } child_consumers[] = {
      {CreateConsumerId(10), kTestTraits1, {}},
      {CreateConsumerId(22), kTestTraits2, {}},
      {CreateConsumerId(44), kTestTraits3, {}},
  };

  // Add consumers.
  for (auto& [consumer_id, traits, consumer] : child_consumers) {
    child_registry->AddMemoryConsumer(consumer_id, traits, &consumer);
  }
  EXPECT_TRUE(
      base::test::RunUntil([&]() { return browser_registry().size() == 3u; }));

  // Verify the consumers' properties.
  EXPECT_THAT(
      browser_registry(),
      testing::UnorderedElementsAre(
          HasConsumer(ChildProcessId(23), CreateConsumerId(10), kTestTraits1),
          HasConsumer(ChildProcessId(23), CreateConsumerId(22), kTestTraits2),
          HasConsumer(ChildProcessId(23), CreateConsumerId(44), kTestTraits3)));

  // Notify the consumers.
  base::test::TestFuture<void> future;
  auto barrier_closure = base::BarrierClosure(3, future.GetCallback());
  for (auto& [consumer_id, traits, consumer] : child_consumers) {
    EXPECT_CALL(consumer, OnReleaseMemory()).WillOnce([&]() {
      barrier_closure.Run();
    });
  }
  for (auto& consumer_info : browser_registry()) {
    consumer_info.consumer.ReleaseMemory();
  }
  EXPECT_TRUE(future.Wait());

  // Remove consumers.
  for (auto& [consumer_id, _, consumer] : child_consumers) {
    child_registry->RemoveMemoryConsumer(consumer_id, &consumer);
  }
  EXPECT_TRUE(
      base::test::RunUntil([&]() { return browser_registry().size() == 0u; }));
}

}  // namespace content
