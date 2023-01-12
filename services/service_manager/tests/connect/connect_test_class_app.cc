// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_executor.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_executable/service_main.h"
#include "services/service_manager/public/cpp/service_keepalive.h"
#include "services/service_manager/public/cpp/service_receiver.h"
#include "services/service_manager/public/mojom/connector.mojom.h"
#include "services/service_manager/public/mojom/service.mojom.h"
#include "services/service_manager/tests/connect/connect.test-mojom.h"

namespace service_manager {

class ConnectTestClassApp : public Service,
                            public test::mojom::ConnectTestService,
                            public test::mojom::ClassInterface {
 public:
  explicit ConnectTestClassApp(mojo::PendingReceiver<mojom::Service> receiver)
      : service_receiver_(this, std::move(receiver)),
        service_keepalive_(&service_receiver_, base::TimeDelta()) {
    receivers_.set_disconnect_handler(base::BindRepeating(
        &ConnectTestClassApp::HandleInterfaceClose, base::Unretained(this)));
    class_interface_receivers_.set_disconnect_handler(base::BindRepeating(
        &ConnectTestClassApp::HandleInterfaceClose, base::Unretained(this)));
    registry_.AddInterface<test::mojom::ConnectTestService>(base::BindRepeating(
        &ConnectTestClassApp::BindConnectTestServiceReceiver,
        base::Unretained(this)));
    registry_.AddInterface<test::mojom::ClassInterface>(
        base::BindRepeating(&ConnectTestClassApp::BindClassInterfaceReceiver,
                            base::Unretained(this)));
  }

  ConnectTestClassApp(const ConnectTestClassApp&) = delete;
  ConnectTestClassApp& operator=(const ConnectTestClassApp&) = delete;

  ~ConnectTestClassApp() override = default;

 private:
  // Service:
  void OnBindInterface(const BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override {
    registry_.BindInterface(interface_name, std::move(interface_pipe));
  }

  void BindConnectTestServiceReceiver(
      mojo::PendingReceiver<test::mojom::ConnectTestService> receiver) {
    refs_.push_back(service_keepalive_.CreateRef());
    receivers_.Add(this, std::move(receiver));
  }

  void BindClassInterfaceReceiver(
      mojo::PendingReceiver<test::mojom::ClassInterface> receiver) {
    refs_.push_back(service_keepalive_.CreateRef());
    class_interface_receivers_.Add(this, std::move(receiver));
  }

  // test::mojom::ConnectTestService:
  void GetTitle(GetTitleCallback callback) override {
    std::move(callback).Run("CLASS APP");
  }

  void GetInstanceId(GetInstanceIdCallback callback) override {
    std::move(callback).Run(service_receiver_.identity().instance_id());
  }

  // test::mojom::ClassInterface:
  void Ping(PingCallback callback) override { std::move(callback).Run("PONG"); }

  void HandleInterfaceClose() { refs_.pop_back(); }

  ServiceReceiver service_receiver_;
  ServiceKeepalive service_keepalive_;
  std::vector<std::unique_ptr<ServiceKeepaliveRef>> refs_;

  BinderRegistry registry_;
  mojo::ReceiverSet<test::mojom::ConnectTestService> receivers_;
  mojo::ReceiverSet<test::mojom::ClassInterface> class_interface_receivers_;
};

}  // namespace service_manager

void ServiceMain(
    mojo::PendingReceiver<service_manager::mojom::Service> receiver) {
  base::SingleThreadTaskExecutor main_task_executor;
  service_manager::ConnectTestClassApp(std::move(receiver))
      .RunUntilTermination();
}
