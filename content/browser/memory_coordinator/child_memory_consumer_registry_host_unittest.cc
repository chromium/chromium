// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/memory_coordinator/child_memory_consumer_registry_host.h"

#include <memory>
#include <string>
#include <utility>

#include "base/memory_coordinator/memory_consumer.h"
#include "base/memory_coordinator/memory_consumer_registry.h"
#include "base/memory_coordinator/traits.h"
#include "base/test/task_environment.h"
#include "content/public/common/child_process_id.h"
#include "content/public/common/process_type.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

class MockDelegate : public ChildMemoryConsumerRegistryHost::Delegate {
 public:
  MOCK_METHOD(void,
              AddMemoryConsumerFromChildProcess,
              (std::string_view consumer_id,
               base::MemoryConsumerTraits traits,
               ProcessType process_type,
               ChildProcessId child_process_id,
               base::MemoryConsumer* consumer),
              (override));
  MOCK_METHOD(void,
              RemoveMemoryConsumerFromChildProcess,
              (std::string_view consumer_id,
               ChildProcessId child_process_id,
               base::MemoryConsumer* consumer),
              (override));
};

class MockChildMemoryConsumer : public mojom::ChildMemoryConsumer {
 public:
  MOCK_METHOD(void, NotifyReleaseMemory, (), (override));
  MOCK_METHOD(void, NotifyUpdateMemoryLimit, (int percentage), (override));
};

// A helper to expose protected methods of MemoryConsumer.
class TestMemoryConsumerRegistry : public base::MemoryConsumerRegistry {
 public:
  TestMemoryConsumerRegistry() = default;
  ~TestMemoryConsumerRegistry() override { NotifyDestruction(); }

  using base::MemoryConsumerRegistry::CreateRegisteredMemoryConsumer;

 private:
  void OnMemoryConsumerAdded(std::string_view consumer_id,
                             base::MemoryConsumerTraits traits,
                             base::RegisteredMemoryConsumer consumer) override {
  }
  void OnMemoryConsumerRemoved(
      std::string_view consumer_id,
      base::RegisteredMemoryConsumer consumer) override {}
};

}  // namespace

class ChildMemoryConsumerRegistryHostTest : public testing::Test {
 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  MockDelegate delegate_;
  ChildMemoryConsumerRegistryHost host_{delegate_};
  TestMemoryConsumerRegistry registry_helper_;
};

TEST_F(ChildMemoryConsumerRegistryHostTest, RegisterAndUnregister) {
  mojo::Remote<mojom::ChildMemoryConsumerRegistryHost> remote_host;
  host_.Bind(PROCESS_TYPE_RENDERER, ChildProcessId(1),
             remote_host.BindNewPipeAndPassReceiver());

  MockChildMemoryConsumer mock_consumer;
  mojo::Receiver<mojom::ChildMemoryConsumer> consumer_receiver(&mock_consumer);

  base::MemoryConsumer* host_side_consumer = nullptr;
  EXPECT_CALL(delegate_, AddMemoryConsumerFromChildProcess(
                             "consumer", testing::_, PROCESS_TYPE_RENDERER,
                             ChildProcessId(1), testing::_))
      .WillOnce(testing::SaveArg<4>(&host_side_consumer));

  remote_host->Register("consumer", {},
                        consumer_receiver.BindNewPipeAndPassRemote());
  remote_host.FlushForTesting();

  ASSERT_TRUE(host_side_consumer);

  EXPECT_CALL(delegate_,
              RemoveMemoryConsumerFromChildProcess(
                  "consumer", ChildProcessId(1), host_side_consumer));

  // Disconnect the consumer to trigger unregistration.
  consumer_receiver.reset();
  remote_host.FlushForTesting();
}

TEST_F(ChildMemoryConsumerRegistryHostTest, NotifyReleaseMemory) {
  mojo::Remote<mojom::ChildMemoryConsumerRegistryHost> remote_host;
  host_.Bind(PROCESS_TYPE_RENDERER, ChildProcessId(1),
             remote_host.BindNewPipeAndPassReceiver());

  MockChildMemoryConsumer mock_consumer;
  mojo::Receiver<mojom::ChildMemoryConsumer> consumer_receiver(&mock_consumer);

  base::MemoryConsumer* host_side_consumer = nullptr;
  EXPECT_CALL(delegate_,
              AddMemoryConsumerFromChildProcess(
                  testing::_, testing::_, testing::_, testing::_, testing::_))
      .WillOnce(testing::SaveArg<4>(&host_side_consumer));

  remote_host->Register("consumer", {},
                        consumer_receiver.BindNewPipeAndPassRemote());
  remote_host.FlushForTesting();

  ASSERT_TRUE(host_side_consumer);

  EXPECT_CALL(mock_consumer, NotifyReleaseMemory());
  registry_helper_.CreateRegisteredMemoryConsumer(host_side_consumer)
      .ReleaseMemory();
  consumer_receiver.FlushForTesting();

  EXPECT_CALL(delegate_, RemoveMemoryConsumerFromChildProcess(
                             testing::_, testing::_, host_side_consumer));
}

}  // namespace content
