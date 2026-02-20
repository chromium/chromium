// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/memory_coordinator/browser_memory_coordinator.h"

#include <memory>

#include "base/test/task_environment.h"
#include "content/common/buildflags.h"
#include "content/common/memory_coordinator/mojom/memory_coordinator.mojom.h"
#include "content/public/common/child_process_id.h"
#include "content/public/common/process_type.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/test_support/fake_message_dispatch_context.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(ENABLE_MEMORY_COORDINATOR_INTERNALS)
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#endif

namespace content {

namespace {

#if BUILDFLAG(ENABLE_MEMORY_COORDINATOR_INTERNALS)
using ::testing::_;
#endif
using ::testing::Test;

#if BUILDFLAG(ENABLE_MEMORY_COORDINATOR_INTERNALS)

class MockChildCoordinator : public mojom::ChildMemoryCoordinator {
 public:
  MOCK_METHOD(void,
              UpdateConsumers,
              (std::vector<MemoryConsumerUpdate> updates),
              (override));
  MOCK_METHOD(
      void,
      EnableDiagnosticsReporting,
      (mojo::PendingRemote<mojom::MemoryCoordinatorDiagnosticsHost> host),
      (override));
};

class MockDiagnosticObserver
    : public MemoryCoordinatorPolicyManager::DiagnosticObserver {
 public:
  MOCK_METHOD(void,
              OnMemoryLimitChanged,
              (std::string_view consumer_id,
               ChildProcessId child_process_id,
               int memory_limit),
              (override));
};
#endif  // BUILDFLAG(ENABLE_MEMORY_COORDINATOR_INTERNALS)

}  // namespace

class BrowserMemoryCoordinatorTest : public Test {
 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  BrowserMemoryCoordinator browser_coordinator_;
};

TEST_F(BrowserMemoryCoordinatorTest, DuplicateBind) {
  const ChildProcessId kChildId(1);

  mojo::test::BadMessageObserver bad_message_observer;

  // First bind should succeed.
  mojo::Remote<mojom::ChildMemoryConsumerRegistryHost> remote_host1;
  {
    mojo::FakeMessageDispatchContext context;
    browser_coordinator_.Bind(PROCESS_TYPE_UTILITY, kChildId,
                              remote_host1.BindNewPipeAndPassReceiver());
  }
  EXPECT_FALSE(bad_message_observer.got_bad_message());

  // Second bind for the same ID should be reported as a bad message.
  mojo::Remote<mojom::ChildMemoryConsumerRegistryHost> remote_host2;
  {
    mojo::FakeMessageDispatchContext context;
    browser_coordinator_.Bind(PROCESS_TYPE_UTILITY, kChildId,
                              remote_host2.BindNewPipeAndPassReceiver());
  }
  EXPECT_EQ("Duplicate MemoryCoordinator host registration",
            bad_message_observer.WaitForBadMessage());
}

#if BUILDFLAG(ENABLE_MEMORY_COORDINATOR_INTERNALS)
TEST_F(BrowserMemoryCoordinatorTest, DiagnosticReporting) {
  const ChildProcessId kChildId(1);

  // 1. Bind a child process.
  mojo::Remote<mojom::ChildMemoryConsumerRegistryHost> remote_host;
  browser_coordinator_.Bind(PROCESS_TYPE_UTILITY, kChildId,
                            remote_host.BindNewPipeAndPassReceiver());

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
