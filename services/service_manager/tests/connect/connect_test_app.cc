// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/guid.h"
#include "base/macros.h"
#include "base/message_loop/message_loop_current.h"
#include "base/run_loop.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/service_manager/public/c/main.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_context.h"
#include "services/service_manager/public/cpp/service_runner.h"
#include "services/service_manager/public/mojom/connector.mojom.h"
#include "services/service_manager/tests/connect/connect_test.mojom.h"

namespace service_manager {

namespace {

void OnConnectResult(base::OnceClosure closure,
                     mojom::ConnectResult* out_result,
                     Identity* out_resolved_identity,
                     mojom::ConnectResult result,
                     const Identity& resolved_identity) {
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

using GetTitleCallback = test::mojom::ConnectTestService::GetTitleCallback;

class ConnectTestApp : public Service,
                       public test::mojom::ConnectTestService,
                       public test::mojom::StandaloneApp,
                       public test::mojom::BlockedInterface,
                       public test::mojom::IdentityTest {
 public:
  ConnectTestApp() {}
  ~ConnectTestApp() override {}

 private:
  // service_manager::Service:
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

  // InterfaceFactory<test::mojom::ConnectTestService>:
  void BindConnectTestServiceRequest(
      test::mojom::ConnectTestServiceRequest request,
      const BindSourceInfo& source_info) {
    bindings_.AddBinding(this, std::move(request));
    test::mojom::ConnectionStatePtr state(test::mojom::ConnectionState::New());
    state->connection_remote_name = source_info.identity.name();
    state->connection_remote_userid = source_info.identity.user_id();
    state->initialize_local_name = context()->identity().name();
    state->initialize_userid = context()->identity().user_id();

    context()->connector()->BindInterface(source_info.identity, &caller_);
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
  void GetInstance(GetInstanceCallback callback) override {
    std::move(callback).Run(context()->identity().instance());
  }

  // test::mojom::StandaloneApp:
  void ConnectToAllowedAppInBlockedPackage(
      ConnectToAllowedAppInBlockedPackageCallback callback) override {
    base::RunLoop run_loop;
    test::mojom::ConnectTestServicePtr test_service;
    context()->connector()->BindInterface("connect_test_a", &test_service);
    test_service.set_connection_error_handler(base::BindRepeating(
        &ConnectTestApp::OnGotTitle, base::Unretained(this),
        base::Unretained(&callback), run_loop.QuitClosure(), "uninitialized"));
    test_service->GetTitle(
        base::BindOnce(&ConnectTestApp::OnGotTitle, base::Unretained(this),
                       base::Unretained(&callback), run_loop.QuitClosure()));
    {
      // This message is dispatched as a task on the same run loop, so we need
      // to allow nesting in order to pump additional signals.
      base::MessageLoopCurrent::ScopedNestableTaskAllower allow;
      run_loop.Run();
    }
  }
  void ConnectToClassInterface(
      ConnectToClassInterfaceCallback callback) override {
    test::mojom::ClassInterfacePtr class_interface;
    context()->connector()->BindInterface("connect_test_class_app",
                                          &class_interface);
    std::string ping_response;
    {
      base::RunLoop loop;
      class_interface->Ping(base::BindOnce(&OnResponseString, &ping_response,
                                           loop.QuitClosure()));
      base::MessageLoopCurrent::ScopedNestableTaskAllower allow;
      loop.Run();
    }
    test::mojom::ConnectTestServicePtr service;
    context()->connector()->BindInterface("connect_test_class_app", &service);
    std::string title_response;
    {
      base::RunLoop loop;
      service->GetTitle(base::BindOnce(&OnResponseString, &title_response,
                                       loop.QuitClosure()));
      base::MessageLoopCurrent::ScopedNestableTaskAllower allow;
      loop.Run();
    }
    std::move(callback).Run(ping_response, title_response);
  }

  // test::mojom::BlockedInterface:
  void GetTitleBlocked(GetTitleBlockedCallback callback) override {
    std::move(callback).Run("Called Blocked Interface!");
  }

  // test::mojom::IdentityTest:
  void ConnectToClassAppWithIdentity(
      const service_manager::Identity& target,
      ConnectToClassAppWithIdentityCallback callback) override {
    context()->connector()->StartService(target);
    mojom::ConnectResult result;
    Identity resolved_identity;
    {
      base::RunLoop loop;
      Connector::TestApi test_api(context()->connector());
      test_api.SetStartServiceCallback(base::BindRepeating(
          &OnConnectResult, loop.QuitClosure(), &result, &resolved_identity));
      base::MessageLoopCurrent::ScopedNestableTaskAllower allow;
      loop.Run();
    }
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
      context()->QuitNow();
  }

  BinderRegistryWithArgs<const BindSourceInfo&> registry_;
  mojo::BindingSet<test::mojom::ConnectTestService> bindings_;
  mojo::BindingSet<test::mojom::StandaloneApp> standalone_bindings_;
  mojo::BindingSet<test::mojom::BlockedInterface> blocked_bindings_;
  mojo::BindingSet<test::mojom::IdentityTest> identity_test_bindings_;
  test::mojom::ExposedInterfacePtr caller_;

  DISALLOW_COPY_AND_ASSIGN(ConnectTestApp);
};

}  // namespace service_manager

MojoResult ServiceMain(MojoHandle service_request_handle) {
  service_manager::ServiceRunner runner(new service_manager::ConnectTestApp);
  return runner.Run(service_request_handle);
}
