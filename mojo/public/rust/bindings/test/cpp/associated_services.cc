// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/rust/bindings/test/cpp/associated_services.h"

#include <vector>

#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/run_loop.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_associated_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/rust/bindings/test/cxx.rs.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace bindings_unittests::mojom {

namespace {

class PlusSevenMathServiceImpl : public MathService {
 public:
  PlusSevenMathServiceImpl() = default;
  ~PlusSevenMathServiceImpl() override = default;

  void Add(uint32_t a, uint32_t b, AddCallback callback) override {
    std::move(callback).Run(a + b + 7);
  }

  void AddTwoInts(TwoIntsPtr ns, AddTwoIntsCallback callback) override {
    std::move(callback).Run(static_cast<uint32_t>(ns->a) +
                            static_cast<uint32_t>(ns->b) + 7);
  }

  void DoNothing() override {}

  void DoNothingWithAck(DoNothingWithAckCallback callback) override {
    std::move(callback).Run();
  }
};

class CppHandleServiceImpl : public HandleService {
 public:
  CppHandleServiceImpl() = default;
  ~CppHandleServiceImpl() override = default;

  void PassHandles(mojo::ScopedMessagePipeHandle h1,
                   mojo::ScopedMessagePipeHandle h2,
                   mojo::ScopedMessagePipeHandle h3,
                   mojo::ScopedHandle h4) override {
    CHECK(h1.is_valid());
    CHECK(h2.is_valid());
    CHECK(h3.is_valid());
    CHECK(h4.is_valid());
    mojo::MakeSelfOwnedReceiver(
        std::make_unique<PlusSevenMathServiceImpl>(),
        mojo::PendingReceiver<MathService>(std::move(h1)));
  }
};
}  // namespace

// Binds an associated receiver from Rust to a PlusSevenMathService, C++ keeps
// ownership via a self-owned associated receiver.
void BindPlusSevenAssociatedReceiver(
    mojo::rust::bindings::CxxPendingAssociatedEndpoint adapter) {
  mojo::PendingAssociatedReceiver<MathService> receiver =
      mojo::rust::bindings::PassPendingAssociatedReceiver<MathService>(
          std::move(adapter));
  mojo::MakeSelfOwnedAssociatedReceiver(
      std::make_unique<PlusSevenMathServiceImpl>(), std::move(receiver));
}

// Binds an associated receiver from Rust to a CppHandleServiceImpl, C++ keeps
// ownership via a self-owned associated receiver.
void BindCppHandleServiceReceiver(
    mojo::rust::bindings::CxxPendingAssociatedEndpoint adapter) {
  mojo::PendingAssociatedReceiver<HandleService> receiver =
      mojo::rust::bindings::PassPendingAssociatedReceiver<HandleService>(
          std::move(adapter));
  mojo::MakeSelfOwnedAssociatedReceiver(
      std::make_unique<CppHandleServiceImpl>(), std::move(receiver));
}

// Binds an associated receiver from Rust to a new PlusSevenMathService and
// returns ownership.
std::unique_ptr<PlusSevenMathService> CreatePlusSevenAssociatedReceiver(
    mojo::rust::bindings::CxxPendingAssociatedEndpoint adapter) {
  mojo::PendingAssociatedReceiver<MathService> receiver =
      mojo::rust::bindings::PassPendingAssociatedReceiver<MathService>(
          std::move(adapter));
  return std::make_unique<PlusSevenMathService>(std::move(receiver));
}

extern "C" void rust_on_cpp_associated_disconnect(int32_t handler_type);

// Sets a disconnect handler on a PlusSevenMathService that notifies Rust.
void SetPlusSevenDisconnectCallback(PlusSevenMathService& service,
                                    int32_t handler_type) {
  service.set_disconnect_handler(
      base::BindOnce(&rust_on_cpp_associated_disconnect, handler_type));
}

