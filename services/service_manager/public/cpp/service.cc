// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/public/cpp/service.h"

#include "base/functional/bind.h"
#include "base/run_loop.h"

namespace service_manager {

Service::Service() = default;

Service::~Service() = default;

// static
void Service::RunAsyncUntilTermination(std::unique_ptr<Service> service,
                                       base::OnceClosure callback) {
  auto* raw_service = service.get();
  raw_service->set_termination_closure(base::BindOnce(
      [](std::unique_ptr<Service> service, base::OnceClosure callback) {
        service.reset();
        if (callback)
          std::move(callback).Run();
      },
      std::move(service), std::move(callback)));
}

void Service::OnStart() {}

void Service::OnConnect(const ConnectSourceInfo& source,
                        const std::string& interface_name,
                        mojo::ScopedMessagePipeHandle receiver_pipe) {
  OnBindInterface(source, interface_name, std::move(receiver_pipe));
}

void Service::OnBindInterface(const BindSourceInfo& source,
                              const std::string& interface_name,
                              mojo::ScopedMessagePipeHandle interface_pipe) {}

void Service::CreatePackagedServiceInstance(
    const std::string& service_name,
    mojo::PendingReceiver<mojom::Service> service_receiver,
    CreatePackagedServiceInstanceCallback callback) {
  std::move(callback).Run(std::nullopt);
}

void Service::OnDisconnected() {
  Terminate();
}

bool Service::OnServiceManagerConnectionLost() {
  return true;
}

void Service::RunUntilTermination() {
  base::RunLoop loop;
  set_termination_closure(loop.QuitClosure());
  loop.Run();
}

void Service::Terminate() {
  if (termination_closure_)
    std::move(termination_closure_).Run();
}

}  // namespace service_manager
