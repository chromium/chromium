// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/memory_coordinator/child_memory_consumer_registry.h"

#include <string>
#include <utility>

#include "base/memory_coordinator/mock_memory_consumer.h"
#include "base/memory_coordinator/traits.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

using ConsumerInfo = ChildMemoryConsumerRegistry::ConsumerInfo;

class DummyBrowserMemoryConsumerRegistry
    : public mojom::BrowserMemoryConsumerRegistry {
 public:
  explicit DummyBrowserMemoryConsumerRegistry(
      mojo::PendingReceiver<mojom::BrowserMemoryConsumerRegistry> receiver)
      : receiver_(this, std::move(receiver)) {}

  // mojom::BrowserMemoryConsumerRegistry:
  void RegisterChildMemoryConsumer(
      const std::string& consumer_id,
      base::MemoryConsumerTraits traits,
      mojo::PendingRemote<mojom::ChildMemoryConsumer> remote_consumer)
      override {
    remote_set_.Add(std::move(remote_consumer));
  }

  mojo::PendingRemote<mojom::BrowserMemoryConsumerRegistry>
  BindNewPipeAndPassRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

 private:
  mojo::Receiver<mojom::BrowserMemoryConsumerRegistry> receiver_;

  mojo::RemoteSet<mojom::ChildMemoryConsumer> remote_set_;
};

const base::MemoryConsumerTraits kTestTraits1{};

}  // namespace

class ChildMemoryConsumerRegistryTest : public testing::Test {
 protected:
  ChildMemoryConsumerRegistry* registry() { return &registry_; }

  std::unique_ptr<DummyBrowserMemoryConsumerRegistry> CreateBrowserRegistry() {
    return std::make_unique<DummyBrowserMemoryConsumerRegistry>(
        registry_.BindAndPassReceiverForTesting());
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;

  ChildMemoryConsumerRegistry registry_;
};

TEST_F(ChildMemoryConsumerRegistryTest, LocalConsumer) {
  base::MockMemoryConsumer consumer;

  // Add the consumer.
  registry()->AddMemoryConsumer("consumer", kTestTraits1, &consumer);
  ASSERT_EQ(registry()->size(), 1u);

  ConsumerInfo& consumer_info = *registry()->begin();

  // // Notify the consumer.
  EXPECT_CALL(consumer, OnReleaseMemory());
  consumer_info.consumer.ReleaseMemory();
  testing::Mock::VerifyAndClearExpectations(&consumer);

  // Remove the consumer.
  registry()->RemoveMemoryConsumer("consumer", &consumer);
  ASSERT_EQ(registry()->size(), 0u);
}

TEST_F(ChildMemoryConsumerRegistryTest, Iterator) {
  base::MockMemoryConsumer consumer;

  // Add the consumer.
  registry()->AddMemoryConsumer("consumer", kTestTraits1, &consumer);
  ASSERT_EQ(registry()->size(), 1u);

  // // Notify the consumer.
  EXPECT_CALL(consumer, OnReleaseMemory());

  for (ConsumerInfo& consumer_info : *registry()) {
    consumer_info.consumer.ReleaseMemory();
  }
  testing::Mock::VerifyAndClearExpectations(&consumer);

  // Remove the consumer.
  registry()->RemoveMemoryConsumer("consumer", &consumer);
}

// Same as ChildMemoryConsumerRegistryTest.LocalConsumer, but the consumer is
// added after the bind to the browser registry, and removed while the receiver
// still exists.
TEST_F(ChildMemoryConsumerRegistryTest, BindBrowser_Initial) {
  auto browser_registry = CreateBrowserRegistry();

  base::MockMemoryConsumer consumer;

  // Add the consumer.
  registry()->AddMemoryConsumer("consumer", kTestTraits1, &consumer);
  ASSERT_EQ(registry()->size(), 1u);

  ConsumerInfo& consumer_info = *registry()->begin();

  // // Notify the consumer.
  EXPECT_CALL(consumer, OnReleaseMemory());
  consumer_info.consumer.ReleaseMemory();
  testing::Mock::VerifyAndClearExpectations(&consumer);

  // Remove the consumer.
  registry()->RemoveMemoryConsumer("consumer", &consumer);
  ASSERT_EQ(registry()->size(), 0u);
}

// Same as ChildMemoryConsumerRegistryTest.LocalConsumer, but the consumer is
// added before the bind to the browser registry, but removed after.
TEST_F(ChildMemoryConsumerRegistryTest, BindBrowser_AfterRegisteredConsumer) {
  base::MockMemoryConsumer consumer;

  // Add the consumer.
  registry()->AddMemoryConsumer("consumer", kTestTraits1, &consumer);
  ASSERT_EQ(registry()->size(), 1u);

  ConsumerInfo& consumer_info = *registry()->begin();

  // // Notify the consumer.
  EXPECT_CALL(consumer, OnReleaseMemory());
  consumer_info.consumer.ReleaseMemory();
  testing::Mock::VerifyAndClearExpectations(&consumer);

  auto browser_registry = CreateBrowserRegistry();

  // Remove the consumer.
  registry()->RemoveMemoryConsumer("consumer", &consumer);
  ASSERT_EQ(registry()->size(), 0u);
}

}  // namespace content
