// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_executor.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_binding.h"
#include "services/service_manager/public/cpp/service_executable/service_main.h"
#include "services/service_manager/public/mojom/service.mojom.h"
#include "services/service_manager/tests/lifecycle/lifecycle.test-mojom.h"

namespace {

class Parent : public service_manager::Service,
               public service_manager::test::mojom::Parent {
 public:
  explicit Parent(service_manager::mojom::ServiceRequest request)
      : service_binding_(this, std::move(request)) {
    registry_.AddInterface<service_manager::test::mojom::Parent>(
        base::BindRepeating(&Parent::Create, base::Unretained(this)));
  }

  ~Parent() override = default;

 private:
  // Service:
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override {
    registry_.BindInterface(interface_name, std::move(interface_pipe));
  }

  void Create(service_manager::test::mojom::ParentRequest request) {
    parent_bindings_.AddBinding(this, std::move(request));
  }

  // service_manager::test::mojom::Parent:
  void ConnectToChild(ConnectToChildCallback callback) override {
    service_manager::test::mojom::LifecycleControlPtr lifecycle;
    service_binding_.GetConnector()->BindInterface("lifecycle_unittest_app",
                                                   &lifecycle);

    base::RunLoop loop(base::RunLoop::Type::kNestableTasksAllowed);
    lifecycle->Ping(loop.QuitClosure());
    loop.Run();

    std::move(callback).Run();
  }

  void Quit() override { Terminate(); }

  service_manager::ServiceBinding service_binding_;
  service_manager::BinderRegistry registry_;
  mojo::BindingSet<service_manager::test::mojom::Parent> parent_bindings_;

  DISALLOW_COPY_AND_ASSIGN(Parent);
};

}  // namespace

void ServiceMain(service_manager::mojom::ServiceRequest request) {
  base::SingleThreadTaskExecutor main_task_executor;
  Parent(std::move(request)).RunUntilTermination();
}