// AssociatedSender implementation that attaches received MathService endpoints
// or creates new ones backed by PlusSevenMathService.
class AssociatedSenderImpl : public AssociatedSender {
 public:
  AssociatedSenderImpl() = default;
  ~AssociatedSenderImpl() override = default;

  // Receives an associated remote and verifies Add(1, 2) == 3.
  void SendRemote(mojo::PendingAssociatedRemote<MathService> remote) override {
    mojo::AssociatedRemote<MathService> math_remote(std::move(remote));
    math_remote.set_disconnect_handler(
        base::BindOnce(&rust_on_cpp_associated_disconnect, 1));
    math_remote->Add(
        1, 2, base::BindOnce([](uint32_t result) { EXPECT_EQ(result, 3u); }));
    active_remotes_.push_back(std::move(math_remote));
  }

  // Receives an associated receiver and binds it to a PlusSevenMathService.
  void SendReceiver(
      mojo::PendingAssociatedReceiver<MathService> receiver) override {
    auto service = std::make_unique<PlusSevenMathService>(std::move(receiver));
    service->set_disconnect_handler(
        base::BindOnce(&rust_on_cpp_associated_disconnect, 2));
    active_services_.push_back(std::move(service));
  }

  void SendHandleReceiver(
      mojo::PendingAssociatedReceiver<HandleService> receiver) override {
    mojo::MakeSelfOwnedAssociatedReceiver(
        std::make_unique<CppHandleServiceImpl>(), std::move(receiver));
  }

  void SendAssociatedSender(
      mojo::PendingAssociatedReceiver<AssociatedSender> receiver) override {}

  // Creates an associated pair, binds the receiver to a PlusSevenMathService,
  // and returns the remote.
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

  // Creates an associated pair, returns the receiver, and verifies Add(20, 30)
  // == 50.
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

  void RequestHandleRemote(RequestHandleRemoteCallback callback) override {
    mojo::PendingAssociatedRemote<HandleService> remote;
    mojo::PendingAssociatedReceiver<HandleService> receiver =
        remote.InitWithNewEndpointAndPassReceiver();
    mojo::MakeSelfOwnedAssociatedReceiver(
        std::make_unique<CppHandleServiceImpl>(), std::move(receiver));
    std::move(callback).Run(std::move(remote));
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

// Binds an associated receiver from Rust to an AssociatedSenderImpl, C++ keeps
// ownership via a self-owned associated receiver.
void BindPlusSevenAssociatedSender(
    mojo::rust::bindings::CxxPendingAssociatedEndpoint adapter) {
  mojo::PendingAssociatedReceiver<AssociatedSender> receiver =
      mojo::rust::bindings::PassPendingAssociatedReceiver<AssociatedSender>(
          std::move(adapter));
  mojo::MakeSelfOwnedAssociatedReceiver(
      std::make_unique<AssociatedSenderImpl>(), std::move(receiver));
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

  void SendHandleReceiver(
      mojo::PendingAssociatedReceiver<HandleService> receiver) override {
    auto adapter = mojo::rust::bindings::MakeAssociatedEndpointRustAdapter(
        std::move(receiver));
    BindRustHandleServiceReceiver(std::move(adapter));
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

  void RequestHandleRemote(RequestHandleRemoteCallback callback) override {
    mojo::PendingAssociatedRemote<HandleService> remote;
    mojo::PendingAssociatedReceiver<HandleService> receiver =
        remote.InitWithNewEndpointAndPassReceiver();
    auto adapter = mojo::rust::bindings::MakeAssociatedEndpointRustAdapter(
        std::move(receiver));
    std::move(callback).Run(std::move(remote));
    BindRustHandleServiceReceiver(std::move(adapter));
  }

  void SendAssociatedSender(
      mojo::PendingAssociatedReceiver<AssociatedSender> receiver) override {
    auto adapter = mojo::rust::bindings::MakeAssociatedEndpointRustAdapter(
        std::move(receiver));
    BindRustAssociatedSenderReceiver(std::move(adapter));
  }

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
