// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/rust/bindings/multiplex_router/cpp_interop/rust_associated_group_controller.h"

#include <utility>

#include "base/check.h"
#include "base/notimplemented.h"
#include "base/notreached.h"
#include "mojo/public/rust/bindings/multiplex_router/cpp_interop/cxx.rs.h"

namespace mojo::rust::bindings {

RustAssociatedGroupController::RustAssociatedGroupController(
    ::rust::Box<MultiplexRouter> router,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : AssociatedGroupController(std::move(task_runner)),
      router_(std::move(router)) {}

RustAssociatedGroupController::~RustAssociatedGroupController() = default;

RustAssociatedGroupController* CreateGroupControllerForRustRouter(
    ::rust::Box<MultiplexRouter> router,
    base::SequencedTaskRunner& runner) {
  // `release()` hands the reference we just created to the caller.
  return base::MakeRefCounted<RustAssociatedGroupController>(
             std::move(router), base::WrapRefCounted(&runner))
      .release();
}

mojo::InterfaceId RustAssociatedGroupController::AssociateInterface(
    mojo::ScopedInterfaceEndpointHandle handle_to_send) {
  if (!handle_to_send.pending_association()) {
    return mojo::kInvalidInterfaceId;
  }
  uint32_t id = allocate_interface_id(*router_);
  if (!mojo::IsValidInterfaceId(id) || mojo::IsPrimaryInterfaceId(id)) {
    return mojo::kInvalidInterfaceId;
  }
  bool peer_alive = NotifyAssociation(&handle_to_send, id);
  // TODO(crbug.com/556744520): Handle failure gracefully. Simply sending a
  // disconnect notification now is wrong because it will be sent too early
  // and arrive _before_ the interface that's supposed to be disconnected.
  CHECK(peer_alive);
  return id;
}

mojo::ScopedInterfaceEndpointHandle
RustAssociatedGroupController::CreateLocalEndpointHandle(mojo::InterfaceId id) {
  if (!mojo::IsValidInterfaceId(id) || mojo::IsPrimaryInterfaceId(id) ||
      !register_interface_id(*router_, id)) {
    return mojo::ScopedInterfaceEndpointHandle();
  }
  return CreateScopedInterfaceEndpointHandle(id);
}

mojo::ScopedInterfaceEndpointHandle
RustAssociatedGroupController::CreateScopedHandleForExistingEndpoint(
    mojo::InterfaceId id) {
  if (!mojo::IsValidInterfaceId(id) || mojo::IsPrimaryInterfaceId(id)) {
    return mojo::ScopedInterfaceEndpointHandle();
  }
  return CreateScopedInterfaceEndpointHandle(id);
}

void RustAssociatedGroupController::CloseEndpointHandle(
    mojo::InterfaceId id,
    const std::optional<mojo::DisconnectReason>& reason) {
  if (!mojo::IsValidInterfaceId(id) || mojo::IsPrimaryInterfaceId(id)) {
    return;
  }
  // TODO(crbug.com/561683462): Rust doesn't support disconnect reasons yet
  router_->notify_dropped(id);
}

void RustAssociatedGroupController::NotifyLocalEndpointOfPeerClosure(
    mojo::InterfaceId id) {
  if (!mojo::IsValidInterfaceId(id) || mojo::IsPrimaryInterfaceId(id)) {
    return;
  }
  router_->notify_peer_closed(id);
}

mojo::InterfaceEndpointController*
RustAssociatedGroupController::AttachEndpointClient(
    const mojo::ScopedInterfaceEndpointHandle& handle,
    mojo::InterfaceEndpointClient* endpoint_client,
    scoped_refptr<base::SequencedTaskRunner> runner) {
  NOTREACHED();
}

void RustAssociatedGroupController::DetachEndpointClient(
    const mojo::ScopedInterfaceEndpointHandle& handle) {
  // The only thing we need to happen here is to invalidate
  // weak pointers, which happens automatically.
  // TODO(crbug.com/524990003): Detaching an endpoint without closing it is not
  // yet supported by Rust.
}

void RustAssociatedGroupController::RaiseError() {
  // Raising an error on an associated interface requires tearing down the
  // entire underlying message pipe. Unfortunately, this isn't supported
  // by Rust's ownership model, since the pipe is owned by the primary
  // endpoint. We could add a Mutex, but that slows down every single
  // message call, so leaving this unimplemented for now
  NOTIMPLEMENTED_LOG_ONCE();
}

// Rust only supports fully-serialized messages.
bool RustAssociatedGroupController::PrefersSerializedMessages() {
  return true;
}

bool RustAssociatedGroupController::AssociatePeer(
    mojo::ScopedInterfaceEndpointHandle handle,
    mojo::InterfaceId id) {
  CHECK(handle.pending_association());
  CHECK(mojo::IsValidInterfaceId(id) && !mojo::IsPrimaryInterfaceId(id));
  return NotifyAssociation(&handle, id);
}

}  // namespace mojo::rust::bindings
