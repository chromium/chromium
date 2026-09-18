// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/rust/bindings/test/cpp/associated_services.h"

#include <memory>
#include <vector>

#include "base/functional/bind.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/rust/bindings/multiplex_router/cpp_interop/associated_endpoint_rust_adapter.h"
#include "mojo/public/rust/bindings/test/cpp/add_seven_service.h"
#include "mojo/public/rust/bindings/test/cxx.rs.h"
#include "mojo/public/rust/bindings/test/test_util/bindings_unittests.test-mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace bindings_unittests::mojom {

extern "C" void rust_on_cpp_associated_disconnect(int32_t handler_type);

class AssociatedSenderImpl : public AssociatedSender {
 public:
  AssociatedSenderImpl() = default;
  ~AssociatedSenderImpl() override = default;

  void SendRemote(mojo::PendingAssociatedRemote<MathService> remote) override {
    mojo::AssociatedRemote<MathService> math_remote(std::move(remote));
    math_remote.set_disconnect_handler(
        base::BindOnce(&rust_on_cpp_associated_disconnect, 1));
    math_remote->Add(
        1, 2, base::BindOnce([](uint32_t result) { EXPECT_EQ(result, 3u); }));
    active_remotes_.push_back(std::move(math_remote));
  }

  void SendReceiver(
      mojo::PendingAssociatedReceiver<MathService> receiver) override {
    auto service = std::make_unique<PlusSevenMathService>(std::move(receiver));
    service->set_disconnect_handler(
        base::BindOnce(&rust_on_cpp_associated_disconnect, 2));
    active_services_.push_back(std::move(service));
  }

  void RequestRemote(RequestRemoteCallback callback) override {
    mojo::PendingAssociatedRemote<MathService> remote;
    mojo::PendingAssociatedReceiver<MathService> receiver =
        remote.InitWithNewEndpointAndPassReceiver();
    auto service = std::make_unique<PlusSevenMathService>(std::move(receiver));
    service->set_disconnect_handler(
        base::BindOnce(&rust_on_cpp_associated_disconnect, 3));
    active_services_.push_back(std::move(service));
    std::move(callback).Run(std::move(remote));
  }

  void RequestReceiver(RequestReceiverCallback callback) override {
    mojo::PendingAssociatedRemote<MathService> remote;
    mojo::PendingAssociatedReceiver<MathService> receiver =
        remote.InitWithNewEndpointAndPassReceiver();

    // Call the callback first to associate the endpoint
    std::move(callback).Run(std::move(receiver));

    mojo::AssociatedRemote<MathService> math_remote(std::move(remote));
    math_remote.set_disconnect_handler(
        base::BindOnce(&rust_on_cpp_associated_disconnect, 4));
    math_remote->Add(20, 30, base::BindOnce([](uint32_t result) {
                       EXPECT_EQ(result, 50u);
                     }));

    active_remotes_.push_back(std::move(math_remote));
  }

  void ClearActiveEndpoints() override {
    active_remotes_.clear();
    active_services_.clear();
  }

 private:
  std::vector<mojo::AssociatedRemote<MathService>> active_remotes_;
  std::vector<std::unique_ptr<PlusSevenMathService>> active_services_;
};

// Binds handle to an AssociatedSenderImpl, which handles requests to send or
// create associated MathService endpoints backed by PlusSevenMathService.
void CreateCppAssociatedSender(
    std::unique_ptr<mojo::rust::ScopedMessagePipeHandleWrapper> wrapper) {
  mojo::PendingReceiver<AssociatedSender> receiver(wrapper->take_handle());
  mojo::MakeSelfOwnedReceiver(std::make_unique<AssociatedSenderImpl>(),
                              std::move(receiver));
}

// AssociatedSender implementation that converts received endpoints into Rust
// adapters and passes them back into Rust via BindRustMathServiceReceiver.
class AssociatedSenderInteropTestImpl : public AssociatedSender {
 public:
  AssociatedSenderInteropTestImpl() = default;
  ~AssociatedSenderInteropTestImpl() override = default;

  void SendRemote(mojo::PendingAssociatedRemote<MathService> remote) override {}
  void SendReceiver(
      mojo::PendingAssociatedReceiver<MathService> receiver) override {
    auto adapter = mojo::rust::bindings::MakeAssociatedEndpointRustAdapter(
        std::move(receiver));
    BindRustMathServiceReceiver(std::move(adapter));
  }

  void RequestRemote(RequestRemoteCallback callback) override {
    mojo::PendingAssociatedRemote<MathService> remote;
    mojo::PendingAssociatedReceiver<MathService> receiver =
        remote.InitWithNewEndpointAndPassReceiver();
    auto adapter = mojo::rust::bindings::MakeAssociatedEndpointRustAdapter(
        std::move(receiver));
    std::move(callback).Run(std::move(remote));
    BindRustMathServiceReceiver(std::move(adapter));
  }

  void RequestReceiver(RequestReceiverCallback callback) override {}
  void ClearActiveEndpoints() override {}
};

// Binds handle to an AssociatedSenderInteropTestImpl, which forwards received
// associated endpoints back to Rust via BindRustMathServiceReceiver.
void CreateAssociatedSenderInteropTest(
    std::unique_ptr<mojo::rust::ScopedMessagePipeHandleWrapper> wrapper) {
  mojo::PendingReceiver<AssociatedSender> receiver(wrapper->take_handle());
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<AssociatedSenderInteropTestImpl>(), std::move(receiver));
}

}  // namespace bindings_unittests::mojom
