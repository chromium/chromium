// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/single_thread_task_executor.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_executable/service_main.h"
#include "services/service_manager/public/cpp/service_receiver.h"
#include "services/service_manager/public/mojom/service.mojom.h"
#include "services/service_manager/tests/service_manager/service_manager.test-mojom.h"
#include "services/service_manager/tests/service_manager/test_manifests.h"

namespace {

class Target : public service_manager::Service {
 public:
  explicit Target(
      mojo::PendingReceiver<service_manager::mojom::Service> receiver)
      : service_receiver_(this, std::move(receiver)) {}

  Target(const Target&) = delete;
  Target& operator=(const Target&) = delete;

  ~Target() override = default;

 private:
  // service_manager::Service:
  void OnStart() override {
    mojo::Remote<service_manager::test::mojom::CreateInstanceTest> service;
    service_receiver_.GetConnector()->BindInterface(
        service_manager::kTestServiceName,
        service.BindNewPipeAndPassReceiver());
    service->SetTargetIdentity(service_receiver_.identity());
  }

  service_manager::ServiceReceiver service_receiver_;
};

}  // namespace

void ServiceMain(
    mojo::PendingReceiver<service_manager::mojom::Service> receiver) {
  base::SingleThreadTaskExecutor main_task_executor;
  Target(std::move(receiver)).RunUntilTermination();
}
