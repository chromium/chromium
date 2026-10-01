// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/memory_coordinator/browser_memory_coordinator_impl.h"

#include <cstdint>
#include <vector>

#include "base/memory_coordinator/memory_limit.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "content/common/buildflags.h"
#include "content/common/memory_coordinator/mojom/memory_coordinator.mojom.h"
#include "content/public/common/child_process_id.h"
#include "content/public/common/memory_consumer_update.h"
#include "content/public/common/process_type.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

#if BUILDFLAG(ENABLE_MEMORY_COORDINATOR_INTERNALS)
using ::testing::_;
#endif
using ::testing::Test;

class MockChildCoordinator : public mojom::ChildMemoryCoordinator {
 public:
  MOCK_METHOD(void,
              UpdateConsumers,
              (std::vector<MemoryConsumerUpdate> updates),
              (override));
  MOCK_METHOD(void,
              SetOverrideLimit,
              (uint32_t consumer_id, base::MemoryLimit memory_limit),
              (override));
  MOCK_METHOD(void,
              ClearOverrideLimit,
              (uint32_t consumer_id, base::MemoryLimit policy_limit),
              (override));
#if BUILDFLAG(ENABLE_MEMORY_COORDINATOR_INTERNALS)
  MOCK_METHOD(
      void,
      EnableDiagnosticsReporting,
      (mojo::PendingRemote<mojom::MemoryCoordinatorDiagnosticsHost> host),
      (override));
#endif
};

#if BUILDFLAG(ENABLE_MEMORY_COORDINATOR_INTERNALS)
class MockDiagnosticObserver
    : public MemoryCoordinatorPolicyManager::DiagnosticObserver {
 public:
  MOCK_METHOD(void,
              OnMemoryLimitChanged,
              (uint32_t consumer_id,
               ChildProcessId child_process_id,
               base::MemoryLimit memory_limit),
              (override));
};
#endif  // BUILDFLAG(ENABLE_MEMORY_COORDINATOR_INTERNALS)

}  // namespace

class BrowserMemoryCoordinatorImplTest : public Test {
 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  BrowserMemoryCoordinatorImpl browser_coordinator_;
};

TEST_F(BrowserMemoryCoordinatorImplTest, DuplicateBind) {
  const ChildProcessId kChildId(1);

  mojo::Remote<mojom::ChildMemoryConsumerRegistryHost> remote_host1;
  ASSERT_TRUE(
      browser_coordinator_.Bind(PROCESS_TYPE_UTILITY, kChildId,
                                remote_host1.BindNewPipeAndPassReceiver()));

  // A second bind for the same ID should be rejected without requiring a
  // Mojo message dispatch context.
  mojo::Remote<mojom::ChildMemoryConsumerRegistryHost> remote_host2;
  EXPECT_FALSE(
      browser_coordinator_.Bind(PROCESS_TYPE_UTILITY, kChildId,
                                remote_host2.BindNewPipeAndPassReceiver()));

  remote_host2.FlushForTesting();
  EXPECT_FALSE(remote_host2.is_connected());
  remote_host1.FlushForTesting();
  EXPECT_TRUE(remote_host1.is_connected());
}

TEST_F(BrowserMemoryCoordinatorImplTest, BindAfterDisconnect) {
  const ChildProcessId kChildId(1);
  mojo::Remote<mojom::ChildMemoryConsumerRegistryHost> remote_host;
  ASSERT_TRUE(
      browser_coordinator_.Bind(PROCESS_TYPE_UTILITY, kChildId,
                                remote_host.BindNewPipeAndPassReceiver()));

  MockChildCoordinator mock_child_coordinator;
  mojo::Receiver<mojom::ChildMemoryCoordinator> coordinator_receiver(
      &mock_child_coordinator);
  remote_host->BindCoordinator(coordinator_receiver.BindNewPipeAndPassRemote());
  remote_host.FlushForTesting();

  // Host destruction closes the coordinator pipe after disconnect cleanup.
  base::RunLoop disconnect_loop;
  coordinator_receiver.set_disconnect_handler(disconnect_loop.QuitClosure());
  remote_host.reset();
  disconnect_loop.Run();

  ASSERT_TRUE(
      browser_coordinator_.Bind(PROCESS_TYPE_UTILITY, kChildId,
                                remote_host.BindNewPipeAndPassReceiver()));
  remote_host.FlushForTesting();
  EXPECT_TRUE(remote_host.is_connected());
}

#if BUILDFLAG(ENABLE_MEMORY_COORDINATOR_INTERNALS)
TEST_F(BrowserMemoryCoordinatorImplTest, DiagnosticReporting) {
  const ChildProcessId kChildId(1);

  // 1. Bind a child process.
  mojo::Remote<mojom::ChildMemoryConsumerRegistryHost> remote_host;
  ASSERT_TRUE(
      browser_coordinator_.Bind(PROCESS_TYPE_UTILITY, kChildId,
                                remote_host.BindNewPipeAndPassReceiver()));

  // 2. Setup the child-side coordinator mock.
  MockChildCoordinator mock_child_coordinator;
  mojo::Receiver<mojom::ChildMemoryCoordinator> coordinator_receiver(
      &mock_child_coordinator);
  remote_host->BindCoordinator(coordinator_receiver.BindNewPipeAndPassRemote());
  remote_host.FlushForTesting();

  // 3. Add a diagnostic observer. This should trigger
  // EnableDiagnosticsReporting on the child-side mock.
  MockDiagnosticObserver observer;
  EXPECT_CALL(mock_child_coordinator, EnableDiagnosticsReporting(_));
  browser_coordinator_.AddDiagnosticObserver(&observer);
  coordinator_receiver.FlushForTesting();

  // 4. Removing the observer should reset the diagnostics host in the child.
  browser_coordinator_.RemoveDiagnosticObserver(&observer);
}
#endif

}  // namespace content
