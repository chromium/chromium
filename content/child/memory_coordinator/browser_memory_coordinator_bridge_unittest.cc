// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/memory_coordinator/browser_memory_coordinator_bridge.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/hash/hash.h"
#include "base/memory_coordinator/mock_memory_consumer.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "content/child/memory_coordinator/child_memory_coordinator.h"
#include "content/common/memory_coordinator/memory_consumer_registry.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"

namespace content {

namespace {

using ::testing::Mock;
using ::testing::Test;

class DummyChildMemoryConsumerRegistryHost
    : public mojom::ChildMemoryConsumerRegistryHost {
 public:
  explicit DummyChildMemoryConsumerRegistryHost(
      mojo::PendingReceiver<mojom::ChildMemoryConsumerRegistryHost> receiver)
      : receiver_(this, std::move(receiver)) {}

  // mojom::ChildMemoryConsumerRegistryHost:
  void BindCoordinator(
      mojo::PendingRemote<mojom::ChildMemoryCoordinator> coordinator) override {
    coordinator_.Bind(std::move(coordinator));
  }

  void Register(std::vector<mojom::MemoryConsumerRegistrationPtr> registrations)
      override {
    ++register_count_;
    for (const auto& registration : registrations) {
      auto [_, inserted] = registered_ids_.insert(registration->consumer_id);
      CHECK(inserted);
    }
  }

  void Unregister(const uint32_t consumer_id) override {
    size_t removed = registered_ids_.erase(consumer_id);
    CHECK_EQ(removed, 1u);
  }

  mojom::ChildMemoryCoordinator* coordinator() { return coordinator_.get(); }

  bool IsRegistered(const uint32_t consumer_id) const {
    return registered_ids_.find(consumer_id) != registered_ids_.end();
  }

  // Number of Register() calls received.
  int register_count() const { return register_count_; }

 private:
  mojo::Receiver<mojom::ChildMemoryConsumerRegistryHost> receiver_;

  mojo::Remote<mojom::ChildMemoryCoordinator> coordinator_;
  absl::flat_hash_set<uint32_t> registered_ids_;
  int register_count_ = 0;
};

constexpr base::MemoryConsumerTraits kTestTraits1(
    base::MemoryConsumerTraits::EstimatedMemoryUsage::kSmall,
    base::MemoryConsumerTraits::ReleaseMemoryCost::kRequiresTraversal,
    base::MemoryConsumerTraits::InformationRetention::kLossless,
    base::MemoryConsumerTraits::ExecutionType::kSynchronous);

}  // namespace

class BrowserMemoryCoordinatorBridgeTest : public Test {
 protected:
  BrowserMemoryCoordinatorBridgeTest() = default;

  ChildMemoryCoordinator& coordinator() { return coordinator_; }
  MemoryConsumerRegistry& registry() { return coordinator_.registry(); }

  std::unique_ptr<DummyChildMemoryConsumerRegistryHost> CreateRegistryHost() {
    return std::make_unique<DummyChildMemoryConsumerRegistryHost>(
        ChildMemoryCoordinator::BindAndPassReceiver());
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  ChildMemoryCoordinator coordinator_;
};

// Tests that a consumer is correctly registered with the browser process
// when added after the bind to the browser registry.
TEST_F(BrowserMemoryCoordinatorBridgeTest, BindBrowser_Initial) {
  auto registry_host = CreateRegistryHost();

  base::MockMemoryConsumer consumer;
  const std::string kConsumerName = "consumer";
  const uint32_t kConsumerId = base::PersistentHash(kConsumerName);

  // Add the consumer.
  registry().AddMemoryConsumer(kConsumerName, kTestTraits1, &consumer);
  ASSERT_EQ(registry().size(), 1u);

  // Wait for the Register call.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return registry_host->IsRegistered(kConsumerId); }));

  // Remove the consumer.
  registry().RemoveMemoryConsumer(kConsumerName, &consumer);
  ASSERT_EQ(registry().size(), 0u);

  // Wait for the Unregister call.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return !registry_host->IsRegistered(kConsumerId); }));
}

// Tests that a consumer added before the bind to the browser registry is
// correctly registered upon binding.
TEST_F(BrowserMemoryCoordinatorBridgeTest,
       BindBrowser_AfterRegisteredConsumer) {
  base::MockMemoryConsumer consumer;
  const std::string kConsumerName = "consumer";
  const uint32_t kConsumerId = base::PersistentHash(kConsumerName);

  // Add the consumer.
  registry().AddMemoryConsumer(kConsumerName, kTestTraits1, &consumer);
  ASSERT_EQ(registry().size(), 1u);

  auto registry_host = CreateRegistryHost();

  // Wait for the Register call.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return registry_host->IsRegistered(kConsumerId); }));

  // Remove the consumer.
  registry().RemoveMemoryConsumer(kConsumerName, &consumer);
  ASSERT_EQ(registry().size(), 0u);

  // Wait for the Unregister call.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return !registry_host->IsRegistered(kConsumerId); }));
}

// Tests that consumers that registered before the host connected are sent to
// the browser in a single batched Register() call.

TEST_F(BrowserMemoryCoordinatorBridgeTest, BindBrowser_BatchesEarlyConsumers) {
  base::MockMemoryConsumer consumer_a;
  base::MockMemoryConsumer consumer_b;
  base::MockMemoryConsumer consumer_c;
  const std::string kNameA = "consumer_a";
  const std::string kNameB = "consumer_b";
  const std::string kNameC = "consumer_c";

  // Register three consumers before the host connects to the browser.
  registry().AddMemoryConsumer(kNameA, kTestTraits1, &consumer_a);
  registry().AddMemoryConsumer(kNameB, kTestTraits1, &consumer_b);
  registry().AddMemoryConsumer(kNameC, kTestTraits1, &consumer_c);
  ASSERT_EQ(registry().size(), 3u);

  auto registry_host = CreateRegistryHost();

  // Wait until all three have reached the browser.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return registry_host->IsRegistered(base::PersistentHash(kNameA)) &&
           registry_host->IsRegistered(base::PersistentHash(kNameB)) &&
           registry_host->IsRegistered(base::PersistentHash(kNameC));
  }));

  // They arrived as a single batched Register().
  EXPECT_EQ(registry_host->register_count(), 1);

  // Cleanup.
  registry().RemoveMemoryConsumer(kNameA, &consumer_a);
  registry().RemoveMemoryConsumer(kNameB, &consumer_b);
  registry().RemoveMemoryConsumer(kNameC, &consumer_c);
}

