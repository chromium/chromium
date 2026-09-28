// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_RUST_BINDINGS_MULTIPLEX_ROUTER_CPP_INTEROP_MOJO_RESPONDER_WRAPPER_H_
#define MOJO_PUBLIC_RUST_BINDINGS_MULTIPLEX_ROUTER_CPP_INTEROP_MOJO_RESPONDER_WRAPPER_H_

#include <memory>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "mojo/public/cpp/bindings/associated_group_controller.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/scoped_interface_endpoint_handle.h"
#include "mojo/public/rust/system/scoped_handle_interop.h"

namespace mojo::internal {
class ResponderThunk;
}

namespace mojo::rust::bindings {

class AssociatedEndpointRustAdapter;

// A struct which allows Rust to respond to a message using a C++-provided
// `ResponderThunk`, and to register new endpoints with the C++
// message pipe.
//
// This is an analogue to the Rust `ResponseSender` type, but uses C++ machinery
// instead of Rust.
//
// Uses `base::SequenceBound` to ensure responses and destruction occur safely
// on the bound C++ sequence.
//
// TODO(crbug.com/542170149): This class mostly exists because we can't pass its
// contents across cxx directly, so it's a good candidate for replacement once
// crubit is supported.
class MojoResponderWrapper {
 public:
  // Construct a responder wrapper. Note that it is valid for
  // `responder` to be null.
  MojoResponderWrapper(
      std::unique_ptr<mojo::internal::ResponderThunk> responder,
      scoped_refptr<base::SequencedTaskRunner> runner,
      scoped_refptr<mojo::AssociatedGroupController> group_controller,
      std::vector<mojo::ScopedInterfaceEndpointHandle> associated_handles = {});
  ~MojoResponderWrapper();

  MojoResponderWrapper(const MojoResponderWrapper&) = delete;
  MojoResponderWrapper& operator=(const MojoResponderWrapper&) = delete;

  // Sends a response message using the wrapped C++ responder.
  // `CanSendResponse()` must be true, and `message` must not be null.
  // May only be called once.
  void Accept(std::unique_ptr<mojo::Message> message) const;

  // Returns true if this wrapper can be used to send a response message.
  // If false, it can only be used to register new endpoints.
  bool CanSendResponse() const;

  // Register a new associated interface with the underlying router. If
  // `interface_id` is `mojo::kInvalidInterfaceId`, a new interface ID
  // will be created; otherwise `interface_id` is used. Returns a new adapter
  // wrapping the endpoint, or nullptr on failure.
  std::unique_ptr<AssociatedEndpointRustAdapter> RegisterNewEndpoint(
      uint32_t interface_id) const;

 private:
  class ResponderHolder;

  // Mutable so that we don't need to plumb through a mutable
  // reference in Rust. To avoid races, it's important that
  // MojoResponderWrapper doesn't implement `Sync` in Rust
  // (though `Send` is okay).
  // This can be null (for messages that don't expect a response),
  // so all methods must make sure to check it before referencing.
  mutable base::SequenceBound<ResponderHolder> responder_;
  scoped_refptr<mojo::AssociatedGroupController> group_controller_;
  // This contains any associated interfaces that were contained in the message;
  // the C++ has already parsed them and wired them to its own infrastructure,
  // but Rust will need to get these objects when they're registered with the
  // router. Mutable for the same reason as `responder_`.
  mutable std::vector<mojo::ScopedInterfaceEndpointHandle> associated_handles_;
};

}  // namespace mojo::rust::bindings

#endif  // MOJO_PUBLIC_RUST_BINDINGS_MULTIPLEX_ROUTER_CPP_INTEROP_MOJO_RESPONDER_WRAPPER_H_
