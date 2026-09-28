// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/rust/bindings/multiplex_router/cpp_interop/rust_associated_group_controller.h"

#include <utility>
#include <vector>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/notimplemented.h"
#include "base/notreached.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/rust/bindings/multiplex_router/cpp_interop/cxx.rs.h"
#include "third_party/abseil-cpp/absl/container/inlined_vector.h"

namespace mojo::rust::bindings {

RustAssociatedEndpointClient::RustAssociatedEndpointClient(
    scoped_refptr<RustAssociatedGroupController> controller,
    mojo::InterfaceId id,
    base::WeakPtr<mojo::InterfaceEndpointClient> client,
    scoped_refptr<base::SequencedTaskRunner> runner)
    : controller_(std::move(controller)),
      id_(id),
      client_(std::move(client)),
      runner_(std::move(runner)) {
  CHECK(runner_);
}

RustAssociatedEndpointClient::~RustAssociatedEndpointClient() = default;

bool RustAssociatedEndpointClient::SendMessage(mojo::Message* message) {
  CHECK(runner_->RunsTasksInCurrentSequence());
  message->set_interface_id(id_);

  // Save any serialized associated interface IDs before taking the message
  // handle, because TakeMojoMessage() moves it entirely into Rust.
  absl::InlinedVector<uint32_t, 4> ids;
  if (uint32_t num_ids = message->payload_num_interface_ids()) {
    auto span =
        // Safety: this is an outgoing message, so we trust that the contents
        // of the message are valid and have the right offsets.
        UNSAFE_BUFFERS(base::span(message->payload_interface_ids(), num_ids));
    ids.assign(span.begin(), span.end());
  }

  bool sent = controller_->SendMessage(id_, message->TakeMojoMessage());
  // If the message failed to send, clean up any associated interface IDs
  // that were contained in the body. We can't rely on the normal cleanup
  // mechanism because Rust owns the message now.
  if (!sent) {
    for (uint32_t endpoint_id : ids) {
      controller_->NotifyLocalEndpointOfPeerClosure(endpoint_id);
    }
    // If sending fails, InterfaceEndpointClient expects the message
    // field to still be valid, so provide a dummy value.
    *message = mojo::Message(0, 0, 0, 0, nullptr);
    return false;
  }
  return true;
}

void RustAssociatedEndpointClient::AllowWokenUpBySyncWatchOnSameThread() {
  NOTREACHED() << "Rust doesn't support sync calls yet";
}

bool RustAssociatedEndpointClient::SyncWatch(const bool& should_stop) {
  NOTREACHED() << "Rust doesn't support sync calls yet";
}

bool RustAssociatedEndpointClient::SyncWatchExclusive(uint64_t request_id) {
  NOTREACHED() << "Rust doesn't support sync calls yet";
}

void RustAssociatedEndpointClient::RegisterExternalSyncWaiter(
    uint64_t request_id) {
  NOTREACHED() << "Rust doesn't support sync calls yet";
}

bool RustAssociatedEndpointClient::DispatchIncomingMessage(
    std::unique_ptr<mojo::Message> message) const {
  CHECK(runner_->RunsTasksInCurrentSequence());
  if (!message) {
    return false;
  }
  if (!client_) {
    // The client was destroyed after this message was queued.
    // `false` means something went terribly wrong, not that we
    // merely failed to deliver the message, so return `true`.
    return true;
  }
  if (!message->DeserializeAssociatedEndpointHandles(controller_.get())) {
    message->NotifyBadMessage(
        "Failed to deserialize associated endpoint handles: duplicate or "
        "invalid interface ID");
    return false;
  }
  return client_->HandleIncomingMessage(message.get());
}

void RustAssociatedEndpointClient::DispatchDisconnect() const {
  CHECK(runner_->RunsTasksInCurrentSequence());
  if (!client_) {
    return;
  }
  client_->NotifyError(std::nullopt);
}

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
  CHECK(endpoint_client);
  CHECK(runner);
  CHECK(mojo::IsValidInterfaceId(handle.id()) &&
        !mojo::IsPrimaryInterfaceId(handle.id()));
  // Fail early if someone tries to do a sync call,
  // since Rust doesn't support them
  CHECK(endpoint_client->sync_method_ordinals().empty())
      << "Rust doesn't support sync calls yet";

  auto endpoint = std::make_unique<RustAssociatedEndpointClient>(
      this, handle.id(), endpoint_client->GetWeakPtr(), runner);
  auto* controller_ptr = endpoint.get();
  attach_cpp_endpoint(*router_, handle.id(), std::move(endpoint), *runner);
  return controller_ptr;
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

bool RustAssociatedGroupController::SendMessage(
    mojo::InterfaceId id,
    mojo::ScopedMessageHandle message_handle) {
  auto wrapper = std::make_unique<mojo::rust::ScopedMessageHandleWrapper>(
      std::move(message_handle));
  return send_message_from_cpp(*router_, id, std::move(wrapper));
}

bool run_cpp_incoming_handler(const RustAssociatedEndpointClient& client,
                              std::unique_ptr<mojo::Message> message) {
  return client.DispatchIncomingMessage(std::move(message));
}

void run_cpp_disconnect_handler(const RustAssociatedEndpointClient& client) {
  client.DispatchDisconnect();
}

}  // namespace mojo::rust::bindings
