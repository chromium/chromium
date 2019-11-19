// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/test_debug_listener.h"

#include "base/auto_reset.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

TestDebugListener::TestDebugListener() {}
TestDebugListener::~TestDebugListener() = default;

void TestDebugListener::DestroyListener(TestPerContextListener* listener) {
  EXPECT_EQ(per_context_listeners_.erase(listener), 1u);
}

void TestDebugListener::AddPort(uint16_t port) {
  EXPECT_EQ(debug_ports_.find(port), debug_ports_.end());
  debug_ports_.insert(port);
  if (on_debug_ports_changed_)
    on_debug_ports_changed_.Run();
}

void TestDebugListener::RemovePort(uint16_t port) {
  EXPECT_EQ(debug_ports_.erase(port), 1u);
  if (on_debug_ports_changed_)
    on_debug_ports_changed_.Run();
}

void TestDebugListener::RunUntilNumberOfPortsIs(size_t size) {
  if (debug_ports_.size() == size)
    return;

  base::RunLoop run_loop;
  base::AutoReset<base::RepeatingClosure> set_on_debug_ports_changed(
      &on_debug_ports_changed_,
      base::BindLambdaForTesting([this, &run_loop, size]() {
        if (debug_ports_.size() == size)
          run_loop.Quit();
      }));
  run_loop.Run();
  EXPECT_EQ(debug_ports_.size(), size);
}

TestDebugListener::TestPerContextListener::TestPerContextListener(
    TestDebugListener* test_debug_listener,
    fidl::InterfaceRequest<fuchsia::web::DevToolsPerContextListener> listener)
    : test_debug_listener_(test_debug_listener),
      binding_(this, std::move(listener)) {
  binding_.set_error_handler([this](zx_status_t) {
    if (port_ != 0)
      test_debug_listener_->RemovePort(port_);
    test_debug_listener_->DestroyListener(this);
  });
}

TestDebugListener::TestPerContextListener::~TestPerContextListener() = default;

void TestDebugListener::TestPerContextListener::OnHttpPortOpen(uint16_t port) {
  port_ = port;
  test_debug_listener_->AddPort(port);
}

void TestDebugListener::OnContextDevToolsAvailable(
    fidl::InterfaceRequest<fuchsia::web::DevToolsPerContextListener> listener) {
  per_context_listeners_.insert(
      std::make_unique<TestPerContextListener>(this, std::move(listener)));
}
