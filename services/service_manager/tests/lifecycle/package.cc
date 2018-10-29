// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <memory>

#include "base/bind.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/service_manager/public/c/main.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/service_binding.h"
#include "services/service_manager/public/mojom/service_factory.mojom.h"
#include "services/service_manager/tests/lifecycle/app_client.h"
#include "services/service_manager/tests/lifecycle/lifecycle_unittest.mojom.h"

namespace {

class PackagedApp : public service_manager::Service,
                    public service_manager::test::mojom::LifecycleControl {
 public:
  PackagedApp(service_manager::mojom::ServiceRequest request,
              base::OnceClosure service_manager_connection_closed_callback,
              base::OnceClosure destruct_callback)
      : service_binding_(this, std::move(request)),
        service_manager_connection_closed_callback_(
            std::move(service_manager_connection_closed_callback)),
        destruct_callback_(std::move(destruct_callback)) {
    bindings_.set_connection_error_handler(
        base::BindRepeating(&PackagedApp::MaybeQuit, base::Unretained(this)));
    registry_.AddInterface<service_manager::test::mojom::LifecycleControl>(
        base::Bind(&PackagedApp::Create, base::Unretained(this)));
  }

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

  void Create(service_manager::test::mojom::LifecycleControlRequest request) {
    bindings_.AddBinding(this, std::move(request));
  }

  // LifecycleControl:
  void Ping(PingCallback callback) override { std::move(callback).Run(); }

  void GracefulQuit() override { service_binding_.RequestClose(); }

  void Crash() override {
    // When multiple instances are vended from the same package instance, this
    // will cause all instances to be quit.
    exit(1);
  }

  void CloseServiceManagerConnection() override {
    std::move(service_manager_connection_closed_callback_).Run();

    if (service_binding_.is_bound())
      service_binding_.Close();

    // This only closed our relationship with the service manager, existing
    // |bindings_| remain active.
    MaybeQuit();
  }

  void MaybeQuit() {
    if (service_binding_.is_bound() || !bindings_.empty())
      return;

    // Deletes |this|.
    std::move(destruct_callback_).Run();
  }

  service_manager::ServiceBinding service_binding_;

  service_manager::BinderRegistry registry_;
  mojo::BindingSet<service_manager::test::mojom::LifecycleControl> bindings_;

  // Run when this object's connection to the service manager is closed.
  base::OnceClosure service_manager_connection_closed_callback_;
  // Run when this object is destructed.
  base::OnceClosure destruct_callback_;

  DISALLOW_COPY_AND_ASSIGN(PackagedApp);
};

class Package : public service_manager::Service,
                public service_manager::mojom::ServiceFactory {
 public:
  explicit Package(service_manager::mojom::ServiceRequest request,
                   base::OnceClosure quit_closure)
      : service_binding_(this, std::move(request)),
        quit_closure_(std::move(quit_closure)),
        app_client_(service_manager::mojom::ServiceRequest(),
                    base::BindOnce(&Package::QuitFromAppClient,
                                   base::Unretained(this))) {
    registry_.AddInterface<service_manager::mojom::ServiceFactory>(
        base::BindRepeating(&Package::Create, base::Unretained(this)));
  }

  ~Package() override = default;

 private:
  void QuitFromAppClient() { std::move(quit_closure_).Run(); }

  // service_manager::Service:
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override {
    if (!registry_.TryBindInterface(interface_name, &interface_pipe)) {
      app_client_.OnBindInterface(source_info, interface_name,
                                  std::move(interface_pipe));
    }
  }

  void OnDisconnected() override { std::move(quit_closure_).Run(); }

  void Create(service_manager::mojom::ServiceFactoryRequest request) {
    bindings_.AddBinding(this, std::move(request));
  }

  // service_manager::mojom::ServiceFactory:
  void CreateService(
      service_manager::mojom::ServiceRequest request,
      const std::string& name,
      service_manager::mojom::PIDReceiverPtr pid_receiver) override {
    ++service_manager_connection_refcount_;
    int id = next_id_++;
    auto app = std::make_unique<PackagedApp>(
        std::move(request),
        base::BindOnce(&Package::OnAppInstanceDisconnected,
                       base::Unretained(this)),
        base::BindOnce(&Package::DestroyAppInstance, base::Unretained(this),
                       id));
    app_instances_.emplace(id, std::move(app));
  }

  void OnAppInstanceDisconnected() {
    if (--service_manager_connection_refcount_ == 0)
      service_binding_.RequestClose();
  }

  void DestroyAppInstance(int id) {
    app_instances_.erase(id);
    if (app_instances_.empty())
      std::move(quit_closure_).Run();
  }

  service_manager::ServiceBinding service_binding_;
  base::OnceClosure quit_closure_;
  service_manager::test::AppClient app_client_;
  int service_manager_connection_refcount_ = 0;
  service_manager::BinderRegistry registry_;
  mojo::BindingSet<service_manager::mojom::ServiceFactory> bindings_;

  int next_id_ = 0;
  std::map<int, std::unique_ptr<PackagedApp>> app_instances_;

  DISALLOW_COPY_AND_ASSIGN(Package);
};

}  // namespace

MojoResult ServiceMain(MojoHandle service_request_handle) {
  base::MessageLoop message_loop;
  base::RunLoop run_loop;
  Package package(service_manager::mojom::ServiceRequest(mojo::MakeScopedHandle(
                      mojo::MessagePipeHandle(service_request_handle))),
                  run_loop.QuitClosure());
  run_loop.Run();
  return MOJO_RESULT_OK;
}
