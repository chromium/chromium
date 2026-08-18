// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_RUST_BINDINGS_MULTIPLEX_ROUTER_CPP_INTEROP_ASSOCIATED_ENDPOINT_RUST_ADAPTER_H_
#define MOJO_PUBLIC_RUST_BINDINGS_MULTIPLEX_ROUTER_CPP_INTEROP_ASSOCIATED_ENDPOINT_RUST_ADAPTER_H_

#include <cstdint>
#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/threading/sequence_bound.h"
#include "mojo/public/cpp/bindings/associated_group_controller.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/scoped_interface_endpoint_handle.h"
#include "mojo/public/rust/bindings/multiplex_router/cpp_interop/mojo_responder_wrapper.h"
#include "third_party/rust/cxx/v1/cxx.h"

namespace mojo::rust::bindings {

// Defined in Rust, exposed in the cxx bridge
struct EndpointInfo;
class InterfaceEndpointClientAdapter;

// This file defines the C++ side of the interop layer that enables Rust to
// attach Mojo associated interfaces to a C++ message pipe.
//
// `AssociatedEndpointRustAdapter` can be thought of as a "generic" pending
// associated remote/receiver. It can be used to create one in either Rust
// or C++.
//
// Functionally, it provides a way for the Rust `Remote` and `Receiver` types
// to create new associated interfaces and send/receive messages from a pipe
// whose primary endpoint lives in C++.
//
// An adapter that wraps a C++ `ScopedInterfaceEndpointHandle` and allows
// a Rust associated remote or receiver to send and receive messages through it.
//
// Sequencing Requirements:
// - Unbound: Unbound adapters may be moved across sequences before `Bind()` is
// called.
// - Bound: `Bind` permanently binds the adapter to a sequence. Outgoing calls
//   (`SendMessage`, `Close`) will automatically run on the given sequence. All
//   incoming IPC messages, disconnect callbacks, and destruction must be
//   invoked only on the bound sequence.
class AssociatedEndpointRustAdapter {
 public:
  explicit AssociatedEndpointRustAdapter(
      mojo::ScopedInterfaceEndpointHandle handle);
  ~AssociatedEndpointRustAdapter();

  AssociatedEndpointRustAdapter(const AssociatedEndpointRustAdapter&) = delete;
  AssociatedEndpointRustAdapter& operator=(
      const AssociatedEndpointRustAdapter&) = delete;

  // Create a new adapter wrapped in a unique_ptr. This function mostly exists
  // to be called from Rust.
  static std::unique_ptr<AssociatedEndpointRustAdapter> Create(
      mojo::ScopedInterfaceEndpointHandle handle);

  // Takes ownership of the underlying endpoint handle.
  mojo::ScopedInterfaceEndpointHandle PassHandle();

  // Binds the endpoint to `runner`. Incoming messages and disconnect events
  // are routed to the provided Rust callbacks.
  void Bind(const base::SequencedTaskRunner& runner,
            ::rust::Box<EndpointInfo> info);

  // Returns the interface ID assigned to this endpoint on the routing group.
  uint32_t GetInterfaceId() const;

  // Closes the endpoint and sends a disconnect notification over the pipe.
  void Close();

  // Forwards an outgoing IPC message from Rust to the bound C++ endpoint
  // client.
  void SendMessage(std::unique_ptr<mojo::rust::ScopedMessageHandleWrapper>
                       message_wrapper) const;

  // Allocates a new associated endpoint on the underlying pipe, and
  // returns a new adapter wrapping it. If |interface_id| is
  // kInvalidInterfaceId, a new ID is generated. Otherwise, an endpoint handle
  // is created for the specified ID.
  // May fail and return nullptr if the adapter isn't yet bound to a pipe, or
  // if the pipe has already been closed.
  std::unique_ptr<AssociatedEndpointRustAdapter> RegisterNewEndpoint(
      uint32_t interface_id) const;

 private:
  // Returns the underlying group controller.
  mojo::AssociatedGroupController* group_controller() const;

  // Exactly one of these will be set at any given time; `handle_` is set during
  // construction, and is moved into `client_adapter_` when `Bind` is called.
  // At that point, it remains in `client_adapter_` for the rest of its life.
  mojo::ScopedInterfaceEndpointHandle handle_;
  scoped_refptr<InterfaceEndpointClientAdapter> client_adapter_;
};

// ****************************************************************************
// Free Functions & Helper Templates
// ****************************************************************************

// Type alias for CXX FFI bindings.
using CxxPendingAssociatedEndpoint =
    std::unique_ptr<AssociatedEndpointRustAdapter>;

// Creates a pair of unassociated endpoint adapters for FFI.
// Presumably one will be returned to Rust, and the other will be serialized.
// The serialization process will automatically update the other endpoint, so
// that the Rust side knows which pipe to send messages through.
void CreatePairPendingAssociation(
    std::unique_ptr<AssociatedEndpointRustAdapter>& self_out,
    std::unique_ptr<AssociatedEndpointRustAdapter>& peer_out);

// ****************************************************************************
// C++ Interop Helpers
//
// Helper templates for C++ developers to convert between C++ Mojo types
// (`mojo::PendingAssociatedReceiver` / `mojo::PendingAssociatedRemote`) and
// `AssociatedEndpointRustAdapter`.
//
// You should use these functions if you need to send or a receive a pending
// associated endpoint to/from Rust code. The Adapter can be used to create a
// Rust associated endpoint once it's passed across the FFI boundary.
// ****************************************************************************

template <typename Interface>
mojo::PendingAssociatedReceiver<Interface> PassPendingAssociatedReceiver(
    std::unique_ptr<AssociatedEndpointRustAdapter> adapter) {
  if (!adapter) {
    return {};
  }
  return mojo::PendingAssociatedReceiver<Interface>(adapter->PassHandle());
}

template <typename Interface>
mojo::PendingAssociatedRemote<Interface> PassPendingAssociatedRemote(
    std::unique_ptr<AssociatedEndpointRustAdapter> adapter) {
  if (!adapter) {
    return {};
  }
  return mojo::PendingAssociatedRemote<Interface>(adapter->PassHandle());
}

template <typename Interface>
std::unique_ptr<AssociatedEndpointRustAdapter>
MakeAssociatedEndpointRustAdapter(
    mojo::PendingAssociatedReceiver<Interface> receiver) {
  return AssociatedEndpointRustAdapter::Create(receiver.PassHandle());
}

template <typename Interface>
std::unique_ptr<AssociatedEndpointRustAdapter>
MakeAssociatedEndpointRustAdapter(
    mojo::PendingAssociatedRemote<Interface> remote) {
  return AssociatedEndpointRustAdapter::Create(remote.PassHandle());
}

}  // namespace mojo::rust::bindings

#endif  // MOJO_PUBLIC_RUST_BINDINGS_MULTIPLEX_ROUTER_CPP_INTEROP_ASSOCIATED_ENDPOINT_RUST_ADAPTER_H_
