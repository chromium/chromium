// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/tests/testable_multiplex_router.h"

#include <gtest/gtest.h>

#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/connector.h"

namespace mojo::test {

TestableMultiplexRouter::TestableMultiplexRouter(
    ScopedMessagePipeHandle message_pipe,
    internal::MultiplexRouter::Config config,
    bool set_interface_id_namespace_bit,
    scoped_refptr<base::SequencedTaskRunner> runner,
    const char* primary_interface_name)
    : MultiplexRouter(base::PassKey<TestableMultiplexRouter>(),
                      std::move(message_pipe),
                      config,
                      set_interface_id_namespace_bit,
                      runner,
                      primary_interface_name) {}

scoped_refptr<TestableMultiplexRouter>
TestableMultiplexRouter::CreateAndStartReceiving(
    ScopedMessagePipeHandle message_pipe,
    internal::MultiplexRouter::Config config,
    bool set_interface_id_namespace_bit,
    scoped_refptr<base::SequencedTaskRunner> runner) {
  auto router = base::MakeRefCounted<TestableMultiplexRouter>(
      std::move(message_pipe), config, set_interface_id_namespace_bit, runner,
      "TestableMultiplexRouter");
  router->StartReceiving();
  return router;
}

Connector& TestableMultiplexRouter::GetConnectorForTesting() {
  return MultiplexRouter::GetConnectorForTesting();  // IN-TEST
}

void TestableMultiplexRouter::AddDestructionWait(base::WaitableEvent* event,
                                                 base::OnceClosure callback) {
  destructor_wait_ = event;
  destruction_callback_ = std::move(callback);
}

TestableMultiplexRouter::~TestableMultiplexRouter() {
  if (destructor_wait_) {
    // We don't want to hang forever, this should signal eventually.
    EXPECT_TRUE(destructor_wait_->TimedWait(base::Seconds(1)));
    destructor_wait_ = nullptr;
  }
  if (destruction_callback_) {
    std::move(destruction_callback_).Run();
  }
}
}  // namespace mojo::test
