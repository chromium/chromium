// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/memory_coordinator/child_memory_consumer_registry_host.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/memory_coordinator/memory_consumer.h"
#include "base/memory_coordinator/memory_consumer_registry.h"
#include "base/memory_coordinator/traits.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/gmock_callback_support.h"
#include "content/common/buildflags.h"
#include "content/common/memory_coordinator/constants.h"
#include "content/common/memory_coordinator/memory_consumer_group_controller.h"
#include "content/common/memory_coordinator/memory_consumer_group_host.h"
#include "content/public/common/child_process_id.h"
#include "content/public/common/process_type.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/test_support/fake_message_dispatch_context.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

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

constexpr base::MemoryConsumerTraits kTestTraits(
    base::MemoryConsumerTraits::EstimatedMemoryUsage::kSmall,
    base::MemoryConsumerTraits::ReleaseMemoryCost::kFreesPagesWithoutTraversal,
    base::MemoryConsumerTraits::InformationRetention::kLossless,
    base::MemoryConsumerTraits::ExecutionType::kSynchronous);

class MockMemoryConsumerGroupController : public MemoryConsumerGroupController {
 public:
  MOCK_METHOD(void,
              AddMemoryConsumerGroupHost,
              (ProcessType process_type,
               ChildProcessId child_process_id,
               MemoryConsumerGroupHost* host),
              (override));

  MOCK_METHOD(void,
              RemoveMemoryConsumerGroupHost,
              (ChildProcessId child_process_id),
              (override));

  MOCK_METHOD(void,
              OnConsumerGroupAdded,
              (uint32_t consumer_id,
               std::string_view consumer_name,
               base::MemoryConsumerTraits traits,
               ChildProcessId child_process_id),
              (override));

