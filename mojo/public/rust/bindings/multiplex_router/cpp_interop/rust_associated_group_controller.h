// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_RUST_BINDINGS_MULTIPLEX_ROUTER_CPP_INTEROP_RUST_ASSOCIATED_GROUP_CONTROLLER_H_
#define MOJO_PUBLIC_RUST_BINDINGS_MULTIPLEX_ROUTER_CPP_INTEROP_RUST_ASSOCIATED_GROUP_CONTROLLER_H_

#include <optional>

#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "mojo/public/cpp/bindings/associated_group_controller.h"
#include "mojo/public/cpp/bindings/disconnect_reason.h"
#include "mojo/public/cpp/bindings/interface_endpoint_client.h"
#include "mojo/public/cpp/bindings/interface_endpoint_controller.h"
#include "mojo/public/cpp/bindings/interface_id.h"
#include "mojo/public/cpp/bindings/scoped_interface_endpoint_handle.h"
#include "third_party/rust/cxx/v1/cxx.h"

namespace mojo::rust::bindings {

struct MultiplexRouter;

// Implements mojo::AssociatedGroupController for associated interfaces backed
// by a Rust MultiplexRouter.
class RustAssociatedGroupController : public mojo::AssociatedGroupController {
 public:
  RustAssociatedGroupController(
      ::rust::Box<MultiplexRouter> router,
      scoped_refptr<base::SequencedTaskRunner> task_runner);

  mojo::InterfaceId AssociateInterface(
      mojo::ScopedInterfaceEndpointHandle handle_to_send) override;

  // Registers `id` with the Rust router.
  //
  // Returns an invalid ScopedInterfaceEndpointHandle if `id` is invalid, refers
  // to the primary endpoint (which Rust owns), or was already registered.
  mojo::ScopedInterfaceEndpointHandle CreateLocalEndpointHandle(
      mojo::InterfaceId id) override;

  // Creates a ScopedInterfaceEndpointHandle for an ID that has already
  // been created and registered on the Rust side.
  //
  // Returns an invalid ScopedInterfaceEndpointHandle if `id` is invalid or
  // refers to the primary endpoint.
  mojo::ScopedInterfaceEndpointHandle CreateScopedHandleForExistingEndpoint(
      mojo::InterfaceId id);

  void CloseEndpointHandle(
      mojo::InterfaceId id,
      const std::optional<mojo::DisconnectReason>& reason) override;

  void NotifyLocalEndpointOfPeerClosure(mojo::InterfaceId id) override;

  mojo::InterfaceEndpointController* AttachEndpointClient(
      const mojo::ScopedInterfaceEndpointHandle& handle,
      mojo::InterfaceEndpointClient* endpoint_client,
      scoped_refptr<base::SequencedTaskRunner> runner) override;

  void DetachEndpointClient(
      const mojo::ScopedInterfaceEndpointHandle& handle) override;

  void RaiseError() override;

  bool PrefersSerializedMessages() override;

  // Associates `handle`'s peer with `id`. `handle` must still be pending
  // association, and `id` must be a valid non-primary ID.
  bool AssociatePeer(mojo::ScopedInterfaceEndpointHandle handle,
                     mojo::InterfaceId id);

 protected:
  ~RustAssociatedGroupController() override;

 private:
  ::rust::Box<MultiplexRouter> router_;
};

// Creates a group controller which forwards to `router`, and which will be
// destroyed on `runner`'s sequence. Returns a pointer which owns one reference
// to it.
RustAssociatedGroupController* CreateGroupControllerForRustRouter(
    ::rust::Box<MultiplexRouter> router,
    base::SequencedTaskRunner& runner);

}  // namespace mojo::rust::bindings

#endif  // MOJO_PUBLIC_RUST_BINDINGS_MULTIPLEX_ROUTER_CPP_INTEROP_RUST_ASSOCIATED_GROUP_CONTROLLER_H_
