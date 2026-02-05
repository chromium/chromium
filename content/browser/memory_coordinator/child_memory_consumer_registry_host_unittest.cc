// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/memory_coordinator/child_memory_consumer_registry_host.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory_coordinator/memory_consumer.h"
#include "base/memory_coordinator/memory_consumer_registry.h"
#include "base/memory_coordinator/traits.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "content/public/common/child_process_id.h"
#include "content/public/common/process_type.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

using ::testing::_;
using ::testing::SaveArg;
using ::testing::Test;

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

class MockChildMemoryCoordinator : public mojom::ChildMemoryCoordinator {
 public:
  MOCK_METHOD(void,
              NotifyReleaseMemory,
              (const std::string& consumer_id),
              (override));
  MOCK_METHOD(void,
              NotifyUpdateMemoryLimit,
              (const std::string& consumer_id, int percentage),
              (override));
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

class ChildMemoryConsumerRegistryHostTest : public Test {
 protected:
  void BindHost(
      ProcessType process_type,
      ChildProcessId child_process_id,
      mojo::PendingReceiver<mojom::ChildMemoryConsumerRegistryHost> receiver) {
    auto host = std::make_unique<ChildMemoryConsumerRegistryHost>(
        delegate_, process_type, child_process_id);
    auto* host_ptr = host.get();
    mojo::ReceiverId id = hosts_.Add(std::move(host), std::move(receiver));
    host_ptr->SetDisconnectHandler(base::BindOnce(
        [](mojo::UniqueReceiverSet<mojom::ChildMemoryConsumerRegistryHost>*
               hosts,
           mojo::ReceiverId id) { hosts->Remove(id); },
        &hosts_, id));
  }

  base::test::TaskEnvironment task_environment_;
  MockDelegate delegate_;
  mojo::UniqueReceiverSet<mojom::ChildMemoryConsumerRegistryHost> hosts_;
  TestMemoryConsumerRegistry registry_helper_;
};

TEST_F(ChildMemoryConsumerRegistryHostTest, RegisterAndUnregister) {
  mojo::Remote<mojom::ChildMemoryConsumerRegistryHost> remote_host;
  BindHost(PROCESS_TYPE_RENDERER, ChildProcessId(1),
           remote_host.BindNewPipeAndPassReceiver());

  MockChildMemoryCoordinator mock_coordinator;
  mojo::Receiver<mojom::ChildMemoryCoordinator> coordinator_receiver(
      &mock_coordinator);
  remote_host->BindCoordinator(coordinator_receiver.BindNewPipeAndPassRemote());

  base::MemoryConsumer* host_side_consumer = nullptr;
  EXPECT_CALL(delegate_,
              AddMemoryConsumerFromChildProcess(
                  "consumer", _, PROCESS_TYPE_RENDERER, ChildProcessId(1), _))
      .WillOnce(SaveArg<4>(&host_side_consumer));

  remote_host->Register("consumer", {});
  remote_host.FlushForTesting();

  ASSERT_TRUE(host_side_consumer);

  EXPECT_CALL(delegate_,
              RemoveMemoryConsumerFromChildProcess(
                  "consumer", ChildProcessId(1), host_side_consumer));

  remote_host->Unregister("consumer");
  remote_host.FlushForTesting();
}

TEST_F(ChildMemoryConsumerRegistryHostTest, NotifyReleaseMemory) {
  mojo::Remote<mojom::ChildMemoryConsumerRegistryHost> remote_host;
  BindHost(PROCESS_TYPE_RENDERER, ChildProcessId(1),
           remote_host.BindNewPipeAndPassReceiver());

  MockChildMemoryCoordinator mock_coordinator;
  mojo::Receiver<mojom::ChildMemoryCoordinator> coordinator_receiver(
      &mock_coordinator);
  remote_host->BindCoordinator(coordinator_receiver.BindNewPipeAndPassRemote());

  base::MemoryConsumer* host_side_consumer = nullptr;
  EXPECT_CALL(delegate_, AddMemoryConsumerFromChildProcess(_, _, _, _, _))
      .WillOnce(SaveArg<4>(&host_side_consumer));

  remote_host->Register("consumer", {});
  remote_host.FlushForTesting();

  ASSERT_TRUE(host_side_consumer);

  EXPECT_CALL(mock_coordinator, NotifyReleaseMemory("consumer"));
  registry_helper_.CreateRegisteredMemoryConsumer(host_side_consumer)
      .ReleaseMemory();
  coordinator_receiver.FlushForTesting();

  EXPECT_CALL(delegate_,
              RemoveMemoryConsumerFromChildProcess(_, _, host_side_consumer));
}

// Tests that a disconnection with the ChildMemoryCoordinator pipe cleans up the
// data associated with that process.
TEST_F(ChildMemoryConsumerRegistryHostTest, DisconnectCoordinator) {
  mojo::Remote<mojom::ChildMemoryConsumerRegistryHost> remote_host;
  BindHost(PROCESS_TYPE_RENDERER, ChildProcessId(1),
           remote_host.BindNewPipeAndPassReceiver());

  MockChildMemoryCoordinator mock_coordinator;
  mojo::Receiver<mojom::ChildMemoryCoordinator> coordinator_receiver(
      &mock_coordinator);
  remote_host->BindCoordinator(coordinator_receiver.BindNewPipeAndPassRemote());

  EXPECT_CALL(delegate_, AddMemoryConsumerFromChildProcess(_, _, _, _, _));

  remote_host->Register("consumer", {});
  remote_host.FlushForTesting();

  EXPECT_CALL(delegate_,
              RemoveMemoryConsumerFromChildProcess(_, ChildProcessId(1), _))
      .WillOnce(base::test::RunOnceClosure(task_environment_.QuitClosure()));

  coordinator_receiver.reset();
  task_environment_.RunUntilQuit();
}

}  // namespace content
