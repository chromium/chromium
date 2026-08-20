// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/rust/bindings/test/cpp/cxx_shim.h"

#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/run_loop.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_associated_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/rust/bindings/test/cxx.rs.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace bindings_unittests::mojom {

std::unique_ptr<PlusSevenMathService> CreatePlusSevenMathService(
    std::unique_ptr<mojo::rust::ScopedMessagePipeHandleWrapper> wrapper) {
  mojo::PendingReceiver<MathService> receiver(wrapper->take_handle());
  return std::make_unique<PlusSevenMathService>(std::move(receiver));
}

// To be called by the Rust testing code. Constructs a math service remote from
// the given handle, and sends several messages through to ensure basic
// functionality.
void TestRemoteFromCpp(
    std::unique_ptr<mojo::rust::ScopedMessagePipeHandleWrapper> wrapper) {
  mojo::Remote<MathService> remote(
      mojo::PendingRemote<MathService>(wrapper->take_handle(), 0));

  base::RunLoop run_loop;

  remote->Add(1, 2,
              base::BindOnce([](uint32_t result) { EXPECT_EQ(result, 3u); }));

  remote->AddTwoInts(TwoInts::New(7, 12),
                     base::BindOnce(
                         [](base::OnceClosure quit_closure, uint32_t result) {
                           EXPECT_EQ(result, 19u);
                           std::move(quit_closure).Run();
                         },
                         run_loop.QuitClosure()));

  run_loop.Run();
}

void CreatePlusSevenMathServiceAndRemote(
    std::unique_ptr<PlusSevenMathService>& service_out,
    std::unique_ptr<mojo::rust::ScopedMessagePipeHandleWrapper>& remote_out) {
  mojo::PendingRemote<MathService> remote;
  service_out = std::make_unique<PlusSevenMathService>(
      remote.InitWithNewPipeAndPassReceiver());
  remote_out = std::make_unique<mojo::rust::ScopedMessagePipeHandleWrapper>(
      remote.PassPipe());
}

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

void CreateCppAssociatedSender(
    std::unique_ptr<mojo::rust::ScopedMessagePipeHandleWrapper> wrapper) {
  mojo::PendingReceiver<AssociatedSender> receiver(wrapper->take_handle());
  mojo::MakeSelfOwnedReceiver(std::make_unique<AssociatedSenderImpl>(),
                              std::move(receiver));
}

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

void CreateAssociatedSenderInteropTest(
    std::unique_ptr<mojo::rust::ScopedMessagePipeHandleWrapper> wrapper) {
  mojo::PendingReceiver<AssociatedSender> receiver(wrapper->take_handle());
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<AssociatedSenderInteropTestImpl>(), std::move(receiver));
}

AssociatedSenderTestRemote::AssociatedSenderTestRemote(
    std::unique_ptr<mojo::rust::ScopedMessagePipeHandleWrapper> wrapper)
    : remote_(
          mojo::PendingRemote<AssociatedSender>(wrapper->take_handle(), 0)) {}

AssociatedSenderTestRemote::~AssociatedSenderTestRemote() = default;

CxxPendingAssociatedEndpoint AssociatedSenderTestRemote::RequestRemote() {
  base::RunLoop run_loop;
  CxxPendingAssociatedEndpoint result;
  remote_->RequestRemote(base::BindOnce(
      [](base::OnceClosure quit, CxxPendingAssociatedEndpoint* result,
         mojo::PendingAssociatedRemote<MathService> pending_remote) {
        *result = mojo::rust::bindings::MakeAssociatedEndpointRustAdapter(
            std::move(pending_remote));
        std::move(quit).Run();
      },
      run_loop.QuitClosure(), &result));
  run_loop.Run();
  return result;
}

void AssociatedSenderTestRemote::SendReceiver(
    CxxPendingAssociatedEndpoint receiver_adapter) {
  mojo::PendingAssociatedReceiver<MathService> pending_receiver =
      mojo::rust::bindings::PassPendingAssociatedReceiver<MathService>(
          std::move(receiver_adapter));
  remote_->SendReceiver(std::move(pending_receiver));
}

std::unique_ptr<AssociatedSenderTestRemote> CreateAssociatedSenderTestRemote(
    std::unique_ptr<mojo::rust::ScopedMessagePipeHandleWrapper> wrapper) {
  return std::make_unique<AssociatedSenderTestRemote>(std::move(wrapper));
}

}  // namespace bindings_unittests::mojom
