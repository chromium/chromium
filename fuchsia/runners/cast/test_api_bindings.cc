// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/runners/cast/test_api_bindings.h"

#include "base/run_loop.h"

TestApiBindings::TestApiBindings() = default;

TestApiBindings::~TestApiBindings() = default;

fidl::InterfaceHandle<::fuchsia::web::MessagePort>
TestApiBindings::RunUntilMessagePortReceived(base::StringPiece port_name) {
  while (ports_.find(port_name.as_string()) == ports_.end()) {
    base::RunLoop run_loop;
    port_received_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  fidl::InterfaceHandle<::fuchsia::web::MessagePort> port =
      std::move(ports_[port_name.as_string()]);
  ports_.erase(port_name.as_string());
  return port;
}

void TestApiBindings::GetAll(GetAllCallback callback) {
  callback(std::move(bindings_));
}

void TestApiBindings::Connect(
    std::string port_name,
    fidl::InterfaceHandle<::fuchsia::web::MessagePort> message_port) {
  ports_[port_name] = std::move(message_port);

  if (port_received_closure_)
    std::move(port_received_closure_).Run();
}
