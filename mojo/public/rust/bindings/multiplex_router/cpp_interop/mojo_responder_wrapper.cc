// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implements `MojoResponderWrapper` and `ResponderHolder` for thread-safe C++
// IPC response delivery.

#include "mojo/public/rust/bindings/multiplex_router/cpp_interop/mojo_responder_wrapper.h"

#include <utility>

#include "mojo/public/cpp/bindings/interface_id.h"
#include "mojo/public/rust/bindings/multiplex_router/cpp_interop/associated_endpoint_rust_adapter.h"

namespace mojo::rust::bindings {

// Concrete target object held inside `base::SequenceBound<ResponderHolder>`.
// Accepts incoming message wrappers from Rust, converts them into C++
// `mojo::Message` instances, and calls `MessageReceiverWithStatus::Accept` on
// the bound sequence.
class MojoResponderWrapper::ResponderHolder {
 public:
  explicit ResponderHolder(
      std::unique_ptr<mojo::MessageReceiverWithStatus> responder)
      : responder_(std::move(responder)) {}

  void Accept(
      std::unique_ptr<mojo::rust::ScopedMessageHandleWrapper> msg_wrapper) {
    if (!responder_ || !msg_wrapper) {
      return;
    }
    mojo::ScopedMessageHandle handle = msg_wrapper->take_handle();
    mojo::Message message = mojo::Message::CreateFromMessageHandle(&handle);
    std::ignore = responder_->Accept(&message);
  }

 private:
  std::unique_ptr<mojo::MessageReceiverWithStatus> responder_;
};

MojoResponderWrapper::MojoResponderWrapper(
    std::unique_ptr<mojo::MessageReceiverWithStatus> responder,
    scoped_refptr<base::SequencedTaskRunner> runner,
    scoped_refptr<mojo::AssociatedGroupController> group_controller)
    : responder_(
          responder ? base::SequenceBound<ResponderHolder>(runner,
                                                           std::move(responder))
                    : base::SequenceBound<ResponderHolder>()),
      group_controller_(std::move(group_controller)) {}

MojoResponderWrapper::~MojoResponderWrapper() = default;

// Sends a response message using the wrapped C++ responder.
bool MojoResponderWrapper::Accept(
    std::unique_ptr<mojo::rust::ScopedMessageHandleWrapper> message_wrapper)
    const {
  if (!message_wrapper || !responder_) {
    return false;
  }
  responder_.AsyncCall(&ResponderHolder::Accept)
      .WithArgs(std::move(message_wrapper));
  return true;
}

// Returns true if this wrapper can be used to send a response message.
// If false, it can only be used to register new endpoints.
bool MojoResponderWrapper::CanSendResponse() const {
  return !responder_.is_null();
}

// Register a new associated interface with the underlying router. If
// `interface_id` is `mojo::kInvalidInterfaceId`, a new interface ID
// will be created; otherwise `interface_id` is used.
std::unique_ptr<AssociatedEndpointRustAdapter>
MojoResponderWrapper::RegisterNewEndpoint(uint32_t interface_id) const {
  if (!group_controller_) {
    return nullptr;
  }

  mojo::ScopedInterfaceEndpointHandle new_handle;
  if (interface_id == mojo::kInvalidInterfaceId) {
    mojo::ScopedInterfaceEndpointHandle local_handle;
    mojo::ScopedInterfaceEndpointHandle remote_handle;
    mojo::ScopedInterfaceEndpointHandle::CreatePairPendingAssociation(
        &local_handle, &remote_handle);
    group_controller_->AssociateInterface(std::move(remote_handle));
    new_handle = std::move(local_handle);
  } else {
    new_handle = group_controller_->CreateLocalEndpointHandle(
        mojo::InterfaceId(interface_id));
  }

  if (!new_handle.is_valid()) {
    return nullptr;
  }
  return std::make_unique<AssociatedEndpointRustAdapter>(std::move(new_handle));
}

// Returns a copy of this wrapper that can register new endpoints on the
// message pipe but can't send response messages.
std::unique_ptr<MojoResponderWrapper> MojoResponderWrapper::CloneAsRegistrar()
    const {
  return std::make_unique<MojoResponderWrapper>(
      nullptr, scoped_refptr<base::SequencedTaskRunner>(), group_controller_);
}

}  // namespace mojo::rust::bindings
