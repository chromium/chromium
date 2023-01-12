// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <memory>

#include "base/functional/bind.h"
#include "base/task/single_thread_task_executor.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/service_executable/service_main.h"
#include "services/service_manager/public/cpp/service_receiver.h"
#include "services/service_manager/tests/lifecycle/app_client.h"
#include "services/service_manager/tests/lifecycle/lifecycle.test-mojom.h"

namespace {

class PackagedApp : public service_manager::Service,
                    public service_manager::test::mojom::LifecycleControl {
 public:
  PackagedApp(mojo::PendingReceiver<service_manager::mojom::Service> receiver,
              base::OnceClosure service_manager_connection_closed_callback,
              base::OnceClosure destruct_callback)
      : service_receiver_(this, std::move(receiver)),
        service_manager_connection_closed_callback_(
            std::move(service_manager_connection_closed_callback)),
        destruct_callback_(std::move(destruct_callback)) {
    receivers_.set_disconnect_handler(
        base::BindRepeating(&PackagedApp::MaybeQuit, base::Unretained(this)));
    registry_.AddInterface<service_manager::test::mojom::LifecycleControl>(
        base::BindRepeating(&PackagedApp::Create, base::Unretained(this)));
  }

  PackagedApp(const PackagedApp&) = delete;
  PackagedApp& operator=(const PackagedApp&) = delete;

  ~PackagedApp() override = default;

 private:
  // service_manager::Service:
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override {
    registry_.BindInterface(interface_name, std::move(interface_pipe));
  }

  void OnDisconnected() override {
    std::move(service_manager_connection_closed_callback_).Run();
    std::move(destruct_callback_).Run();
  }

  void Create(
      mojo::PendingReceiver<service_manager::test::mojom::LifecycleControl>
          receiver) {
    receivers_.Add(this, std::move(receiver));
  }

  // LifecycleControl:
  void Ping(PingCallback callback) override { std::move(callback).Run(); }

  void GracefulQuit() override { service_receiver_.RequestClose(); }

  void Crash() override {
    // When multiple instances are vended from the same package instance, this
    // will cause all instances to be quit.
    exit(1);
  }

  void CloseServiceManagerConnection() override {
    std::move(service_manager_connection_closed_callback_).Run();

    if (service_receiver_.is_bound())
      service_receiver_.Close();

    // This only closed our relationship with the service manager, existing
    // |receivers_| remain active.
    MaybeQuit();
  }

  void MaybeQuit() {
    if (service_receiver_.is_bound() || !receivers_.empty())
      return;

    // Deletes |this|.
    std::move(destruct_callback_).Run();
  }

  service_manager::ServiceReceiver service_receiver_;

  service_manager::BinderRegistry registry_;
  mojo::ReceiverSet<service_manager::test::mojom::LifecycleControl> receivers_;

  // Run when this object's connection to the service manager is closed.
  base::OnceClosure service_manager_connection_closed_callback_;
  // Run when this object is destructed.
  base::OnceClosure destruct_callback_;
};

class Package : public service_manager::Service {
 public:
  explicit Package(
      mojo::PendingReceiver<service_manager::mojom::Service> receiver)
      : service_receiver_(this, std::move(receiver)),
        app_client_(mojo::NullReceiver()) {
    app_client_.set_termination_closure(
        base::BindOnce(&Package::Terminate, base::Unretained(this)));
  }

  Package(const Package&) = delete;
  Package& operator=(const Package&) = delete;

  ~Package() override = default;

 private:
  // service_manager::Service:
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override {
    app_client_.OnBindInterface(source_info, interface_name,
                                std::move(interface_pipe));
  }

  void CreatePackagedServiceInstance(
      const std::string& service_name,
      mojo::PendingReceiver<service_manager::mojom::Service> receiver,
      CreatePackagedServiceInstanceCallback callback) override {
    ++service_manager_connection_refcount_;
    int id = next_id_++;
    auto app = std::make_unique<PackagedApp>(
        std::move(receiver),
        base::BindOnce(&Package::OnAppInstanceDisconnected,
                       base::Unretained(this)),
        base::BindOnce(&Package::DestroyAppInstance, base::Unretained(this),
                       id));
    app_instances_.emplace(id, std::move(app));
    std::move(callback).Run(base::GetCurrentProcId());
  }

  void OnAppInstanceDisconnected() {
    if (--service_manager_connection_refcount_ == 0)
      service_receiver_.RequestClose();
  }

  void DestroyAppInstance(int id) {
    app_instances_.erase(id);
    if (app_instances_.empty())
      Terminate();
  }

  service_manager::ServiceReceiver service_receiver_;
  service_manager::test::AppClient app_client_;
  int service_manager_connection_refcount_ = 0;

  int next_id_ = 0;
  std::map<int, std::unique_ptr<PackagedApp>> app_instances_;
};

}  // namespace

void ServiceMain(
    mojo::PendingReceiver<service_manager::mojom::Service> receiver) {
  base::SingleThreadTaskExecutor main_task_executor;
  Package(std::move(receiver)).RunUntilTermination();
}
