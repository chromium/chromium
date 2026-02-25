// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/memory_coordinator/child_memory_consumer_registry_host.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/memory_coordinator/memory_consumer.h"
#include "base/memory_coordinator/memory_consumer_registry.h"
#include "base/memory_coordinator/traits.h"
#include "base/test/gmock_callback_support.h"
#include "content/common/buildflags.h"
#include "content/common/memory_coordinator/memory_consumer_group_controller.h"
#include "content/common/memory_coordinator/memory_consumer_group_host.h"
#include "content/public/common/child_process_id.h"
#include "content/public/common/process_type.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

#if BUILDFLAG(ENABLE_MEMORY_COORDINATOR_INTERNALS)
#include "mojo/public/cpp/test_support/fake_message_dispatch_context.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#endif

namespace content {

namespace {

using ::testing::_;
using ::testing::Test;

class MockChildMemoryCoordinator : public mojom::ChildMemoryCoordinator {
 public:
  MOCK_METHOD(void,
              UpdateConsumers,
              (std::vector<MemoryConsumerUpdate> updates),
              (override));
#if BUILDFLAG(ENABLE_MEMORY_COORDINATOR_INTERNALS)
  MOCK_METHOD(
      void,
      EnableDiagnosticsReporting,
      (mojo::PendingRemote<mojom::MemoryCoordinatorDiagnosticsHost> host),
      (override));
#endif
};

class MockMemoryConsumerGroupController : public MemoryConsumerGroupController {
 public:
  MOCK_METHOD(void,
              AddMemoryConsumerGroupHost,
              (ChildProcessId child_process_id, MemoryConsumerGroupHost* host),
              (override));

  MOCK_METHOD(void,
              RemoveMemoryConsumerGroupHost,
              (ChildProcessId child_process_id),
              (override));

  MOCK_METHOD(void,
              OnConsumerGroupAdded,
              (std::string_view consumer_id,
               std::optional<base::MemoryConsumerTraits> traits,
               ProcessType process_type,
               ChildProcessId child_process_id),
              (override));

  MOCK_METHOD(void,
              OnConsumerGroupRemoved,
              (std::string_view consumer_id, ChildProcessId child_process_id),
              (override));

#if BUILDFLAG(ENABLE_MEMORY_COORDINATOR_INTERNALS)
  MOCK_METHOD(void,
              OnMemoryLimitChanged,
              (std::string_view consumer_id,
               ChildProcessId child_process_id,
               int memory_limit),
              (override));
#endif
};

}  // namespace

class ChildMemoryConsumerRegistryHostTest : public Test {
 protected:
  void BindHost(
      ProcessType process_type,
      ChildProcessId child_process_id,
      mojo::PendingReceiver<mojom::ChildMemoryConsumerRegistryHost> receiver) {
    auto host = std::make_unique<ChildMemoryConsumerRegistryHost>(
        controller_, process_type, child_process_id, std::move(receiver),
        base::BindOnce(&ChildMemoryConsumerRegistryHostTest::OnHostDisconnected,
                       base::Unretained(this), child_process_id));
    bool inserted = hosts_.emplace(child_process_id, std::move(host)).second;
    CHECK(inserted);
  }

  void OnHostDisconnected(ChildProcessId child_process_id) {
    size_t removed = hosts_.erase(child_process_id);
    CHECK_EQ(removed, 1u);
  }

