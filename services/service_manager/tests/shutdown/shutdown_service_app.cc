// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/task/single_thread_task_executor.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_executable/service_main.h"
#include "services/service_manager/public/cpp/service_receiver.h"
#include "services/service_manager/public/mojom/service.mojom.h"
#include "services/service_manager/tests/shutdown/shutdown.test-mojom.h"

namespace service_manager {
namespace {

class ShutdownServiceApp : public Service, public mojom::ShutdownTestService {
 public:
  explicit ShutdownServiceApp(mojo::PendingReceiver<mojom::Service> receiver)
      : service_receiver_(this, std::move(receiver)) {
    registry_.AddInterface<mojom::ShutdownTestService>(base::BindRepeating(
        &ShutdownServiceApp::Create, base::Unretained(this)));
  }

  ShutdownServiceApp(const ShutdownServiceApp&) = delete;
  ShutdownServiceApp& operator=(const ShutdownServiceApp&) = delete;

  ~ShutdownServiceApp() override = default;

 private:
  // Service:
  void OnBindInterface(const BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override {
    registry_.BindInterface(interface_name, std::move(interface_pipe));
  }

  // mojom::ShutdownTestService:
  void SetClient(
      mojo::PendingRemote<mojom::ShutdownTestClient> client) override {}
  void ShutDown() override { Terminate(); }

  void Create(mojo::PendingReceiver<mojom::ShutdownTestService> receiver) {
    receivers_.Add(this, std::move(receiver));
  }

  ServiceReceiver service_receiver_;
  BinderRegistry registry_;
  mojo::ReceiverSet<mojom::ShutdownTestService> receivers_;
};

}  // namespace
}  // namespace service_manager

void ServiceMain(
    mojo::PendingReceiver<service_manager::mojom::Service> receiver) {
  base::SingleThreadTaskExecutor main_task_executor;
  service_manager::ShutdownServiceApp(std::move(receiver))
      .RunUntilTermination();
}
