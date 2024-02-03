// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/task/current_thread.h"
#include "base/task/single_thread_task_executor.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_receiver.h"
#include "services/service_manager/public/mojom/connector.mojom.h"
#include "services/service_manager/public/mojom/service.mojom.h"
#include "services/service_manager/tests/connect/connect.test-mojom.h"

namespace service_manager {

namespace {

void OnConnectResult(base::OnceClosure closure,
                     mojom::ConnectResult* out_result,
                     std::optional<Identity>* out_resolved_identity,
                     mojom::ConnectResult result,
                     const std::optional<Identity>& resolved_identity) {
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
  explicit ConnectTestApp(mojo::PendingReceiver<mojom::Service> receiver)
      : service_receiver_(this, std::move(receiver)) {}

  ConnectTestApp(const ConnectTestApp&) = delete;
  ConnectTestApp& operator=(const ConnectTestApp&) = delete;

  ~ConnectTestApp() override = default;

 private:
  // Service:
  void OnStart() override {
    receivers_.set_disconnect_handler(base::BindRepeating(
        &ConnectTestApp::OnMojoDisconnect, base::Unretained(this)));
    standalone_receivers_.set_disconnect_handler(base::BindRepeating(
        &ConnectTestApp::OnMojoDisconnect, base::Unretained(this)));
    registry_.AddInterface<test::mojom::ConnectTestService>(
        base::BindRepeating(&ConnectTestApp::BindConnectTestServiceReceiver,
                            base::Unretained(this)));
    registry_.AddInterface<test::mojom::StandaloneApp>(base::BindRepeating(
        &ConnectTestApp::BindStandaloneAppreceiver, base::Unretained(this)));
    registry_.AddInterface<test::mojom::BlockedInterface>(base::BindRepeating(
        &ConnectTestApp::BindBlockedInterfacereceiver, base::Unretained(this)));
    registry_.AddInterface<test::mojom::IdentityTest>(base::BindRepeating(
        &ConnectTestApp::BindIdentityTestreceiver, base::Unretained(this)));
  }

  void OnBindInterface(const BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override {
    registry_.BindInterface(interface_name, std::move(interface_pipe),
                            source_info);
  }

  void BindConnectTestServiceReceiver(
      mojo::PendingReceiver<test::mojom::ConnectTestService> receiver,
      const BindSourceInfo& source_info) {
    receivers_.Add(this, std::move(receiver));
    test::mojom::ConnectionStatePtr state(test::mojom::ConnectionState::New());
    state->connection_remote_name = source_info.identity.name();
    state->connection_remote_instance_group =
        source_info.identity.instance_group();
    state->initialize_local_name = service_receiver_.identity().name();
    state->initialize_local_instance_group =
        service_receiver_.identity().instance_group();

    caller_.reset();
    service_receiver_.GetConnector()->Connect(
        source_info.identity, caller_.BindNewPipeAndPassReceiver());
    caller_->ConnectionAccepted(std::move(state));
  }

  void BindStandaloneAppreceiver(
      mojo::PendingReceiver<test::mojom::StandaloneApp> receiver,
      const BindSourceInfo& source_info) {
    standalone_receivers_.Add(this, std::move(receiver));
  }

  void BindBlockedInterfacereceiver(
      mojo::PendingReceiver<test::mojom::BlockedInterface> receiver,
      const BindSourceInfo& source_info) {
    blocked_receivers_.Add(this, std::move(receiver));
  }

  void BindIdentityTestreceiver(
      mojo::PendingReceiver<test::mojom::IdentityTest> receiver,
      const BindSourceInfo& source_info) {
    identity_test_receivers_.Add(this, std::move(receiver));
  }

  // test::mojom::ConnectTestService:
  void GetTitle(GetTitleCallback callback) override {
    std::move(callback).Run("APP");
  }

  void GetInstanceId(GetInstanceIdCallback callback) override {
    std::move(callback).Run(service_receiver_.identity().instance_id());
  }

  // test::mojom::StandaloneApp:
  void ConnectToAllowedAppInBlockedPackage(
      ConnectToAllowedAppInBlockedPackageCallback callback) override {
    base::RunLoop run_loop{base::RunLoop::Type::kNestableTasksAllowed};
    mojo::Remote<test::mojom::ConnectTestService> test_service;
    service_receiver_.GetConnector()->Connect(
        "connect_test_a", test_service.BindNewPipeAndPassReceiver());
    test_service.set_disconnect_handler(base::BindOnce(
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
    mojo::Remote<test::mojom::ClassInterface> class_interface;
    service_receiver_.GetConnector()->Connect(
        "connect_test_class_app", class_interface.BindNewPipeAndPassReceiver());
    std::string ping_response;
    {
      base::RunLoop loop{base::RunLoop::Type::kNestableTasksAllowed};
      class_interface->Ping(base::BindOnce(&OnResponseString, &ping_response,
                                           loop.QuitClosure()));
      loop.Run();
    }
    mojo::Remote<test::mojom::ConnectTestService> service;
    service_receiver_.GetConnector()->Connect(
        "connect_test_class_app", service.BindNewPipeAndPassReceiver());
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
    std::optional<Identity> resolved_identity;
    base::RunLoop loop{base::RunLoop::Type::kNestableTasksAllowed};
    service_receiver_.GetConnector()->WarmService(
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

  void OnMojoDisconnect() {
    if (receivers_.empty() && standalone_receivers_.empty())
      Terminate();
  }

  ServiceReceiver service_receiver_;
  BinderRegistryWithArgs<const BindSourceInfo&> registry_;
  mojo::ReceiverSet<test::mojom::ConnectTestService> receivers_;
  mojo::ReceiverSet<test::mojom::StandaloneApp> standalone_receivers_;
  mojo::ReceiverSet<test::mojom::BlockedInterface> blocked_receivers_;
  mojo::ReceiverSet<test::mojom::IdentityTest> identity_test_receivers_;
  mojo::Remote<test::mojom::ExposedInterface> caller_;
};

}  // namespace service_manager

void ServiceMain(
    mojo::PendingReceiver<service_manager::mojom::Service> receiver) {
  base::SingleThreadTaskExecutor main_task_executor;
  service_manager::ConnectTestApp(std::move(receiver)).RunUntilTermination();
}
