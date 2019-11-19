// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/macros.h"
#include "base/task/single_thread_task_executor.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_binding.h"
#include "services/service_manager/public/mojom/service.mojom.h"
#include "services/service_manager/tests/background.test-mojom.h"

namespace service_manager {

// A service that exports a simple interface for testing. Used to test the
// parent background service manager.
class TestClient : public Service, public mojom::TestService {
 public:
  TestClient(mojom::ServiceRequest request)
      : service_binding_(this, std::move(request)) {
    registry_.AddInterface(base::BindRepeating(
        &TestClient::BindTestServiceReceiver, base::Unretained(this)));
  }

  ~TestClient() override = default;

 private:
  // Service:
  void OnBindInterface(const BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override {
    registry_.BindInterface(interface_name, std::move(interface_pipe));
  }

  // mojom::TestService
  void Test(TestCallback callback) override { std::move(callback).Run(); }

  void BindTestServiceReceiver(
      mojo::PendingReceiver<mojom::TestService> receiver) {
    receivers_.Add(this, std::move(receiver));
  }

  void Quit() override { service_binding_.RequestClose(); }

  ServiceBinding service_binding_;
  BinderRegistry registry_;
  mojo::ReceiverSet<mojom::TestService> receivers_;

  DISALLOW_COPY_AND_ASSIGN(TestClient);
};

}  // namespace service_manager

void ServiceMain(service_manager::mojom::ServiceRequest request) {
  base::SingleThreadTaskExecutor main_task_executor;
  service_manager::TestClient(std::move(request)).RunUntilTermination();
}
