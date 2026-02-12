// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/memory_coordinator/browser_memory_coordinator.h"

#include <memory>

#include "base/test/task_environment.h"
#include "content/common/memory_coordinator/mojom/memory_coordinator.mojom.h"
#include "content/public/common/child_process_id.h"
#include "content/public/common/process_type.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/test_support/fake_message_dispatch_context.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class BrowserMemoryCoordinatorTest : public testing::Test {
 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  BrowserMemoryCoordinator coordinator_;
};

TEST_F(BrowserMemoryCoordinatorTest, DuplicateBind) {
  const ChildProcessId kChildId(1);

  mojo::test::BadMessageObserver bad_message_observer;

  // First bind should succeed.
  mojo::Remote<mojom::ChildMemoryConsumerRegistryHost> remote_host1;
  {
    mojo::FakeMessageDispatchContext context;
    coordinator_.Bind(PROCESS_TYPE_UTILITY, kChildId,
                      remote_host1.BindNewPipeAndPassReceiver());
  }
  EXPECT_FALSE(bad_message_observer.got_bad_message());

  // Second bind for the same ID should be reported as a bad message.
  mojo::Remote<mojom::ChildMemoryConsumerRegistryHost> remote_host2;
  {
    mojo::FakeMessageDispatchContext context;
    coordinator_.Bind(PROCESS_TYPE_UTILITY, kChildId,
                      remote_host2.BindNewPipeAndPassReceiver());
  }
  EXPECT_EQ("Duplicate MemoryCoordinator host registration",
            bad_message_observer.WaitForBadMessage());
}

}  // namespace content
