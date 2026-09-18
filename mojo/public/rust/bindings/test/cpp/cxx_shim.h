// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_RUST_BINDINGS_TEST_CPP_CXX_SHIM_H_
#define MOJO_PUBLIC_RUST_BINDINGS_TEST_CPP_CXX_SHIM_H_

#include "mojo/public/c/system/types.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/rust/bindings/multiplex_router/cpp_interop/associated_endpoint_rust_adapter.h"
#include "mojo/public/rust/bindings/test/cpp/add_seven_service.h"
#include "mojo/public/rust/system/scoped_handle_interop.h"

namespace bindings_unittests::mojom {

std::unique_ptr<PlusSevenMathService> CreatePlusSevenMathService(
    std::unique_ptr<mojo::rust::ScopedMessagePipeHandleWrapper> handle);

// Creates a new pipe, binds the receiver to a PlusSevenMathService (returned in
// service_out), and returns the remote handle in remote_out.
void CreatePlusSevenMathServiceAndRemote(
    std::unique_ptr<PlusSevenMathService>& service_out,
    std::unique_ptr<mojo::rust::ScopedMessagePipeHandleWrapper>& remote_out);

using CxxPendingAssociatedEndpoint =
    mojo::rust::bindings::CxxPendingAssociatedEndpoint;

// Wraps a mojo::Remote<AssociatedSender> and exposes methods across the CXX
// bridge so that Rust tests can trigger C++ associated endpoint operations.
class AssociatedSenderTestRemote {
 public:
  explicit AssociatedSenderTestRemote(
      std::unique_ptr<mojo::rust::ScopedMessagePipeHandleWrapper> handle);
  ~AssociatedSenderTestRemote();

  // Requests an associated remote from the receiver, waits for the response,
  // and returns it to Rust as an adapter.
  CxxPendingAssociatedEndpoint RequestRemote();

  // Sends an associated receiver adapter to the receiver.
  void SendReceiver(CxxPendingAssociatedEndpoint receiver_adapter);

 private:
  mojo::Remote<AssociatedSender> remote_;
};

// Creates an AssociatedSenderTestRemote wrapping handle.
std::unique_ptr<AssociatedSenderTestRemote> CreateAssociatedSenderTestRemote(
    std::unique_ptr<mojo::rust::ScopedMessagePipeHandleWrapper> handle);

}  // namespace bindings_unittests::mojom

#endif  // MOJO_PUBLIC_RUST_BINDINGS_TEST_CPP_CXX_SHIM_H_
