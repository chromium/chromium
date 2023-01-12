// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_executor.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_executable/service_main.h"
#include "services/service_manager/public/cpp/service_receiver.h"
#include "services/service_manager/public/mojom/service.mojom.h"
#include "services/service_manager/tests/lifecycle/lifecycle.test-mojom.h"

namespace {

class Parent : public service_manager::Service,
               public service_manager::test::mojom::Parent {
 public:
  explicit Parent(
      mojo::PendingReceiver<service_manager::mojom::Service> receiver)
      : service_receiver_(this, std::move(receiver)) {
    registry_.AddInterface<service_manager::test::mojom::Parent>(
        base::BindRepeating(&Parent::Create, base::Unretained(this)));
  }

  Parent(const Parent&) = delete;
  Parent& operator=(const Parent&) = delete;

  ~Parent() override = default;

 private:
  // Service:
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override {
    registry_.BindInterface(interface_name, std::move(interface_pipe));
  }

  void Create(
      mojo::PendingReceiver<service_manager::test::mojom::Parent> receiver) {
    parent_receivers_.Add(this, std::move(receiver));
  }

  // service_manager::test::mojom::Parent:
  void ConnectToChild(ConnectToChildCallback callback) override {
    mojo::Remote<service_manager::test::mojom::LifecycleControl> lifecycle;
    service_receiver_.GetConnector()->BindInterface(
        "lifecycle_unittest_app", lifecycle.BindNewPipeAndPassReceiver());

    base::RunLoop loop(base::RunLoop::Type::kNestableTasksAllowed);
    lifecycle->Ping(loop.QuitClosure());
    loop.Run();

    std::move(callback).Run();
  }

  void Quit() override { Terminate(); }

  service_manager::ServiceReceiver service_receiver_;
  service_manager::BinderRegistry registry_;
  mojo::ReceiverSet<service_manager::test::mojom::Parent> parent_receivers_;
};

}  // namespace

void ServiceMain(
    mojo::PendingReceiver<service_manager::mojom::Service> receiver) {
  base::SingleThreadTaskExecutor main_task_executor;
  Parent(std::move(receiver)).RunUntilTermination();
}
