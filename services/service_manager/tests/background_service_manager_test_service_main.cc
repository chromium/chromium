// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/task/single_thread_task_executor.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_receiver.h"
#include "services/service_manager/public/mojom/service.mojom.h"
#include "services/service_manager/tests/background.test-mojom.h"

namespace service_manager {

// A service that exports a simple interface for testing. Used to test the
// parent background service manager.
class TestClient : public Service, public mojom::TestService {
 public:
  explicit TestClient(mojo::PendingReceiver<mojom::Service> receiver)
      : service_receiver_(this, std::move(receiver)) {
    registry_.AddInterface(base::BindRepeating(
        &TestClient::BindTestServiceReceiver, base::Unretained(this)));
  }

  TestClient(const TestClient&) = delete;
  TestClient& operator=(const TestClient&) = delete;

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

  void Quit() override { service_receiver_.RequestClose(); }

  ServiceReceiver service_receiver_;
  BinderRegistry registry_;
  mojo::ReceiverSet<mojom::TestService> receivers_;
};

}  // namespace service_manager

void ServiceMain(
    mojo::PendingReceiver<service_manager::mojom::Service> receiver) {
  base::SingleThreadTaskExecutor main_task_executor;
  service_manager::TestClient(std::move(receiver)).RunUntilTermination();
}
