// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/functional/bind.h"
#include "base/task/single_thread_task_executor.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_executable/service_main.h"
#include "services/service_manager/public/cpp/service_receiver.h"
#include "services/service_manager/public/mojom/service.mojom.h"
#include "services/service_manager/tests/service_manager/test_manifests.h"

namespace {

class PackagedService : public service_manager::Service {
 public:
  explicit PackagedService(
      mojo::PendingReceiver<service_manager::mojom::Service> receiver)
      : service_receiver_(this, std::move(receiver)) {}

  PackagedService(const PackagedService&) = delete;
  PackagedService& operator=(const PackagedService&) = delete;

  ~PackagedService() override = default;

 private:
  service_manager::ServiceReceiver service_receiver_;
};

class Embedder : public service_manager::Service {
 public:
  explicit Embedder(
      mojo::PendingReceiver<service_manager::mojom::Service> receiver)
      : service_receiver_(this, std::move(receiver)) {}

  Embedder(const Embedder&) = delete;
  Embedder& operator=(const Embedder&) = delete;

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
      packaged_service_ =
          std::make_unique<PackagedService>(std::move(receiver));
      std::move(callback).Run(base::GetCurrentProcId());
    } else {
      LOG(ERROR) << "Failed to create unknown service " << service_name;
      std::move(callback).Run(std::nullopt);
    }
  }

  service_manager::ServiceReceiver service_receiver_;
  std::unique_ptr<service_manager::Service> packaged_service_;
};

}  // namespace

void ServiceMain(
    mojo::PendingReceiver<service_manager::mojom::Service> receiver) {
  base::SingleThreadTaskExecutor main_task_executor;
  Embedder(std::move(receiver)).RunUntilTermination();
}
