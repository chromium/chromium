// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <list>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_executor.h"
#include "base/threading/simple_thread.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_executable/service_main.h"
#include "services/service_manager/public/cpp/service_receiver.h"
#include "services/service_manager/tests/connect/connect.test-mojom.h"

// Tests that multiple services can be packaged in a single service by
// implementing ServiceFactory; that these services can be specified by
// the package's manifest and are thus registered with the PackageManager.

namespace service_manager {
namespace {

void QuitLoop(base::RunLoop* loop,
              mojom::ConnectResult* out_result,
              std::optional<Identity>* out_resolved_identity,
              mojom::ConnectResult result,
              const std::optional<Identity>& resolved_identity) {
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
  ProvidedService(const std::string& title,
                  mojo::PendingReceiver<mojom::Service> receiver)
      : base::SimpleThread(title),
        title_(title),
        receiver_(std::move(receiver)) {
    Start();
  }

  ProvidedService(const ProvidedService&) = delete;
  ProvidedService& operator=(const ProvidedService&) = delete;

  ~ProvidedService() override {
    Join();
  }

 private:
  // service_manager::Service:
  void OnStart() override {
    receivers_.set_disconnect_handler(base::BindRepeating(
        &ProvidedService::OnMojoDisconnect, base::Unretained(this)));
    registry_.AddInterface<test::mojom::ConnectTestService>(
        base::BindRepeating(&ProvidedService::BindConnectTestServiceReceiver,
                            base::Unretained(this)));
    registry_.AddInterface<test::mojom::BlockedInterface>(
        base::BindRepeating(&ProvidedService::BindBlockedInterfaceReceiver,
                            base::Unretained(this)));
    registry_.AddInterface<test::mojom::AlwaysAllowedInterface>(
        base::BindRepeating(
            &ProvidedService::BindAlwaysAllowedInterfaceReceiver,
            base::Unretained(this)));
    registry_.AddInterface(base::BindRepeating(
        &ProvidedService::BindIdentityTestReceiver, base::Unretained(this)));
  }

  void OnBindInterface(const BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override {
    registry_.BindInterface(interface_name, std::move(interface_pipe),
                            source_info);
  }

  void OnDisconnected() override {
    service_receiver_.Close();
    run_loop_->Quit();
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

    service_receiver_.GetConnector()->Connect(
        source_info.identity, caller_.BindNewPipeAndPassReceiver());
    caller_->ConnectionAccepted(std::move(state));
  }

  void BindBlockedInterfaceReceiver(
      mojo::PendingReceiver<test::mojom::BlockedInterface> receiver,
      const BindSourceInfo& source_info) {
    blocked_receivers_.Add(this, std::move(receiver));
  }

  void BindAlwaysAllowedInterfaceReceiver(
      mojo::PendingReceiver<test::mojom::AlwaysAllowedInterface> receiver,
      const BindSourceInfo& source_info) {
    always_allowed_receivers_.Add(this, std::move(receiver));
  }

  void BindIdentityTestReceiver(
      mojo::PendingReceiver<test::mojom::IdentityTest> receiver,
      const BindSourceInfo& source_info) {
    identity_test_receivers_.Add(this, std::move(receiver));
  }

  // test::mojom::ConnectTestService:
  void GetTitle(GetTitleCallback callback) override {
    std::move(callback).Run(title_);
  }

  void GetInstanceId(GetInstanceIdCallback callback) override {
    std::move(callback).Run(service_receiver_.identity().instance_id());
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
    std::optional<Identity> resolved_identity;
    base::RunLoop loop(base::RunLoop::Type::kNestableTasksAllowed);
    service_receiver_.GetConnector()->WarmService(
        filter, base::BindOnce(&QuitLoop, &loop, &result, &resolved_identity));
    loop.Run();
    std::move(callback).Run(static_cast<int32_t>(result), resolved_identity);
  }

  // base::SimpleThread:
  void Run() override {
    base::SingleThreadTaskExecutor main_task_executor;
    base::RunLoop run_loop;
    run_loop_ = &run_loop;
    service_receiver_.Bind(std::move(receiver_));
    run_loop.Run();
    run_loop_ = nullptr;

    caller_.reset();
    receivers_.Clear();
    blocked_receivers_.Clear();
    always_allowed_receivers_.Clear();
    identity_test_receivers_.Clear();
  }

  void OnMojoDisconnect() {
    if (receivers_.empty()) {
      if (service_receiver_.is_bound())
        service_receiver_.Close();
      run_loop_->Quit();
    }
  }

  raw_ptr<base::RunLoop> run_loop_;
  service_manager::ServiceReceiver service_receiver_{this};
  const std::string title_;
  mojo::PendingReceiver<mojom::Service> receiver_;
  mojo::Remote<test::mojom::ExposedInterface> caller_;
  BinderRegistryWithArgs<const BindSourceInfo&> registry_;
  mojo::ReceiverSet<test::mojom::ConnectTestService> receivers_;
  mojo::ReceiverSet<test::mojom::BlockedInterface> blocked_receivers_;
  mojo::ReceiverSet<test::mojom::AlwaysAllowedInterface>
      always_allowed_receivers_;
  mojo::ReceiverSet<test::mojom::IdentityTest> identity_test_receivers_;
};

class ConnectTestService : public Service,
                           public test::mojom::ConnectTestService {
 public:
  explicit ConnectTestService(
      mojo::PendingReceiver<service_manager::mojom::Service> receiver)
      : service_receiver_(this, std::move(receiver)) {}

  ConnectTestService(const ConnectTestService&) = delete;
  ConnectTestService& operator=(const ConnectTestService&) = delete;

  ~ConnectTestService() override = default;

 private:
  // service_manager::Service:
  void OnStart() override {
    base::RepeatingClosure disconnect_handler = base::BindRepeating(
        &ConnectTestService::OnMojoDisconnect, base::Unretained(this));
    receivers_.set_disconnect_handler(disconnect_handler);
    registry_.AddInterface<test::mojom::ConnectTestService>(
        base::BindRepeating(&ConnectTestService::BindConnectTestServiceReceiver,
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
    if (service_name == "connect_test_a") {
      provided_services_.emplace_back(
          std::make_unique<ProvidedService>("A", std::move(receiver)));
    } else if (service_name == "connect_test_b") {
      provided_services_.emplace_back(
          std::make_unique<ProvidedService>("B", std::move(receiver)));
    }
    std::move(callback).Run(base::GetCurrentProcId());
  }

  void OnDisconnected() override {
    provided_services_.clear();
    Terminate();
  }

  void BindConnectTestServiceReceiver(
      mojo::PendingReceiver<test::mojom::ConnectTestService> receiver) {
    receivers_.Add(this, std::move(receiver));
  }

  // test::mojom::ConnectTestService:
  void GetTitle(GetTitleCallback callback) override {
    std::move(callback).Run("ROOT");
  }

  void GetInstanceId(GetInstanceIdCallback callback) override {
    std::move(callback).Run(service_receiver_.identity().instance_id());
  }

  void OnMojoDisconnect() {
    if (receivers_.empty())
      service_receiver_.RequestClose();
  }

  service_manager::ServiceReceiver service_receiver_;
  std::vector<std::unique_ptr<Service>> delegates_;
  BinderRegistry registry_;
  mojo::ReceiverSet<test::mojom::ConnectTestService> receivers_;
  std::list<std::unique_ptr<ProvidedService>> provided_services_;
};

}  // namespace service_manager

void ServiceMain(
    mojo::PendingReceiver<service_manager::mojom::Service> receiver) {
  base::SingleThreadTaskExecutor main_task_executor;
  service_manager::ConnectTestService(std::move(receiver))
      .RunUntilTermination();
}
