// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/functional/bind.h"
#include "base/task/single_thread_task_executor.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_executable/service_main.h"
#include "services/service_manager/public/cpp/service_receiver.h"
#include "services/service_manager/public/mojom/service.mojom.h"
#include "services/service_manager/tests/connect/connect.test-mojom.h"

using service_manager::test::mojom::ConnectTestService;

namespace {

class Target : public service_manager::Service,
               public ConnectTestService {
 public:
  explicit Target(
      mojo::PendingReceiver<service_manager::mojom::Service> receiver)
      : service_receiver_(this, std::move(receiver)) {
    registry_.AddInterface<ConnectTestService>(
        base::BindRepeating(&Target::Create, base::Unretained(this)));
  }

  Target(const Target&) = delete;
  Target& operator=(const Target&) = delete;

  ~Target() override = default;

 private:
  // service_manager::Service:
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override {
    registry_.BindInterface(interface_name, std::move(interface_pipe));
  }

  void Create(mojo::PendingReceiver<ConnectTestService> receiver) {
    receivers_.Add(this, std::move(receiver));
  }

  // ConnectTestService:
  void GetTitle(GetTitleCallback callback) override {
    std::move(callback).Run("connect_test_exe");
  }

  void GetInstanceId(GetInstanceIdCallback callback) override {
    std::move(callback).Run(service_receiver_.identity().instance_id());
  }

  service_manager::ServiceReceiver service_receiver_;
  service_manager::BinderRegistry registry_;
  mojo::ReceiverSet<ConnectTestService> receivers_;
};

}  // namespace

void ServiceMain(
    mojo::PendingReceiver<service_manager::mojom::Service> receiver) {
  base::SingleThreadTaskExecutor executor;
  Target(std::move(receiver)).RunUntilTermination();
}
