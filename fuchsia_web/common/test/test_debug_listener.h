// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_COMMON_TEST_TEST_DEBUG_LISTENER_H_
#define FUCHSIA_WEB_COMMON_TEST_TEST_DEBUG_LISTENER_H_

#include <fuchsia/web/cpp/fidl.h>
#include <lib/fidl/cpp/binding.h>

#include "base/containers/flat_set.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/functional/callback.h"

// Listens to debug events and enables test code to block until a desired
// number of DevTools ports are open.
class TestDebugListener final : public fuchsia::web::DevToolsListener {
 public:
  TestDebugListener();

  TestDebugListener(const TestDebugListener&) = delete;
  TestDebugListener& operator=(const TestDebugListener&) = delete;

  ~TestDebugListener() override;

  // Spins a RunLoop until there are exactly |size| DevTools ports open.
  void RunUntilNumberOfPortsIs(size_t size);

  base::flat_set<uint16_t>& debug_ports() { return debug_ports_; }

 private:
  class TestPerContextListener final
      : public fuchsia::web::DevToolsPerContextListener {
   public:
    TestPerContextListener(
        TestDebugListener* test_debug_listener,
        fidl::InterfaceRequest<fuchsia::web::DevToolsPerContextListener>
            listener);

    TestPerContextListener(const TestPerContextListener&) = delete;
    TestPerContextListener& operator=(const TestPerContextListener&) = delete;

    ~TestPerContextListener() override;

   private:
    // fuchsia::web::DevToolsPerContextListener implementation.
    void OnHttpPortOpen(uint16_t port) override;

    uint16_t port_ = 0;
    TestDebugListener* test_debug_listener_;
    fidl::Binding<fuchsia::web::DevToolsPerContextListener> binding_;
  };

  // fuchsia::web::DevToolsListener implementation.
  void OnContextDevToolsAvailable(
      fidl::InterfaceRequest<fuchsia::web::DevToolsPerContextListener> listener)
      override;

  void DestroyListener(TestPerContextListener* listener);
  void AddPort(uint16_t port);
  void RemovePort(uint16_t port);

  base::flat_set<uint16_t> debug_ports_;
  base::flat_set<std::unique_ptr<TestPerContextListener>,
                 base::UniquePtrComparator>
      per_context_listeners_;
  base::RepeatingClosure on_debug_ports_changed_;
};

#endif  // FUCHSIA_WEB_COMMON_TEST_TEST_DEBUG_LISTENER_H_
