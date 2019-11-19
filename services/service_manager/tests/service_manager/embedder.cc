// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/macros.h"
#include "base/task/single_thread_task_executor.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_binding.h"
#include "services/service_manager/public/cpp/service_executable/service_main.h"
#include "services/service_manager/public/mojom/service.mojom.h"
#include "services/service_manager/tests/service_manager/test_manifests.h"

namespace {

class PackagedService : public service_manager::Service {
 public:
  explicit PackagedService(service_manager::mojom::ServiceRequest request)
      : service_binding_(this, std::move(request)) {}
  ~PackagedService() override = default;

 private:
  service_manager::ServiceBinding service_binding_;

  DISALLOW_COPY_AND_ASSIGN(PackagedService);
};

class Embedder : public service_manager::Service {
 public:
  explicit Embedder(service_manager::mojom::ServiceRequest request)
      : service_binding_(this, std::move(request)) {}
  ~Embedder() override = default;

 private:
  // service_manager::Service:
  void CreatePackagedServiceInstance(
      const std::string& service_name,
      mojo::PendingReceiver<service_manager::mojom::Service> receiver,
      CreatePackagedServiceInstanceCallback callback) override {
    if (service_name == service_manager::kTestRegularServiceName ||
        service_name == service_manager::kTestSharedServiceName ||
        service_name == service_manager::kTestSingletonServiceName) {
      packaged_service_ = std::make_unique<PackagedService>(
          service_manager::mojom::ServiceRequest(std::move(receiver)));
      std::move(callback).Run(base::GetCurrentProcId());
    } else {
      LOG(ERROR) << "Failed to create unknown service " << service_name;
      std::move(callback).Run(base::nullopt);
    }
  }

  service_manager::ServiceBinding service_binding_;
  std::unique_ptr<service_manager::Service> packaged_service_;

  DISALLOW_COPY_AND_ASSIGN(Embedder);
};

}  // namespace

void ServiceMain(service_manager::mojom::ServiceRequest request) {
  base::SingleThreadTaskExecutor main_task_executor;
  Embedder(std::move(request)).RunUntilTermination();
}
