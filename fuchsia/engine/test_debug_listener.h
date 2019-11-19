// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_ENGINE_TEST_DEBUG_LISTENER_H_
#define FUCHSIA_ENGINE_TEST_DEBUG_LISTENER_H_

#include <fuchsia/web/cpp/fidl.h>
#include <lib/fidl/cpp/binding.h>

#include "base/callback.h"
#include "base/containers/flat_set.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/macros.h"

// Listens to debug events and enables test code to block until a desired
// number of DevTools ports are open.
class TestDebugListener : public fuchsia::web::DevToolsListener {
 public:
  TestDebugListener();
  ~TestDebugListener() final;

  // Spins a RunLoop until there are exactly |size| DevTools ports open.
  void RunUntilNumberOfPortsIs(size_t size);

  base::flat_set<uint16_t>& debug_ports() { return debug_ports_; }

 private:
  class TestPerContextListener
      : public fuchsia::web::DevToolsPerContextListener {
   public:
    TestPerContextListener(
        TestDebugListener* test_debug_listener,
        fidl::InterfaceRequest<fuchsia::web::DevToolsPerContextListener>
            listener);
    ~TestPerContextListener() final;

   private:
    // fuchsia::web::DevToolsPerContextListener implementation.
    void OnHttpPortOpen(uint16_t port) final;

    uint16_t port_ = 0;
    TestDebugListener* test_debug_listener_;
    fidl::Binding<fuchsia::web::DevToolsPerContextListener> binding_;

    DISALLOW_COPY_AND_ASSIGN(TestPerContextListener);
  };

  // fuchsia::web::DevToolsListener implementation.
  void OnContextDevToolsAvailable(
      fidl::InterfaceRequest<fuchsia::web::DevToolsPerContextListener> listener)
      final;

  void DestroyListener(TestPerContextListener* listener);
  void AddPort(uint16_t port);
  void RemovePort(uint16_t port);

  base::flat_set<uint16_t> debug_ports_;
  base::flat_set<std::unique_ptr<TestPerContextListener>,
                 base::UniquePtrComparator>
      per_context_listeners_;
  base::RepeatingClosure on_debug_ports_changed_;

  DISALLOW_COPY_AND_ASSIGN(TestDebugListener);
};

#endif  // FUCHSIA_ENGINE_TEST_DEBUG_LISTENER_H_