  BrowserTaskEnvironment task_environment_;
  MockMemoryConsumerGroupController controller_;
  absl::flat_hash_map<ChildProcessId,
                      std::unique_ptr<ChildMemoryConsumerRegistryHost>>
      hosts_;
};

TEST_F(ChildMemoryConsumerRegistryHostTest, RegisterAndUnregister) {
  const ChildProcessId kChildId(1);

  EXPECT_CALL(controller_, AddMemoryConsumerGroupHost(kChildId, _));
  mojo::Remote<mojom::ChildMemoryConsumerRegistryHost> remote_host;
  BindHost(PROCESS_TYPE_UTILITY, kChildId,
           remote_host.BindNewPipeAndPassReceiver());

  MockChildMemoryCoordinator mock_coordinator;
  mojo::Receiver<mojom::ChildMemoryCoordinator> coordinator_receiver(
      &mock_coordinator);
  remote_host->BindCoordinator(coordinator_receiver.BindNewPipeAndPassRemote());

  EXPECT_CALL(controller_, OnConsumerGroupAdded(
                               "consumer", _, PROCESS_TYPE_UTILITY, kChildId));

  remote_host->Register("consumer", {});
  remote_host.FlushForTesting();

  EXPECT_CALL(controller_, OnConsumerGroupRemoved("consumer", kChildId));

  remote_host->Unregister("consumer");
  remote_host.FlushForTesting();

  EXPECT_CALL(controller_, RemoveMemoryConsumerGroupHost(kChildId));
  hosts_.clear();
}

TEST_F(ChildMemoryConsumerRegistryHostTest, UpdateConsumers) {
  const ChildProcessId kChildId(1);

  MemoryConsumerGroupHost* host = nullptr;
  EXPECT_CALL(controller_, AddMemoryConsumerGroupHost(kChildId, _))
      .WillOnce(testing::SaveArg<1>(&host));

  mojo::Remote<mojom::ChildMemoryConsumerRegistryHost> remote_host;
  BindHost(PROCESS_TYPE_UTILITY, kChildId,
           remote_host.BindNewPipeAndPassReceiver());

  MockChildMemoryCoordinator mock_coordinator;
  mojo::Receiver<mojom::ChildMemoryCoordinator> coordinator_receiver(
      &mock_coordinator);
  remote_host->BindCoordinator(coordinator_receiver.BindNewPipeAndPassRemote());

  EXPECT_CALL(controller_, OnConsumerGroupAdded(_, _, _, _));

  remote_host->Register("consumer", {});
  remote_host.FlushForTesting();

  ASSERT_TRUE(host);

  EXPECT_CALL(mock_coordinator,
              UpdateConsumers(testing::ElementsAre(
                  MemoryConsumerUpdate{"consumer", 50, true})));
  host->UpdateConsumers({{std::string("consumer"), 50, true}});
  coordinator_receiver.FlushForTesting();

  EXPECT_CALL(controller_, OnConsumerGroupRemoved(_, _));
  EXPECT_CALL(controller_, RemoveMemoryConsumerGroupHost(kChildId));
  hosts_.clear();
}

// Tests that a disconnection with the ChildMemoryCoordinator pipe cleans up the
// data associated with that process.
TEST_F(ChildMemoryConsumerRegistryHostTest, DisconnectCoordinator) {
  const ChildProcessId kChildId(1);

  EXPECT_CALL(controller_, AddMemoryConsumerGroupHost(kChildId, _));
  mojo::Remote<mojom::ChildMemoryConsumerRegistryHost> remote_host;
  BindHost(PROCESS_TYPE_UTILITY, kChildId,
           remote_host.BindNewPipeAndPassReceiver());

  MockChildMemoryCoordinator mock_coordinator;
  mojo::Receiver<mojom::ChildMemoryCoordinator> coordinator_receiver(
      &mock_coordinator);
  remote_host->BindCoordinator(coordinator_receiver.BindNewPipeAndPassRemote());

  EXPECT_CALL(controller_, OnConsumerGroupAdded(_, _, _, _));

  remote_host->Register("consumer", {});
  remote_host.FlushForTesting();

  EXPECT_CALL(controller_, OnConsumerGroupRemoved(_, kChildId));
  EXPECT_CALL(controller_, RemoveMemoryConsumerGroupHost(kChildId))
      .WillOnce(base::test::RunOnceClosure(task_environment_.QuitClosure()));

  coordinator_receiver.reset();
  remote_host.FlushForTesting();

  // We need to wait for the host to be destroyed.
  task_environment_.RunUntilQuit();
}

TEST_F(ChildMemoryConsumerRegistryHostTest, RenderProcessExited) {
  TestBrowserContext browser_context;
  MockRenderProcessHost rph(&browser_context);
  rph.Init();
  const ChildProcessId kChildId = rph.GetID();

  EXPECT_CALL(controller_, AddMemoryConsumerGroupHost(kChildId, _));
  mojo::Remote<mojom::ChildMemoryConsumerRegistryHost> remote_host;
  BindHost(PROCESS_TYPE_RENDERER, kChildId,
           remote_host.BindNewPipeAndPassReceiver());

  EXPECT_CALL(controller_, RemoveMemoryConsumerGroupHost(kChildId))
      .WillOnce(base::test::RunOnceClosure(task_environment_.QuitClosure()));

  rph.SimulateRenderProcessExit(base::TERMINATION_STATUS_PROCESS_CRASHED, 0);

  // We need to wait for the host to be destroyed.
  task_environment_.RunUntilQuit();
}

#if BUILDFLAG(ENABLE_MEMORY_COORDINATOR_INTERNALS)
TEST_F(ChildMemoryConsumerRegistryHostTest, EnableReporting_BeforeBind) {
  const ChildProcessId kChildId(1);
  mojo::Remote<mojom::ChildMemoryConsumerRegistryHost> remote_host;
  BindHost(PROCESS_TYPE_UTILITY, kChildId,
           remote_host.BindNewPipeAndPassReceiver());

  auto it = hosts_.find(kChildId);
  ChildMemoryConsumerRegistryHost* host_impl = it->second.get();

  // 1. Enable reporting BEFORE the coordinator pipe is bound.
  host_impl->EnableDiagnosticsReporting();

  // 2. Bind the coordinator pipe.
  MockChildMemoryCoordinator mock_coordinator;
  mojo::Receiver<mojom::ChildMemoryCoordinator> coordinator_receiver(
      &mock_coordinator);

  // The host should immediately try to enable diagnostics because it was
  // already requested.
  EXPECT_CALL(mock_coordinator, EnableDiagnosticsReporting(_));
  remote_host->BindCoordinator(coordinator_receiver.BindNewPipeAndPassRemote());
  remote_host.FlushForTesting();
}

TEST_F(ChildMemoryConsumerRegistryHostTest, EnableReporting_AfterBind) {
  const ChildProcessId kChildId(1);
  mojo::Remote<mojom::ChildMemoryConsumerRegistryHost> remote_host;
  BindHost(PROCESS_TYPE_UTILITY, kChildId,
           remote_host.BindNewPipeAndPassReceiver());

  auto it = hosts_.find(kChildId);
  ChildMemoryConsumerRegistryHost* host_impl = it->second.get();

  // 1. Bind the coordinator pipe.
  MockChildMemoryCoordinator mock_coordinator;
  mojo::Receiver<mojom::ChildMemoryCoordinator> coordinator_receiver(
      &mock_coordinator);

  // 2. Enable reporting now. The host should immediately try to enable
  // diagnostics because it was already requested.
  EXPECT_CALL(mock_coordinator, EnableDiagnosticsReporting(_));
  host_impl->EnableDiagnosticsReporting();

  remote_host->BindCoordinator(coordinator_receiver.BindNewPipeAndPassRemote());
  remote_host.FlushForTesting();
}

TEST_F(ChildMemoryConsumerRegistryHostTest, OnMemoryLimitChanged) {
  const ChildProcessId kChildId(1);
  mojo::Remote<mojom::ChildMemoryConsumerRegistryHost> remote_host;
  BindHost(PROCESS_TYPE_UTILITY, kChildId,
           remote_host.BindNewPipeAndPassReceiver());

  auto it = hosts_.find(kChildId);
  ChildMemoryConsumerRegistryHost* host_impl = it->second.get();

  mojo::test::BadMessageObserver bad_message_observer;

  // Valid percentage (positive) should be forwarded.
  EXPECT_CALL(controller_, OnMemoryLimitChanged("consumer", kChildId, 100));
  {
    mojo::FakeMessageDispatchContext context;
    host_impl->OnMemoryLimitChanged("consumer", 100);
  }
  EXPECT_FALSE(bad_message_observer.got_bad_message());

  // Invalid percentage (negative) should trigger a bad message.
  EXPECT_CALL(controller_, OnMemoryLimitChanged(_, _, _)).Times(0);
  {
    mojo::FakeMessageDispatchContext context;
    host_impl->OnMemoryLimitChanged("consumer", -1);
    EXPECT_EQ("OnMemoryLimitChanged: out of range",
              bad_message_observer.WaitForBadMessage());
  }
}
#endif  // BUILDFLAG(ENABLE_MEMORY_COORDINATOR_INTERNALS)

}  // namespace content