// Tests that multiple consumers added while connected, within the same task,
// are coalesced into a single batched Register() call.
TEST_F(BrowserMemoryCoordinatorBridgeTest, CoalescesPostConnectBurst) {
  base::HistogramTester histograms;
  auto registry_host = CreateRegistryHost();

  base::MockMemoryConsumer consumer_a;
  base::MockMemoryConsumer consumer_b;
  base::MockMemoryConsumer consumer_c;
  const std::string kNameA = "consumer_a";
  const std::string kNameB = "consumer_b";
  const std::string kNameC = "consumer_c";

  // Add three consumers in one task, while already connected.
  registry().AddMemoryConsumer(kNameA, kTestTraits1, &consumer_a);
  registry().AddMemoryConsumer(kNameB, kTestTraits1, &consumer_b);
  registry().AddMemoryConsumer(kNameC, kTestTraits1, &consumer_c);

  ASSERT_TRUE(base::test::RunUntil([&]() {
    return registry_host->IsRegistered(base::PersistentHash(kNameA)) &&
           registry_host->IsRegistered(base::PersistentHash(kNameB)) &&
           registry_host->IsRegistered(base::PersistentHash(kNameC));
  }));

  // The burst coalesced into one Register() and the batch size (3) was
  // recorded.
  EXPECT_EQ(registry_host->register_count(), 1);

  histograms.ExpectUniqueSample("Memory.Coordinator.RegistrationBatchSize", 3,
                                1);

  // Cleanup.
  registry().RemoveMemoryConsumer(kNameA, &consumer_a);
  registry().RemoveMemoryConsumer(kNameB, &consumer_b);
  registry().RemoveMemoryConsumer(kNameC, &consumer_c);
}

// Tests that browser notifications are correctly routed through the bridge to
// the consumer.
TEST_F(BrowserMemoryCoordinatorBridgeTest, BrowserNotification) {
  auto registry_host = CreateRegistryHost();

  base::MockMemoryConsumer consumer;
  const std::string kConsumerName = "consumer";
  const uint32_t kConsumerId = base::PersistentHash(kConsumerName);

  // Add the consumer.
  registry().AddMemoryConsumer(kConsumerName, kTestTraits1, &consumer);

  // Wait for the Register call.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return registry_host->IsRegistered(kConsumerId); }));

  // Registry host notifies through the coordinator.
  base::test::TestFuture<void> release_memory_future;
  EXPECT_CALL(consumer, OnReleaseMemory()).WillOnce([&]() {
    release_memory_future.SetValue();
  });
  registry_host->coordinator()->UpdateConsumers({{kConsumerId, 100, true}});

  // Wait for the Mojo call to reach the child and trigger the consumer.
  EXPECT_TRUE(release_memory_future.Wait());

  // Cleanup.
  registry().RemoveMemoryConsumer(kConsumerName, &consumer);
}

}  // namespace content
