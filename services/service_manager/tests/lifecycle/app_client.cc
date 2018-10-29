// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/tests/lifecycle/app_client.h"

#include "base/macros.h"
#include "base/run_loop.h"
#include "services/service_manager/public/cpp/service_binding.h"

namespace service_manager {
namespace test {

AppClient::AppClient(service_manager::mojom::ServiceRequest request,
                     base::OnceClosure quit_closure)
    : service_binding_(this, std::move(request)),
      quit_closure_(std::move(quit_closure)) {
  bindings_.set_connection_error_handler(base::BindRepeating(
      &AppClient::LifecycleControlBindingLost, base::Unretained(this)));

  registry_.AddInterface<mojom::LifecycleControl>(
      base::BindRepeating(&AppClient::Create, base::Unretained(this)));
}

AppClient::~AppClient() {}

void AppClient::OnBindInterface(const BindSourceInfo& source_info,
                                const std::string& interface_name,
                                mojo::ScopedMessagePipeHandle interface_pipe) {
  registry_.BindInterface(interface_name, std::move(interface_pipe));
}

void AppClient::OnDisconnected() {
  DCHECK(service_binding_.is_bound());
  service_binding_.Close();

  if (quit_closure_)
    std::move(quit_closure_).Run();
}

void AppClient::Create(mojom::LifecycleControlRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void AppClient::Ping(PingCallback callback) {
  std::move(callback).Run();
}

void AppClient::GracefulQuit() {
  if (service_binding_.is_bound())
    service_binding_.RequestClose();
  else if (quit_closure_)
    std::move(quit_closure_).Run();
}

void AppClient::Crash() {
  // Rather than actually crash, which causes a bunch of console spray and
  // maybe UI clutter on some platforms, just exit without shutting anything
  // down properly.
  exit(1);
}

void AppClient::CloseServiceManagerConnection() {
  if (service_binding_.is_bound())
    service_binding_.Close();
}

void AppClient::LifecycleControlBindingLost() {
  if (!service_binding_.is_bound() && bindings_.empty() && quit_closure_)
    std::move(quit_closure_).Run();
}

}  // namespace test
}  // namespace service_manager
