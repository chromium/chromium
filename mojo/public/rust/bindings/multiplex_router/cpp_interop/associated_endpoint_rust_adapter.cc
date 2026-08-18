// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implements `AssociatedEndpointRustAdapter` public methods and FFI entry
// points.
//
// Design Rationale:
// `AssociatedEndpointRustAdapter` represents an unassociated or bound
// associated endpoint. `Bind()` delays instantiating the ref-counted
// `InterfaceEndpointClientAdapter` until Rust explicitly binds the endpoint to
// a sequence task runner. This lazy initialization allows pending associated
// endpoints created in Rust to be passed around as unassociated handles before
// being bound.

#include "mojo/public/rust/bindings/multiplex_router/cpp_interop/associated_endpoint_rust_adapter.h"

#include <utility>

#include "base/check.h"
#include "mojo/public/cpp/bindings/interface_id.h"
#include "mojo/public/rust/bindings/multiplex_router/cpp_interop/interface_endpoint_client_adapter.h"

namespace mojo::rust::bindings {

AssociatedEndpointRustAdapter::AssociatedEndpointRustAdapter(
    mojo::ScopedInterfaceEndpointHandle handle)
    : handle_(std::move(handle)) {}

AssociatedEndpointRustAdapter::~AssociatedEndpointRustAdapter() = default;

std::unique_ptr<AssociatedEndpointRustAdapter>
AssociatedEndpointRustAdapter::Create(
    mojo::ScopedInterfaceEndpointHandle handle) {
  return std::make_unique<AssociatedEndpointRustAdapter>(std::move(handle));
}

void CreatePairPendingAssociation(
    std::unique_ptr<AssociatedEndpointRustAdapter>& self_out,
    std::unique_ptr<AssociatedEndpointRustAdapter>& peer_out) {
  mojo::ScopedInterfaceEndpointHandle handle0;
  mojo::ScopedInterfaceEndpointHandle handle1;
  mojo::ScopedInterfaceEndpointHandle::CreatePairPendingAssociation(&handle0,
                                                                    &handle1);
  self_out = AssociatedEndpointRustAdapter::Create(std::move(handle0));
  peer_out = AssociatedEndpointRustAdapter::Create(std::move(handle1));
}

// Takes ownership of the underlying endpoint handle.
mojo::ScopedInterfaceEndpointHandle
AssociatedEndpointRustAdapter::PassHandle() {
  return std::move(handle_);
}

// Binds the endpoint to the current sequence's SequencedTaskRunner by creating
// an InterfaceEndpointClientAdapter. Incoming messages and disconnect events
// will be routed to the provided Rust callbacks. Fills the role of
// MultiplexRouterHandle::bind().
void AssociatedEndpointRustAdapter::Bind(
    const base::SequencedTaskRunner& runner,
    ::rust::Box<EndpointInfo> info) {
  CHECK(!client_adapter_);
  client_adapter_ = base::MakeRefCounted<InterfaceEndpointClientAdapter>(
      std::move(handle_), std::move(info),
      scoped_refptr<base::SequencedTaskRunner>(
          const_cast<base::SequencedTaskRunner*>(&runner)));
}

// Returns the interface ID assigned to this endpoint on the routing group.
// Fills the role of MultiplexRouterHandle::interface_id().
uint32_t AssociatedEndpointRustAdapter::GetInterfaceId() const {
  return client_adapter_ ? client_adapter_->id() : handle_.id();
}

// Closes the endpoint and sends the peer-endpoint-closed disconnect
// notification over the IPC pipe immediately.
void AssociatedEndpointRustAdapter::Close() {
  if (client_adapter_) {
    client_adapter_->Close();
  } else {
    handle_.reset();
  }
}

// Forwards an outgoing IPC message from Rust to the bound C++ endpoint client.
// Fills the role of MultiplexRouterHandle::send_message().
void AssociatedEndpointRustAdapter::SendMessage(
    std::unique_ptr<mojo::rust::ScopedMessageHandleWrapper> message_wrapper)
    const {
  CHECK(client_adapter_);
  client_adapter_->SendMessage(std::move(message_wrapper));
}

// Allocates a new nested associated endpoint on this routing group and returns
// a new adapter wrapping it. If |interface_id| is kInvalidInterfaceId
// a new ID is generated. Otherwise, an endpoint handle is created
// for the specified ID. Returns nullptr on failure. Fills the role of
// MultiplexRouterHandle::register_new_endpoint().
std::unique_ptr<AssociatedEndpointRustAdapter>
AssociatedEndpointRustAdapter::RegisterNewEndpoint(
    uint32_t interface_id) const {
  auto* controller = group_controller();
  if (!controller) {
    return nullptr;
  }

  mojo::ScopedInterfaceEndpointHandle new_handle;
  if (interface_id == mojo::kInvalidInterfaceId) {
    mojo::ScopedInterfaceEndpointHandle local_handle;
    mojo::ScopedInterfaceEndpointHandle remote_handle;
    mojo::ScopedInterfaceEndpointHandle::CreatePairPendingAssociation(
        &local_handle, &remote_handle);
    controller->AssociateInterface(std::move(remote_handle));
    new_handle = std::move(local_handle);
  } else {
    new_handle =
        controller->CreateLocalEndpointHandle(mojo::InterfaceId(interface_id));
  }

  if (!new_handle.is_valid()) {
    return nullptr;
  }
  return std::make_unique<AssociatedEndpointRustAdapter>(std::move(new_handle));
}

// Returns the underlying group controller
mojo::AssociatedGroupController*
AssociatedEndpointRustAdapter::group_controller() const {
  return client_adapter_ ? client_adapter_->group_controller()
                         : handle_.group_controller();
}

}  // namespace mojo::rust::bindings
