// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/common/test/test_debug_listener.h"

#include "base/auto_reset.h"
#include "base/containers/contains.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "testing/gtest/include/gtest/gtest.h"

TestDebugListener::TestDebugListener() = default;
TestDebugListener::~TestDebugListener() = default;

void TestDebugListener::DestroyListener(TestPerContextListener* listener) {
  EXPECT_EQ(per_context_listeners_.erase(listener), 1u);
}

void TestDebugListener::AddPort(uint16_t port) {
  EXPECT_FALSE(base::Contains(debug_ports_, port));
  debug_ports_.insert(port);
  if (on_debug_ports_changed_) {
    on_debug_ports_changed_.Run();
  }
}

void TestDebugListener::RemovePort(uint16_t port) {
  EXPECT_EQ(debug_ports_.erase(port), 1u);
  if (on_debug_ports_changed_) {
    on_debug_ports_changed_.Run();
  }
}

void TestDebugListener::RunUntilNumberOfPortsIs(size_t size) {
  if (debug_ports_.size() == size) {
    return;
  }

  base::RunLoop run_loop;
  base::AutoReset<base::RepeatingClosure> set_on_debug_ports_changed(
      &on_debug_ports_changed_,
      base::BindLambdaForTesting([this, &run_loop, size]() {
        if (debug_ports_.size() == size) {
          run_loop.Quit();
        }
      }));
  run_loop.Run();
  ASSERT_EQ(debug_ports_.size(), size);
}

TestDebugListener::TestPerContextListener::TestPerContextListener(
    TestDebugListener* test_debug_listener,
    fidl::InterfaceRequest<fuchsia::web::DevToolsPerContextListener> listener)
    : test_debug_listener_(test_debug_listener),
      binding_(this, std::move(listener)) {
  binding_.set_error_handler([this](zx_status_t) {
    if (port_ != 0) {
      test_debug_listener_->RemovePort(port_);
    }
    test_debug_listener_->DestroyListener(this);
  });
}

TestDebugListener::TestPerContextListener::~TestPerContextListener() = default;

void TestDebugListener::TestPerContextListener::OnHttpPortOpen(uint16_t port) {
  // If `port` is non-zero then the PerContextListener has created, or replaced,
  // its DevTools port. If `port` is zero then the PerContextListener is
  // reporting that it is not listening on DevTools.
  // Remove the previously-reported `port_`, if any, from the TestDebugListener,
  // before adding the new `port`, if set, to maintain the list of available
  // DevTools ports.
  if (port_ != 0) {
    test_debug_listener_->RemovePort(port_);
  }
  port_ = port;
  if (port != 0) {
    test_debug_listener_->AddPort(port);
  }
}

void TestDebugListener::OnContextDevToolsAvailable(
    fidl::InterfaceRequest<fuchsia::web::DevToolsPerContextListener> listener) {
  per_context_listeners_.insert(
      std::make_unique<TestPerContextListener>(this, std::move(listener)));
}
