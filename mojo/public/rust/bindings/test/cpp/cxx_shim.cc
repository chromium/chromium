// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/rust/bindings/test/cpp/cxx_shim.h"

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace bindings_unittests::mojom {

// Binds handle to a PlusSevenMathService and returns ownership.
std::unique_ptr<PlusSevenMathService> CreatePlusSevenMathService(
    std::unique_ptr<mojo::rust::ScopedMessagePipeHandleWrapper> wrapper) {
  mojo::PendingReceiver<MathService> receiver(wrapper->take_handle());
  return std::make_unique<PlusSevenMathService>(std::move(receiver));
}

// Creates a new pipe, binds the receiver to a PlusSevenMathService (returned in
// service_out), and returns the remote handle in remote_out.
void CreatePlusSevenMathServiceAndRemote(
    std::unique_ptr<PlusSevenMathService>& service_out,
    std::unique_ptr<mojo::rust::ScopedMessagePipeHandleWrapper>& remote_out) {
  mojo::PendingRemote<MathService> remote;
  service_out = std::make_unique<PlusSevenMathService>(
      remote.InitWithNewPipeAndPassReceiver());
  remote_out = std::make_unique<mojo::rust::ScopedMessagePipeHandleWrapper>(
      remote.PassPipe());
}

AssociatedSenderTestRemote::AssociatedSenderTestRemote(
    std::unique_ptr<mojo::rust::ScopedMessagePipeHandleWrapper> wrapper)
    : remote_(
          mojo::PendingRemote<AssociatedSender>(wrapper->take_handle(), 0)) {}

AssociatedSenderTestRemote::~AssociatedSenderTestRemote() = default;

// Requests an associated remote from the receiver, waits for the response,
// and returns it to Rust as an adapter.
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

// Sends an associated receiver adapter to the receiver.
void AssociatedSenderTestRemote::SendReceiver(
    CxxPendingAssociatedEndpoint receiver_adapter) {
  mojo::PendingAssociatedReceiver<MathService> pending_receiver =
      mojo::rust::bindings::PassPendingAssociatedReceiver<MathService>(
          std::move(receiver_adapter));
  remote_->SendReceiver(std::move(pending_receiver));
}

// Just creates an AssociatedSenderTestRemote wrapped in a unique pointer.
std::unique_ptr<AssociatedSenderTestRemote> CreateAssociatedSenderTestRemote(
    std::unique_ptr<mojo::rust::ScopedMessagePipeHandleWrapper> wrapper) {
  return std::make_unique<AssociatedSenderTestRemote>(std::move(wrapper));
}

}  // namespace bindings_unittests::mojom
