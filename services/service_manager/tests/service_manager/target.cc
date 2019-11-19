// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/task/single_thread_task_executor.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_binding.h"
#include "services/service_manager/public/cpp/service_executable/service_main.h"
#include "services/service_manager/public/mojom/service.mojom.h"
#include "services/service_manager/tests/service_manager/service_manager.test-mojom.h"
#include "services/service_manager/tests/service_manager/test_manifests.h"

namespace {

class Target : public service_manager::Service {
 public:
  explicit Target(service_manager::mojom::ServiceRequest request)
      : service_binding_(this, std::move(request)) {}
  ~Target() override = default;

 private:
  // service_manager::Service:
  void OnStart() override {
    service_manager::test::mojom::CreateInstanceTestPtr service;
    service_binding_.GetConnector()->BindInterface(
        service_manager::kTestServiceName, &service);
    service->SetTargetIdentity(service_binding_.identity());
  }

  service_manager::ServiceBinding service_binding_;

  DISALLOW_COPY_AND_ASSIGN(Target);
};

}  // namespace

void ServiceMain(service_manager::mojom::ServiceRequest request) {
  base::SingleThreadTaskExecutor main_task_executor;
  Target(std::move(request)).RunUntilTermination();
}
