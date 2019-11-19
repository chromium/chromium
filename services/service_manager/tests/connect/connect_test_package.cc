// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_executor.h"
#include "base/threading/simple_thread.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_binding.h"
#include "services/service_manager/public/cpp/service_executable/service_main.h"
#include "services/service_manager/tests/connect/connect.test-mojom.h"

// Tests that multiple services can be packaged in a single service by
// implementing ServiceFactory; that these services can be specified by
// the package's manifest and are thus registered with the PackageManager.

namespace service_manager {
namespace {

void QuitLoop(base::RunLoop* loop,
              mojom::ConnectResult* out_result,
              base::Optional<Identity>* out_resolved_identity,
              mojom::ConnectResult result,
              const base::Optional<Identity>& resolved_identity) {
  loop->Quit();
  *out_result = result;
  *out_resolved_identity = resolved_identity;
}

}  // namespace

using GetTitleCallback = test::mojom::ConnectTestService::GetTitleCallback;

class ProvidedService : public Service,
                        public test::mojom::ConnectTestService,
                        public test::mojom::BlockedInterface,
                        public test::mojom::AlwaysAllowedInterface,
                        public test::mojom::IdentityTest,
                        public base::SimpleThread {
 public:
  ProvidedService(const std::string& title, mojom::ServiceRequest request)
      : base::SimpleThread(title),
        title_(title),
        request_(std::move(request)) {
    Start();
  }

  ~ProvidedService() override {
    Join();
  }

 private:
  // service_manager::Service:
  void OnStart() override {
    bindings_.set_connection_error_handler(base::BindRepeating(
        &ProvidedService::OnConnectionError, base::Unretained(this)));
    registry_.AddInterface<test::mojom::ConnectTestService>(
        base::BindRepeating(&ProvidedService::BindConnectTestServiceRequest,
                            base::Unretained(this)));
    registry_.AddInterface<test::mojom::BlockedInterface>(base::BindRepeating(
        &ProvidedService::BindBlockedInterfaceRequest, base::Unretained(this)));
    registry_.AddInterface<test::mojom::AlwaysAllowedInterface>(
        base::BindRepeating(&ProvidedService::BindAlwaysAllowedInterfaceRequest,
                            base::Unretained(this)));
    registry_.AddInterface(base::BindRepeating(
        &ProvidedService::BindIdentityTestRequest, base::Unretained(this)));
  }

  void OnBindInterface(const BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override {
    registry_.BindInterface(interface_name, std::move(interface_pipe),
                            source_info);
  }

  void OnDisconnected() override {
    service_binding_.Close();
    run_loop_->Quit();
  }

  void BindConnectTestServiceRequest(
      test::mojom::ConnectTestServiceRequest request,
      const BindSourceInfo& source_info) {
    bindings_.AddBinding(this, std::move(request));
    test::mojom::ConnectionStatePtr state(test::mojom::ConnectionState::New());
    state->connection_remote_name = source_info.identity.name();
    state->connection_remote_instance_group =
        source_info.identity.instance_group();
    state->initialize_local_name = service_binding_.identity().name();
    state->initialize_local_instance_group =
        service_binding_.identity().instance_group();

    service_binding_.GetConnector()->BindInterface(source_info.identity,
                                                   &caller_);
    caller_->ConnectionAccepted(std::move(state));
  }

  void BindBlockedInterfaceRequest(test::mojom::BlockedInterfaceRequest request,
                                   const BindSourceInfo& source_info) {
    blocked_bindings_.AddBinding(this, std::move(request));
  }

  void BindAlwaysAllowedInterfaceRequest(
      test::mojom::AlwaysAllowedInterfaceRequest request,
      const BindSourceInfo& source_info) {
    always_allowed_bindings_.AddBinding(this, std::move(request));
  }

  void BindIdentityTestRequest(test::mojom::IdentityTestRequest request,
                               const BindSourceInfo& source_info) {
    identity_test_bindings_.AddBinding(this, std::move(request));
  }

  // test::mojom::ConnectTestService:
  void GetTitle(GetTitleCallback callback) override {
    std::move(callback).Run(title_);
  }

  void GetInstanceId(GetInstanceIdCallback callback) override {
    std::move(callback).Run(service_binding_.identity().instance_id());
  }

  // test::mojom::BlockedInterface:
  void GetTitleBlocked(GetTitleBlockedCallback callback) override {
    std::move(callback).Run("Called Blocked Interface!");
  }

  // test::mojom::AlwaysAllowedInterface:
  void GetTitleAlwaysAllowed(GetTitleAlwaysAllowedCallback callback) override {
    std::move(callback).Run("always_allowed");
  }

  // test::mojom::IdentityTest:
  void ConnectToClassAppWithFilter(
      const service_manager::ServiceFilter& filter,
      ConnectToClassAppWithFilterCallback callback) override {
    mojom::ConnectResult result;
    base::Optional<Identity> resolved_identity;
    base::RunLoop loop(base::RunLoop::Type::kNestableTasksAllowed);
    service_binding_.GetConnector()->WarmService(
        filter, base::BindOnce(&QuitLoop, &loop, &result, &resolved_identity));
    loop.Run();
    std::move(callback).Run(static_cast<int32_t>(result), resolved_identity);
  }

  // base::SimpleThread:
  void Run() override {
    base::SingleThreadTaskExecutor main_task_executor;
    base::RunLoop run_loop;
    run_loop_ = &run_loop;
    service_binding_.Bind(std::move(request_));
    run_loop.Run();
    run_loop_ = nullptr;

    caller_.reset();
    bindings_.CloseAllBindings();
    blocked_bindings_.CloseAllBindings();
    always_allowed_bindings_.CloseAllBindings();
    identity_test_bindings_.CloseAllBindings();
  }

  void OnConnectionError() {
    if (bindings_.empty()) {
      if (service_binding_.is_bound())
        service_binding_.Close();
      run_loop_->Quit();
    }
  }

  base::RunLoop* run_loop_;
  service_manager::ServiceBinding service_binding_{this};
  const std::string title_;
  mojom::ServiceRequest request_;
  test::mojom::ExposedInterfacePtr caller_;
  BinderRegistryWithArgs<const BindSourceInfo&> registry_;
  mojo::BindingSet<test::mojom::ConnectTestService> bindings_;
  mojo::BindingSet<test::mojom::BlockedInterface> blocked_bindings_;
  mojo::BindingSet<test::mojom::AlwaysAllowedInterface>
      always_allowed_bindings_;
  mojo::BindingSet<test::mojom::IdentityTest> identity_test_bindings_;

  DISALLOW_COPY_AND_ASSIGN(ProvidedService);
};

class ConnectTestService : public Service,
                           public test::mojom::ConnectTestService {
 public:
  explicit ConnectTestService(service_manager::mojom::ServiceRequest request)
      : service_binding_(this, std::move(request)) {}
  ~ConnectTestService() override = default;

 private:
  // service_manager::Service:
  void OnStart() override {
    base::RepeatingClosure error_handler = base::BindRepeating(
        &ConnectTestService::OnConnectionError, base::Unretained(this));
    bindings_.set_connection_error_handler(error_handler);
    registry_.AddInterface<test::mojom::ConnectTestService>(
        base::BindRepeating(&ConnectTestService::BindConnectTestServiceRequest,
                            base::Unretained(this)));
  }

  void OnBindInterface(const BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override {
    registry_.BindInterface(interface_name, std::move(interface_pipe));
  }

  void CreatePackagedServiceInstance(
      const std::string& service_name,
      mojo::PendingReceiver<mojom::Service> receiver,
      CreatePackagedServiceInstanceCallback callback) override {
    service_manager::mojom::ServiceRequest request(std::move(receiver));
    if (service_name == "connect_test_a") {
      provided_services_.emplace_back(
          std::make_unique<ProvidedService>("A", std::move(request)));
    } else if (service_name == "connect_test_b") {
      provided_services_.emplace_back(
          std::make_unique<ProvidedService>("B", std::move(request)));
    }
    std::move(callback).Run(base::GetCurrentProcId());
  }

  void OnDisconnected() override {
    provided_services_.clear();
    Terminate();
  }

  void BindConnectTestServiceRequest(
      test::mojom::ConnectTestServiceRequest request) {
    bindings_.AddBinding(this, std::move(request));
  }

  // test::mojom::ConnectTestService:
  void GetTitle(GetTitleCallback callback) override {
    std::move(callback).Run("ROOT");
  }

  void GetInstanceId(GetInstanceIdCallback callback) override {
    std::move(callback).Run(service_binding_.identity().instance_id());
  }

  void OnConnectionError() {
    if (bindings_.empty())
      service_binding_.RequestClose();
  }

  service_manager::ServiceBinding service_binding_;
  std::vector<std::unique_ptr<Service>> delegates_;
  BinderRegistry registry_;
  mojo::BindingSet<test::mojom::ConnectTestService> bindings_;
  std::list<std::unique_ptr<ProvidedService>> provided_services_;

  DISALLOW_COPY_AND_ASSIGN(ConnectTestService);
};

}  // namespace service_manager

void ServiceMain(service_manager::mojom::ServiceRequest request) {
  base::SingleThreadTaskExecutor main_task_executor;
  service_manager::ConnectTestService(std::move(request)).RunUntilTermination();
}
