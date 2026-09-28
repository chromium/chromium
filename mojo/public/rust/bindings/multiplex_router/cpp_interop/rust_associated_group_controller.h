// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_RUST_BINDINGS_MULTIPLEX_ROUTER_CPP_INTEROP_RUST_ASSOCIATED_GROUP_CONTROLLER_H_
#define MOJO_PUBLIC_RUST_BINDINGS_MULTIPLEX_ROUTER_CPP_INTEROP_RUST_ASSOCIATED_GROUP_CONTROLLER_H_

#include <memory>
#include <optional>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "mojo/public/cpp/bindings/associated_group_controller.h"
#include "mojo/public/cpp/bindings/disconnect_reason.h"
#include "mojo/public/cpp/bindings/interface_endpoint_client.h"
#include "mojo/public/cpp/bindings/interface_endpoint_controller.h"
#include "mojo/public/cpp/bindings/interface_id.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/scoped_interface_endpoint_handle.h"
#include "mojo/public/rust/system/scoped_handle_interop.h"
#include "third_party/rust/cxx/v1/cxx.h"

namespace mojo::rust::bindings {

struct MultiplexRouter;
class RustAssociatedGroupController;

// Implements mojo::InterfaceEndpointController for a specific associated
// endpoint backed by a Rust MultiplexRouter.
class RustAssociatedEndpointClient : public mojo::InterfaceEndpointController {
 public:
  RustAssociatedEndpointClient(
      scoped_refptr<RustAssociatedGroupController> controller,
      mojo::InterfaceId id,
      base::WeakPtr<mojo::InterfaceEndpointClient> client,
      scoped_refptr<base::SequencedTaskRunner> runner);
  ~RustAssociatedEndpointClient() override;

  // mojo::InterfaceEndpointController implementation:
  bool SendMessage(mojo::Message* message) override;
  void AllowWokenUpBySyncWatchOnSameThread() override;
  bool SyncWatch(const bool& should_stop) override;
  bool SyncWatchExclusive(uint64_t request_id) override;
  void RegisterExternalSyncWaiter(uint64_t request_id) override;

  // Passes an incoming message from the Rust router to the C++ interface
  // client. Returns true if the message passed validation and was dispatched
  // to the endpoint, or false if Mojom validation or deserialization failed.
  bool DispatchIncomingMessage(std::unique_ptr<mojo::Message> message) const;
  void DispatchDisconnect() const;

 private:
  const scoped_refptr<RustAssociatedGroupController> controller_;
  const mojo::InterfaceId id_;
  // `client_` is owned by the C++ AssociatedReceiver or AssociatedRemote and is
  // bound to `runner_`. `this` is kept alive by an Arc in Rust's
  // MultiplexRouter handlers. When `client_` is destroyed, its WeakPtrFactory
  // invalidates `client_`. When the handle is closed, `CloseEndpointHandle`
  // removes the entry from the map. Any tasks already posted to `runner_`
  // will no-op after client is invalidated.
  base::WeakPtr<mojo::InterfaceEndpointClient> client_;
  const scoped_refptr<base::SequencedTaskRunner> runner_;
};

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

  // Forwards an outgoing message from a C++ associated endpoint into the Rust
  // MultiplexRouter to be sent over the primary pipe. Returns true if the
  // message was successfully queued, or false if the pipe or endpoint is
  // closed.
  bool SendMessage(mojo::InterfaceId id,
                   mojo::ScopedMessageHandle message_handle);

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

// Called from Rust to deliver an incoming message to a C++ associated endpoint.
// Returns true on success, or false if the message failed validation.
bool run_cpp_incoming_handler(const RustAssociatedEndpointClient& client,
                              std::unique_ptr<mojo::Message> message);

// Notifies the C++ associated endpoint that the its other endpoint has
// disconnected.
void run_cpp_disconnect_handler(const RustAssociatedEndpointClient& client);

}  // namespace mojo::rust::bindings

#endif  // MOJO_PUBLIC_RUST_BINDINGS_MULTIPLEX_ROUTER_CPP_INTEROP_RUST_ASSOCIATED_GROUP_CONTROLLER_H_
