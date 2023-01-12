// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_executor.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_executable/service_main.h"
#include "services/service_manager/public/cpp/service_receiver.h"
#include "services/service_manager/tests/shutdown/shutdown.test-mojom.h"

namespace service_manager {

class ShutdownClientApp : public Service,
                          public mojom::ShutdownTestClientController,
                          public mojom::ShutdownTestClient {
 public:
  explicit ShutdownClientApp(mojo::PendingReceiver<mojom::Service> receiver)
      : service_receiver_(this, std::move(receiver)) {
    registry_.AddInterface<mojom::ShutdownTestClientController>(
        base::BindRepeating(&ShutdownClientApp::Create,
                            base::Unretained(this)));
  }

  ShutdownClientApp(const ShutdownClientApp&) = delete;
  ShutdownClientApp& operator=(const ShutdownClientApp&) = delete;

  ~ShutdownClientApp() override = default;

 private:
  // service_manager::Service:
  void OnBindInterface(const BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override {
    registry_.BindInterface(interface_name, std::move(interface_pipe));
  }

  void Create(
      mojo::PendingReceiver<mojom::ShutdownTestClientController> receiver) {
    receivers_.Add(this, std::move(receiver));
  }

  // mojom::ShutdownTestClientController:
  void ConnectAndWait(ConnectAndWaitCallback callback) override {
    mojo::Remote<mojom::ShutdownTestService> service;
    service_receiver_.GetConnector()->BindInterface(
        "shutdown_service", service.BindNewPipeAndPassReceiver());

    mojo::Receiver<mojom::ShutdownTestClient> client_receiver(this);

    service->SetClient(client_receiver.BindNewPipeAndPassRemote());

    base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
    client_receiver.set_disconnect_handler(run_loop.QuitClosure());
    run_loop.Run();

    std::move(callback).Run();
  }

  ServiceReceiver service_receiver_;
  BinderRegistry registry_;
  mojo::ReceiverSet<mojom::ShutdownTestClientController> receivers_;
};

}  // namespace service_manager

void ServiceMain(
    mojo::PendingReceiver<service_manager::mojom::Service> receiver) {
  base::SingleThreadTaskExecutor main_task_executor;
  service_manager::ShutdownClientApp(std::move(receiver)).RunUntilTermination();
}
