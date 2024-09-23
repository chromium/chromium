// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/runners/cast/test/fake_api_bindings.h"

#include <fuchsia/web/cpp/fidl.h>

#include <string_view>

#include "base/auto_reset.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/run_loop.h"

FakeApiBindingsImpl::FakeApiBindingsImpl() = default;

FakeApiBindingsImpl::~FakeApiBindingsImpl() = default;

fidl::InterfaceHandle<::fuchsia::web::MessagePort>
FakeApiBindingsImpl::RunAndReturnConnectedPort(std::string_view name) {
  base::AutoReset<std::string_view> store_name(&expected_port_name_, name);

  auto it = ports_.find(expected_port_name_);
  if (it == ports_.end()) {
    base::RunLoop run_loop;
    base::AutoReset<base::OnceClosure> store_closure(
        &on_expected_port_received_, run_loop.QuitClosure());
    run_loop.Run();
    it = ports_.find(expected_port_name_);
  }

  if (it == ports_.end()) {
    return {};
  }

  fidl::InterfaceHandle<::fuchsia::web::MessagePort> port =
      std::move(it->second);
  ports_.erase(it);
  return port;
}

void FakeApiBindingsImpl::GetAll(GetAllCallback callback) {
  std::vector<chromium::cast::ApiBinding> bindings_clone;
  for (auto& binding : bindings_) {
    chromium::cast::ApiBinding binding_clone;
    zx_status_t status = binding.Clone(&binding_clone);
    ZX_CHECK(status == ZX_OK, status);
    bindings_clone.push_back(std::move(binding_clone));
  }
  callback(std::move(bindings_clone));
}

void FakeApiBindingsImpl::Connect(
    std::string port_name,
    fidl::InterfaceHandle<::fuchsia::web::MessagePort> message_port) {
  DCHECK(!port_name.empty());

  ports_[port_name] = std::move(message_port);
  if (expected_port_name_ == port_name) {
    DCHECK(on_expected_port_received_);
    std::move(on_expected_port_received_).Run();
  }
}
