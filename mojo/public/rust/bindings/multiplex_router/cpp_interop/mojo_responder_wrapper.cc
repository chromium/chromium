// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implements `MojoResponderWrapper` and `ResponderHolder` for thread-safe C++
// IPC response delivery.

#include "mojo/public/rust/bindings/multiplex_router/cpp_interop/mojo_responder_wrapper.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "mojo/public/cpp/bindings/interface_id.h"
#include "mojo/public/cpp/bindings/lib/responder_thunk.h"
#include "mojo/public/rust/bindings/multiplex_router/cpp_interop/associated_endpoint_rust_adapter.h"

namespace mojo::rust::bindings {

// Concrete target object held inside `base::SequenceBound<ResponderHolder>`.
// Accepts incoming message wrappers from Rust, converts them into C++
// `mojo::Message` instances, and calls `ResponderThunk::Accept` on
// the bound sequence.
class MojoResponderWrapper::ResponderHolder {
 public:
  explicit ResponderHolder(
      std::unique_ptr<mojo::internal::ResponderThunk> responder)
      : responder_(std::move(responder)) {}

  void Accept(std::unique_ptr<mojo::Message> message) {
    auto responder = std::move(responder_);
    std::ignore = responder->Accept(message.get());
  }

 private:
  std::unique_ptr<mojo::internal::ResponderThunk> responder_;
};

MojoResponderWrapper::MojoResponderWrapper(
    std::unique_ptr<mojo::internal::ResponderThunk> responder,
    scoped_refptr<base::SequencedTaskRunner> runner,
    scoped_refptr<mojo::AssociatedGroupController> group_controller,
    std::vector<mojo::ScopedInterfaceEndpointHandle> associated_handles)
    : responder_(
          responder ? base::SequenceBound<ResponderHolder>(runner,
                                                           std::move(responder))
                    : base::SequenceBound<ResponderHolder>()),
      group_controller_(std::move(group_controller)),
      associated_handles_(std::move(associated_handles)) {}

MojoResponderWrapper::~MojoResponderWrapper() = default;

// Sends a response message using the wrapped C++ responder.
void MojoResponderWrapper::Accept(
    std::unique_ptr<mojo::Message> message) const {
  CHECK(responder_);
  responder_.AsyncCall(&ResponderHolder::Accept).WithArgs(std::move(message));
  responder_.Reset();
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
  // If an interface ID was provided, it must be an endpoint from the incoming
  // message, already extracted into `associated_handles_`.
  if (interface_id != mojo::kInvalidInterfaceId) {
    auto it = std::ranges::find_if(associated_handles_,
                                   [interface_id](const auto& handle) {
                                     return handle.id() == interface_id;
                                   });
    if (it == associated_handles_.end()) {
      return nullptr;
    }
    auto handle = std::move(*it);
    associated_handles_.erase(it);
    return std::make_unique<AssociatedEndpointRustAdapter>(std::move(handle));
  }

  // If we got an invalid interface ID, register a new one
  auto adapter = std::make_unique<AssociatedEndpointRustAdapter>(
      group_controller_.get(), mojo::kInvalidInterfaceId);
  return adapter->is_valid() ? std::move(adapter) : nullptr;
}

uint32_t MojoResponderWrapper::AssociateInterface(
    std::unique_ptr<AssociatedEndpointRustAdapter> endpoint) const {
  if (!group_controller_ || !endpoint) {
    return mojo::kInvalidInterfaceId;
  }
  return group_controller_->AssociateInterface(endpoint->PassHandle());
}

}  // namespace mojo::rust::bindings
