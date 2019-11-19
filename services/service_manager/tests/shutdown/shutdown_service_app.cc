// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/macros.h"
#include "base/task/single_thread_task_executor.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_binding.h"
#include "services/service_manager/public/cpp/service_executable/service_main.h"
#include "services/service_manager/public/mojom/service.mojom.h"
#include "services/service_manager/tests/shutdown/shutdown.test-mojom.h"

namespace service_manager {
namespace {

class ShutdownServiceApp : public Service, public mojom::ShutdownTestService {
 public:
  explicit ShutdownServiceApp(mojom::ServiceRequest request)
      : service_binding_(this, std::move(request)) {
    registry_.AddInterface<mojom::ShutdownTestService>(base::BindRepeating(
        &ShutdownServiceApp::Create, base::Unretained(this)));
  }

  ~ShutdownServiceApp() override = default;

 private:
  // Service:
  void OnBindInterface(const BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override {
    registry_.BindInterface(interface_name, std::move(interface_pipe));
  }

  // mojom::ShutdownTestService:
  void SetClient(mojom::ShutdownTestClientPtr client) override {}
  void ShutDown() override { Terminate(); }

  void Create(mojom::ShutdownTestServiceRequest request) {
    bindings_.AddBinding(this, std::move(request));
  }

  ServiceBinding service_binding_;
  BinderRegistry registry_;
  mojo::BindingSet<mojom::ShutdownTestService> bindings_;

  DISALLOW_COPY_AND_ASSIGN(ShutdownServiceApp);
};

}  // namespace
}  // namespace service_manager

void ServiceMain(service_manager::mojom::ServiceRequest request) {
  base::SingleThreadTaskExecutor main_task_executor;
  service_manager::ShutdownServiceApp(std::move(request)).RunUntilTermination();
}