  MOCK_METHOD(void,
              OnConsumerGroupRemoved,
              (uint32_t consumer_id, ChildProcessId child_process_id),
              (override));

#if BUILDFLAG(ENABLE_MEMORY_COORDINATOR_INTERNALS)
  MOCK_METHOD(void,
              OnMemoryLimitChanged,
              (uint32_t consumer_id,
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

  EXPECT_CALL(controller_,
              AddMemoryConsumerGroupHost(PROCESS_TYPE_UTILITY, kChildId, _));
  mojo::Remote<mojom::ChildMemoryConsumerRegistryHost> remote_host;
  BindHost(PROCESS_TYPE_UTILITY, kChildId,
           remote_host.BindNewPipeAndPassReceiver());

  static constexpr char kConsumerName[] = "consumer";
  const uint32_t kConsumerId = base::PersistentHash(kConsumerName);

  MockChildMemoryCoordinator mock_coordinator;
  mojo::Receiver<mojom::ChildMemoryCoordinator> coordinator_receiver(
      &mock_coordinator);
  remote_host->BindCoordinator(coordinator_receiver.BindNewPipeAndPassRemote());

  EXPECT_CALL(controller_,
              OnConsumerGroupAdded(kConsumerId, kConsumerName, _, kChildId));

  std::vector<mojom::MemoryConsumerRegistrationPtr> registrations;
  registrations.push_back(mojom::MemoryConsumerRegistration::New(
      kConsumerId, kConsumerName, kTestTraits));
  remote_host->Register(std::move(registrations));
  remote_host.FlushForTesting();

  EXPECT_CALL(controller_, OnConsumerGroupRemoved(kConsumerId, kChildId));

  remote_host->Unregister(kConsumerId);
  remote_host.FlushForTesting();

  EXPECT_CALL(controller_, RemoveMemoryConsumerGroupHost(kChildId));
  hosts_.clear();
}

TEST_F(ChildMemoryConsumerRegistryHostTest, UpdateConsumers) {
  const ChildProcessId kChildId(1);

  MemoryConsumerGroupHost* host = nullptr;
  EXPECT_CALL(controller_,
              AddMemoryConsumerGroupHost(PROCESS_TYPE_UTILITY, kChildId, _))
      .WillOnce(testing::SaveArg<2>(&host));

  mojo::Remote<mojom::ChildMemoryConsumerRegistryHost> remote_host;
  BindHost(PROCESS_TYPE_UTILITY, kChildId,
           remote_host.BindNewPipeAndPassReceiver());

  MockChildMemoryCoordinator mock_coordinator;
  mojo::Receiver<mojom::ChildMemoryCoordinator> coordinator_receiver(
      &mock_coordinator);
  remote_host->BindCoordinator(coordinator_receiver.BindNewPipeAndPassRemote());

  EXPECT_CALL(controller_, OnConsumerGroupAdded(_, _, _, _));

  static constexpr char kConsumerName[] = "consumer";
  const uint32_t kConsumerId = base::PersistentHash(kConsumerName);

  std::vector<mojom::MemoryConsumerRegistrationPtr> registrations;
  registrations.push_back(mojom::MemoryConsumerRegistration::New(
      kConsumerId, kConsumerName, kTestTraits));
  remote_host->Register(std::move(registrations));
  remote_host.FlushForTesting();

  ASSERT_TRUE(host);

  EXPECT_CALL(mock_coordinator,
              UpdateConsumers(testing::ElementsAre(
                  MemoryConsumerUpdate{kConsumerId, 50, true})));
  host->UpdateConsumers({{kConsumerId, 50, true}});
  coordinator_receiver.FlushForTesting();

  EXPECT_CALL(controller_, OnConsumerGroupRemoved(_, _));
  EXPECT_CALL(controller_, RemoveMemoryConsumerGroupHost(kChildId));
  hosts_.clear();
}

// Tests that a disconnection with the ChildMemoryCoordinator pipe cleans up the
// data associated with that process.
TEST_F(ChildMemoryConsumerRegistryHostTest, DisconnectCoordinator) {
  const ChildProcessId kChildId(1);

  EXPECT_CALL(controller_,
              AddMemoryConsumerGroupHost(PROCESS_TYPE_UTILITY, kChildId, _));
  mojo::Remote<mojom::ChildMemoryConsumerRegistryHost> remote_host;
  BindHost(PROCESS_TYPE_UTILITY, kChildId,
           remote_host.BindNewPipeAndPassReceiver());

  MockChildMemoryCoordinator mock_coordinator;
  mojo::Receiver<mojom::ChildMemoryCoordinator> coordinator_receiver(
      &mock_coordinator);
  remote_host->BindCoordinator(coordinator_receiver.BindNewPipeAndPassRemote());

  EXPECT_CALL(controller_, OnConsumerGroupAdded(_, _, _, _));

  static constexpr char kConsumerName[] = "consumer";
  const uint32_t kConsumerId = base::PersistentHash(kConsumerName);

  std::vector<mojom::MemoryConsumerRegistrationPtr> registrations;
  registrations.push_back(mojom::MemoryConsumerRegistration::New(
      kConsumerId, kConsumerName, kTestTraits));
  remote_host->Register(std::move(registrations));
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

  EXPECT_CALL(controller_,
              AddMemoryConsumerGroupHost(PROCESS_TYPE_RENDERER, kChildId, _));
  mojo::Remote<mojom::ChildMemoryConsumerRegistryHost> remote_host;
  BindHost(PROCESS_TYPE_RENDERER, kChildId,
           remote_host.BindNewPipeAndPassReceiver());

  EXPECT_CALL(controller_, RemoveMemoryConsumerGroupHost(kChildId))
      .WillOnce(base::test::RunOnceClosure(task_environment_.QuitClosure()));

  rph.SimulateRenderProcessExit(base::TERMINATION_STATUS_PROCESS_CRASHED, 0);

  // We need to wait for the host to be destroyed.
  task_environment_.RunUntilQuit();
}

TEST_F(ChildMemoryConsumerRegistryHostTest, Register_TooManyConsumers) {
  const ChildProcessId kChildId(1);

  EXPECT_CALL(controller_,
              AddMemoryConsumerGroupHost(PROCESS_TYPE_UTILITY, kChildId, _));
  mojo::Remote<mojom::ChildMemoryConsumerRegistryHost> remote_host;
  BindHost(PROCESS_TYPE_UTILITY, kChildId,
           remote_host.BindNewPipeAndPassReceiver());

  MockChildMemoryCoordinator mock_coordinator;
  mojo::Receiver<mojom::ChildMemoryCoordinator> coordinator_receiver(
      &mock_coordinator);
  remote_host->BindCoordinator(coordinator_receiver.BindNewPipeAndPassRemote());

  EXPECT_CALL(controller_, OnConsumerGroupAdded(_, _, _, _))
      .Times(kMaxMemoryConsumersPerProcess);

  std::vector<mojom::MemoryConsumerRegistrationPtr> registrations;
  for (size_t i = 0; i < kMaxMemoryConsumersPerProcess; ++i) {
    std::string name = "consumer" + base::NumberToString(i);
    registrations.push_back(mojom::MemoryConsumerRegistration::New(
        base::PersistentHash(name), name, kTestTraits));
  }
  remote_host->Register(std::move(registrations));
  remote_host.FlushForTesting();

  // The next registration should fail.
  mojo::test::BadMessageObserver bad_message_observer;
  std::string name = "extra";
  std::vector<mojom::MemoryConsumerRegistrationPtr> extra_registrations;
  extra_registrations.push_back(mojom::MemoryConsumerRegistration::New(
      base::PersistentHash(name), name, kTestTraits));
  remote_host->Register(std::move(extra_registrations));
  EXPECT_EQ("Too many memory consumers registered",
            bad_message_observer.WaitForBadMessage());

  EXPECT_CALL(controller_, OnConsumerGroupRemoved(_, _))
      .Times(kMaxMemoryConsumersPerProcess);
  EXPECT_CALL(controller_, RemoveMemoryConsumerGroupHost(kChildId));
  hosts_.clear();
}

TEST_F(ChildMemoryConsumerRegistryHostTest, Register_NameTooLong) {
  const ChildProcessId kChildId(1);

  EXPECT_CALL(controller_,
              AddMemoryConsumerGroupHost(PROCESS_TYPE_UTILITY, kChildId, _));
  mojo::Remote<mojom::ChildMemoryConsumerRegistryHost> remote_host;
  BindHost(PROCESS_TYPE_UTILITY, kChildId,
           remote_host.BindNewPipeAndPassReceiver());

  MockChildMemoryCoordinator mock_coordinator;
  mojo::Receiver<mojom::ChildMemoryCoordinator> coordinator_receiver(
      &mock_coordinator);
  remote_host->BindCoordinator(coordinator_receiver.BindNewPipeAndPassRemote());

  mojo::test::BadMessageObserver bad_message_observer;
  std::string long_name(kMaxMemoryConsumerNameLength + 1, 'a');
  std::vector<mojom::MemoryConsumerRegistrationPtr> registrations;
  registrations.push_back(mojom::MemoryConsumerRegistration::New(
      base::PersistentHash(long_name), long_name, kTestTraits));
  remote_host->Register(std::move(registrations));
  EXPECT_EQ("Memory consumer name is too long",
            bad_message_observer.WaitForBadMessage());

  EXPECT_CALL(controller_, RemoveMemoryConsumerGroupHost(kChildId));
  hosts_.clear();
}

TEST_F(ChildMemoryConsumerRegistryHostTest, Register_InvalidConsumerId) {
  const ChildProcessId kChildId(1);

  EXPECT_CALL(controller_,
              AddMemoryConsumerGroupHost(PROCESS_TYPE_UTILITY, kChildId, _));
  mojo::Remote<mojom::ChildMemoryConsumerRegistryHost> remote_host;
  BindHost(PROCESS_TYPE_UTILITY, kChildId,
           remote_host.BindNewPipeAndPassReceiver());

  MockChildMemoryCoordinator mock_coordinator;
  mojo::Receiver<mojom::ChildMemoryCoordinator> coordinator_receiver(
      &mock_coordinator);
  remote_host->BindCoordinator(coordinator_receiver.BindNewPipeAndPassRemote());

  mojo::test::BadMessageObserver bad_message_observer;
  static constexpr char kConsumerName[] = "consumer";
  const uint32_t kInvalidConsumerId = base::PersistentHash(kConsumerName) + 1;
  std::vector<mojom::MemoryConsumerRegistrationPtr> registrations;
  registrations.push_back(mojom::MemoryConsumerRegistration::New(
      kInvalidConsumerId, kConsumerName, kTestTraits));
  remote_host->Register(std::move(registrations));
  EXPECT_EQ("consumer_id does not match the hash of consumer_name",
            bad_message_observer.WaitForBadMessage());

  EXPECT_CALL(controller_, RemoveMemoryConsumerGroupHost(kChildId));
  hosts_.clear();
}

TEST_F(ChildMemoryConsumerRegistryHostTest, Register_Batch) {
  const ChildProcessId kChildId(1);

  EXPECT_CALL(controller_,
              AddMemoryConsumerGroupHost(PROCESS_TYPE_UTILITY, kChildId, _));
  mojo::Remote<mojom::ChildMemoryConsumerRegistryHost> remote_host;
  BindHost(PROCESS_TYPE_UTILITY, kChildId,
           remote_host.BindNewPipeAndPassReceiver());

  MockChildMemoryCoordinator mock_coordinator;
  mojo::Receiver<mojom::ChildMemoryCoordinator> coordinator_receiver(
      &mock_coordinator);
  remote_host->BindCoordinator(coordinator_receiver.BindNewPipeAndPassRemote());

  static constexpr char kNameA[] = "consumer_a";
  static constexpr char kNameB[] = "consumer_b";
  const uint32_t kIdA = base::PersistentHash(kNameA);
  const uint32_t kIdB = base::PersistentHash(kNameB);

  // A batched Register() registers every consumer in the batch.
  EXPECT_CALL(controller_, OnConsumerGroupAdded(kIdA, kNameA, _, kChildId));
  EXPECT_CALL(controller_, OnConsumerGroupAdded(kIdB, kNameB, _, kChildId));

  std::vector<mojom::MemoryConsumerRegistrationPtr> registrations;
  registrations.push_back(
      mojom::MemoryConsumerRegistration::New(kIdA, kNameA, kTestTraits));
  registrations.push_back(
      mojom::MemoryConsumerRegistration::New(kIdB, kNameB, kTestTraits));
  remote_host->Register(std::move(registrations));
  remote_host.FlushForTesting();

  EXPECT_CALL(controller_, OnConsumerGroupRemoved(_, _)).Times(2);
  EXPECT_CALL(controller_, RemoveMemoryConsumerGroupHost(kChildId));
  hosts_.clear();
}

// Tests that an invalid entry in a batched Register() reports a bad message
// and stops processing, leaving entries after it unregistered.
TEST_F(ChildMemoryConsumerRegistryHostTest, Register_StopsOnInvalidEntry) {
  const ChildProcessId kChildId(1);

  EXPECT_CALL(controller_,
              AddMemoryConsumerGroupHost(PROCESS_TYPE_UTILITY, kChildId, _));
  mojo::Remote<mojom::ChildMemoryConsumerRegistryHost> remote_host;
  BindHost(PROCESS_TYPE_UTILITY, kChildId,
           remote_host.BindNewPipeAndPassReceiver());

  MockChildMemoryCoordinator mock_coordinator;
  mojo::Receiver<mojom::ChildMemoryCoordinator> coordinator_receiver(
      &mock_coordinator);
  remote_host->BindCoordinator(coordinator_receiver.BindNewPipeAndPassRemote());

  static constexpr char kGoodName[] = "good";
  static constexpr char kBadName[] = "bad";
  static constexpr char kAfterName[] = "after";
  const uint32_t kGoodId = base::PersistentHash(kGoodName);

  // The first (valid) entry registers; the second has a mismatched id, which
  // reports a bad message and aborts the batch; the third is never reached.
  EXPECT_CALL(controller_,
              OnConsumerGroupAdded(kGoodId, kGoodName, _, kChildId));
  EXPECT_CALL(controller_,
              OnConsumerGroupAdded(base::PersistentHash(kAfterName), _, _, _))
      .Times(0);

  std::vector<mojom::MemoryConsumerRegistrationPtr> registrations;
  registrations.push_back(
      mojom::MemoryConsumerRegistration::New(kGoodId, kGoodName, kTestTraits));
  registrations.push_back(mojom::MemoryConsumerRegistration::New(
      base::PersistentHash(kBadName) + 1, kBadName, kTestTraits));
  registrations.push_back(mojom::MemoryConsumerRegistration::New(
      base::PersistentHash(kAfterName), kAfterName, kTestTraits));

  mojo::test::BadMessageObserver bad_message_observer;
  remote_host->Register(std::move(registrations));
  EXPECT_EQ("consumer_id does not match the hash of consumer_name",
            bad_message_observer.WaitForBadMessage());

  // Only the first (valid) consumer remains registered.
  EXPECT_CALL(controller_, OnConsumerGroupRemoved(kGoodId, _));
  EXPECT_CALL(controller_, RemoveMemoryConsumerGroupHost(kChildId));
  hosts_.clear();
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

TEST_F(ChildMemoryConsumerRegistryHostTest, OnMemoryLimitChanged_Valid) {
  const ChildProcessId kChildId(1);
  mojo::Remote<mojom::ChildMemoryConsumerRegistryHost> remote_host;
  BindHost(PROCESS_TYPE_UTILITY, kChildId,
           remote_host.BindNewPipeAndPassReceiver());

  auto it = hosts_.find(kChildId);
  ChildMemoryConsumerRegistryHost* host_impl = it->second.get();

  static constexpr char kConsumerName[] = "consumer";
  const uint32_t kConsumerId = base::PersistentHash(kConsumerName);

  // Register the consumer first.
  MockChildMemoryCoordinator mock_coordinator;
  mojo::Receiver<mojom::ChildMemoryCoordinator> coordinator_receiver(
      &mock_coordinator);
  remote_host->BindCoordinator(coordinator_receiver.BindNewPipeAndPassRemote());
  EXPECT_CALL(controller_, OnConsumerGroupAdded(kConsumerId, _, _, _));
  std::vector<mojom::MemoryConsumerRegistrationPtr> registrations;
  registrations.push_back(mojom::MemoryConsumerRegistration::New(
      kConsumerId, kConsumerName, kTestTraits));
  remote_host->Register(std::move(registrations));
  remote_host.FlushForTesting();

  // Valid percentage (positive) should be forwarded.
  EXPECT_CALL(controller_, OnMemoryLimitChanged(kConsumerId, kChildId, 100));
  {
    mojo::FakeMessageDispatchContext context;
    host_impl->OnMemoryLimitChanged(kConsumerId, 100);
  }
}

TEST_F(ChildMemoryConsumerRegistryHostTest, OnMemoryLimitChanged_InvalidRange) {
  const ChildProcessId kChildId(1);
  mojo::Remote<mojom::ChildMemoryConsumerRegistryHost> remote_host;
  BindHost(PROCESS_TYPE_UTILITY, kChildId,
           remote_host.BindNewPipeAndPassReceiver());

  auto it = hosts_.find(kChildId);
  ChildMemoryConsumerRegistryHost* host_impl = it->second.get();

  static constexpr char kConsumerName[] = "consumer";
  const uint32_t kConsumerId = base::PersistentHash(kConsumerName);

  // Register the consumer first.
  MockChildMemoryCoordinator mock_coordinator;
  mojo::Receiver<mojom::ChildMemoryCoordinator> coordinator_receiver(
      &mock_coordinator);
  remote_host->BindCoordinator(coordinator_receiver.BindNewPipeAndPassRemote());
  EXPECT_CALL(controller_, OnConsumerGroupAdded(kConsumerId, _, _, _));
  std::vector<mojom::MemoryConsumerRegistrationPtr> registrations;
  registrations.push_back(mojom::MemoryConsumerRegistration::New(
      kConsumerId, kConsumerName, kTestTraits));
  remote_host->Register(std::move(registrations));
  remote_host.FlushForTesting();

  // Invalid percentage (negative) should trigger a bad message.
  EXPECT_CALL(controller_, OnMemoryLimitChanged(_, _, _)).Times(0);
  {
    mojo::test::BadMessageObserver bad_message_observer;
    mojo::FakeMessageDispatchContext context;
    host_impl->OnMemoryLimitChanged(kConsumerId, -1);
    EXPECT_EQ("OnMemoryLimitChanged: out of range",
              bad_message_observer.WaitForBadMessage());
  }
}

TEST_F(ChildMemoryConsumerRegistryHostTest,
       OnMemoryLimitChanged_UnknownConsumerId) {
  const ChildProcessId kChildId(1);
  mojo::Remote<mojom::ChildMemoryConsumerRegistryHost> remote_host;
  BindHost(PROCESS_TYPE_UTILITY, kChildId,
           remote_host.BindNewPipeAndPassReceiver());

  auto it = hosts_.find(kChildId);
  ChildMemoryConsumerRegistryHost* host_impl = it->second.get();

  // Unknown consumer_id should trigger a bad message.
  const uint32_t kUnknownConsumerId = 12345;
  {
    mojo::test::BadMessageObserver bad_message_observer;
    mojo::FakeMessageDispatchContext context;
    host_impl->OnMemoryLimitChanged(kUnknownConsumerId, 100);
    EXPECT_EQ("OnMemoryLimitChanged: unknown consumer_id",
              bad_message_observer.WaitForBadMessage());
  }
}
#endif  // BUILDFLAG(ENABLE_MEMORY_COORDINATOR_INTERNALS)

}  // namespace content
