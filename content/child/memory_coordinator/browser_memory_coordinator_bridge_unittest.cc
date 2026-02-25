// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/memory_coordinator/browser_memory_coordinator_bridge.h"

#include <memory>
#include <string>
#include <utility>

#include "base/check_op.h"
#include "base/memory_coordinator/mock_memory_consumer.h"
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

  void Register(const std::string& consumer_id,
                std::optional<base::MemoryConsumerTraits> traits) override {
    auto [_, inserted] = registered_ids_.insert(consumer_id);
    CHECK(inserted);
  }

  void Unregister(const std::string& consumer_id) override {
    size_t removed = registered_ids_.erase(consumer_id);
    CHECK_EQ(removed, 1u);
  }

  mojom::ChildMemoryCoordinator* coordinator() { return coordinator_.get(); }

  bool IsRegistered(const std::string& consumer_id) const {
    return registered_ids_.find(consumer_id) != registered_ids_.end();
  }

 private:
  mojo::Receiver<mojom::ChildMemoryConsumerRegistryHost> receiver_;

  mojo::Remote<mojom::ChildMemoryCoordinator> coordinator_;
  absl::flat_hash_set<std::string> registered_ids_;
};

const std::optional<base::MemoryConsumerTraits> kTestTraits1 = std::nullopt;

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

  // Add the consumer.
  registry().AddMemoryConsumer("consumer", kTestTraits1, &consumer);
  ASSERT_EQ(registry().size(), 1u);

  // Wait for the Register call.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return registry_host->IsRegistered("consumer"); }));

  // Remove the consumer.
  registry().RemoveMemoryConsumer("consumer", &consumer);
  ASSERT_EQ(registry().size(), 0u);

  // Wait for the Unregister call.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return !registry_host->IsRegistered("consumer"); }));
}

// Tests that a consumer added before the bind to the browser registry is
// correctly registered upon binding.
TEST_F(BrowserMemoryCoordinatorBridgeTest,
       BindBrowser_AfterRegisteredConsumer) {
  base::MockMemoryConsumer consumer;

  // Add the consumer.
  registry().AddMemoryConsumer("consumer", kTestTraits1, &consumer);
  ASSERT_EQ(registry().size(), 1u);

  auto registry_host = CreateRegistryHost();

  // Wait for the Register call.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return registry_host->IsRegistered("consumer"); }));

  // Remove the consumer.
  registry().RemoveMemoryConsumer("consumer", &consumer);
  ASSERT_EQ(registry().size(), 0u);

  // Wait for the Unregister call.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return !registry_host->IsRegistered("consumer"); }));
}

// Tests that browser notifications are correctly routed through the bridge to
// the consumer.
TEST_F(BrowserMemoryCoordinatorBridgeTest, BrowserNotification) {
  auto registry_host = CreateRegistryHost();

  base::MockMemoryConsumer consumer;
  registry().AddMemoryConsumer("consumer", kTestTraits1, &consumer);

  // Wait for the Register call.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return registry_host->IsRegistered("consumer"); }));

  // Registry host notifies through the coordinator.
  base::test::TestFuture<void> release_memory_future;
  EXPECT_CALL(consumer, OnReleaseMemory()).WillOnce([&]() {
    release_memory_future.SetValue();
  });
  registry_host->coordinator()->UpdateConsumers({{"consumer", 100, true}});

  // Wait for the Mojo call to reach the child and trigger the consumer.
  EXPECT_TRUE(release_memory_future.Wait());

  // Cleanup.
  registry().RemoveMemoryConsumer("consumer", &consumer);
}

}  // namespace content
