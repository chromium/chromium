// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_executor.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_binding.h"
#include "services/service_manager/public/cpp/service_executable/service_main.h"
#include "services/service_manager/tests/shutdown/shutdown.test-mojom.h"

namespace service_manager {

class ShutdownClientApp : public Service,
                          public mojom::ShutdownTestClientController,
                          public mojom::ShutdownTestClient {
 public:
  explicit ShutdownClientApp(mojom::ServiceRequest request)
      : service_binding_(this, std::move(request)) {
    registry_.AddInterface<mojom::ShutdownTestClientController>(
        base::BindRepeating(&ShutdownClientApp::Create,
                            base::Unretained(this)));
  }
  ~ShutdownClientApp() override = default;

 private:
  // service_manager::Service:
  void OnBindInterface(const BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override {
    registry_.BindInterface(interface_name, std::move(interface_pipe));
  }

  void Create(mojom::ShutdownTestClientControllerRequest request) {
    bindings_.AddBinding(this, std::move(request));
  }

  // mojom::ShutdownTestClientController:
  void ConnectAndWait(ConnectAndWaitCallback callback) override {
    mojom::ShutdownTestServicePtr service;
    service_binding_.GetConnector()->BindInterface("shutdown_service",
                                                   &service);

    mojo::Binding<mojom::ShutdownTestClient> client_binding(this);

    mojom::ShutdownTestClientPtr client_ptr;
    client_binding.Bind(mojo::MakeRequest(&client_ptr));

    service->SetClient(std::move(client_ptr));

    base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
    client_binding.set_connection_error_handler(run_loop.QuitClosure());
    run_loop.Run();

    std::move(callback).Run();
  }

  ServiceBinding service_binding_;
  BinderRegistry registry_;
  mojo::BindingSet<mojom::ShutdownTestClientController> bindings_;

  DISALLOW_COPY_AND_ASSIGN(ShutdownClientApp);
};

}  // namespace service_manager

void ServiceMain(service_manager::mojom::ServiceRequest request) {
  base::SingleThreadTaskExecutor main_task_executor;
  service_manager::ShutdownClientApp(std::move(request)).RunUntilTermination();
}
