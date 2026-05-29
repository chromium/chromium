// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_TESTS_TESTABLE_MULTIPLEX_ROUTER_H_
#define MOJO_PUBLIC_CPP_BINDINGS_TESTS_TESTABLE_MULTIPLEX_ROUTER_H_

#include "base/memory/scoped_refptr.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "mojo/public/cpp/bindings/lib/multiplex_router.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace mojo::test {

// This class extends MultiplexRouter to expose some internals for testing.
// Some methods are overridden to facilitate testing race conditions.
class TestableMultiplexRouter : public internal::MultiplexRouter {
 public:
  TestableMultiplexRouter(ScopedMessagePipeHandle message_pipe,
                          internal::MultiplexRouter::Config config,
                          bool set_interface_id_namespace_bit,
                          scoped_refptr<base::SequencedTaskRunner> runner,
                          const char* primary_interface_name);

  static scoped_refptr<TestableMultiplexRouter> CreateAndStartReceiving(
      ScopedMessagePipeHandle message_pipe,
      internal::MultiplexRouter::Config config,
      bool set_interface_id_namespace_bit,
      scoped_refptr<base::SequencedTaskRunner> runner);

  Connector& GetConnectorForTesting();

  void AddDestructionWait(base::WaitableEvent* event,
                          base::OnceClosure callback);

 private:
  ~TestableMultiplexRouter() override;

  raw_ptr<base::WaitableEvent> destructor_wait_;
  base::OnceClosure destruction_callback_;
};

}  // namespace mojo::test

#endif  // MOJO_PUBLIC_CPP_BINDINGS_TESTS_TESTABLE_MULTIPLEX_ROUTER_H_
