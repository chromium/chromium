// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/macros.h"
#include "base/message_loop/message_loop_current.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_executor.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_binding.h"
#include "services/service_manager/public/mojom/connector.mojom.h"
#include "services/service_manager/public/mojom/service.mojom.h"
#include "services/service_manager/tests/connect/connect.test-mojom.h"

namespace service_manager {

namespace {

void OnConnectResult(base::OnceClosure closure,
                     mojom::ConnectResult* out_result,
                     base::Optional<Identity>* out_resolved_identity,
                     mojom::ConnectResult result,
                     const base::Optional<Identity>& resolved_identity) {
  std::move(closure).Run();
  *out_result = result;
  *out_resolved_identity = resolved_identity;
}

void OnResponseString(std::string* string,
                      base::OnceClosure closure,
                      const std::string& response) {
  *string = response;
  std::move(closure).Run();
}

}  // namespace

class ConnectTestApp : public Service,
                       public test::mojom::ConnectTestService,
                       public test::mojom::StandaloneApp,
                       public test::mojom::BlockedInterface,
                       public test::mojom::IdentityTest {
 public:
  explicit ConnectTestApp(mojom::ServiceRequest request)
      : service_binding_(this, std::move(request)) {}

  ~ConnectTestApp() override = default;

 private:
  // Service:
  void OnStart() override {
    bindings_.set_connection_error_handler(base::BindRepeating(
        &ConnectTestApp::OnConnectionError, base::Unretained(this)));
    standalone_bindings_.set_connection_error_handler(base::BindRepeating(
        &ConnectTestApp::OnConnectionError, base::Unretained(this)));
    registry_.AddInterface<test::mojom::ConnectTestService>(
        base::BindRepeating(&ConnectTestApp::BindConnectTestServiceRequest,
                            base::Unretained(this)));
    registry_.AddInterface<test::mojom::StandaloneApp>(base::BindRepeating(
        &ConnectTestApp::BindStandaloneAppRequest, base::Unretained(this)));
    registry_.AddInterface<test::mojom::BlockedInterface>(base::BindRepeating(
        &ConnectTestApp::BindBlockedInterfaceRequest, base::Unretained(this)));
    registry_.AddInterface<test::mojom::IdentityTest>(base::BindRepeating(
        &ConnectTestApp::BindIdentityTestRequest, base::Unretained(this)));
  }

  void OnBindInterface(const BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override {
    registry_.BindInterface(interface_name, std::move(interface_pipe),
                            source_info);
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

  void BindStandaloneAppRequest(test::mojom::StandaloneAppRequest request,
                                const BindSourceInfo& source_info) {
    standalone_bindings_.AddBinding(this, std::move(request));
  }

  void BindBlockedInterfaceRequest(test::mojom::BlockedInterfaceRequest request,
                                   const BindSourceInfo& source_info) {
    blocked_bindings_.AddBinding(this, std::move(request));
  }

  void BindIdentityTestRequest(test::mojom::IdentityTestRequest request,
                               const BindSourceInfo& source_info) {
    identity_test_bindings_.AddBinding(this, std::move(request));
  }

  // test::mojom::ConnectTestService:
  void GetTitle(GetTitleCallback callback) override {
    std::move(callback).Run("APP");
  }

  void GetInstanceId(GetInstanceIdCallback callback) override {
    std::move(callback).Run(service_binding_.identity().instance_id());
  }

  // test::mojom::StandaloneApp:
  void ConnectToAllowedAppInBlockedPackage(
      ConnectToAllowedAppInBlockedPackageCallback callback) override {
    base::RunLoop run_loop{base::RunLoop::Type::kNestableTasksAllowed};
    test::mojom::ConnectTestServicePtr test_service;
    service_binding_.GetConnector()->BindInterface("connect_test_a",
                                                   &test_service);
    test_service.set_connection_error_handler(base::BindRepeating(
        &ConnectTestApp::OnGotTitle, base::Unretained(this),
        base::Unretained(&callback), run_loop.QuitClosure(), "uninitialized"));
    test_service->GetTitle(
        base::BindOnce(&ConnectTestApp::OnGotTitle, base::Unretained(this),
                       base::Unretained(&callback), run_loop.QuitClosure()));

    // This message is dispatched as a task on the same run loop, so we need
    // to allow nesting in order to pump additional signals.
    run_loop.Run();
  }

  void ConnectToClassInterface(
      ConnectToClassInterfaceCallback callback) override {
    test::mojom::ClassInterfacePtr class_interface;
    service_binding_.GetConnector()->BindInterface("connect_test_class_app",
                                                   &class_interface);
    std::string ping_response;
    {
      base::RunLoop loop{base::RunLoop::Type::kNestableTasksAllowed};
      class_interface->Ping(base::BindOnce(&OnResponseString, &ping_response,
                                           loop.QuitClosure()));
      loop.Run();
    }
    test::mojom::ConnectTestServicePtr service;
    service_binding_.GetConnector()->BindInterface("connect_test_class_app",
                                                   &service);
    std::string title_response;
    {
      base::RunLoop loop{base::RunLoop::Type::kNestableTasksAllowed};
      service->GetTitle(base::BindOnce(&OnResponseString, &title_response,
                                       loop.QuitClosure()));
      loop.Run();
    }
    std::move(callback).Run(ping_response, title_response);
  }

  // test::mojom::BlockedInterface:
  void GetTitleBlocked(GetTitleBlockedCallback callback) override {
    std::move(callback).Run("Called Blocked Interface!");
  }

  // test::mojom::IdentityTest:
  void ConnectToClassAppWithFilter(
      const ServiceFilter& filter,
      ConnectToClassAppWithFilterCallback callback) override {
    mojom::ConnectResult result;
    base::Optional<Identity> resolved_identity;
    base::RunLoop loop{base::RunLoop::Type::kNestableTasksAllowed};
    service_binding_.GetConnector()->WarmService(
        filter, base::BindOnce(&OnConnectResult, loop.QuitClosure(), &result,
                               &resolved_identity));
    loop.Run();
    std::move(callback).Run(static_cast<int32_t>(result), resolved_identity);
  }

  void OnGotTitle(ConnectToAllowedAppInBlockedPackageCallback* callback,
                  base::OnceClosure closure,
                  const std::string& title) {
    std::move(*callback).Run(title);
    std::move(closure).Run();
  }

  void OnConnectionError() {
    if (bindings_.empty() && standalone_bindings_.empty())
      Terminate();
  }

  ServiceBinding service_binding_;
  BinderRegistryWithArgs<const BindSourceInfo&> registry_;
  mojo::BindingSet<test::mojom::ConnectTestService> bindings_;
  mojo::BindingSet<test::mojom::StandaloneApp> standalone_bindings_;
  mojo::BindingSet<test::mojom::BlockedInterface> blocked_bindings_;
  mojo::BindingSet<test::mojom::IdentityTest> identity_test_bindings_;
  test::mojom::ExposedInterfacePtr caller_;

  DISALLOW_COPY_AND_ASSIGN(ConnectTestApp);
};

}  // namespace service_manager

void ServiceMain(service_manager::mojom::ServiceRequest request) {
  base::SingleThreadTaskExecutor main_task_executor;
  service_manager::ConnectTestApp(std::move(request)).RunUntilTermination();
}
