// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_RUST_BINDINGS_MULTIPLEX_ROUTER_CPP_INTEROP_MOJO_RESPONDER_WRAPPER_H_
#define MOJO_PUBLIC_RUST_BINDINGS_MULTIPLEX_ROUTER_CPP_INTEROP_MOJO_RESPONDER_WRAPPER_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "mojo/public/cpp/bindings/associated_group_controller.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/rust/system/scoped_handle_interop.h"

namespace mojo::rust::bindings {

class AssociatedEndpointRustAdapter;

// A struct which allows Rust to respond to a message using a C++-provided
// `MessageReceiverWithStatus`, and to register new endpoints with the C++
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
  MojoResponderWrapper(
      std::unique_ptr<mojo::MessageReceiverWithStatus> responder,
      scoped_refptr<base::SequencedTaskRunner> runner,
      scoped_refptr<mojo::AssociatedGroupController> group_controller);
  ~MojoResponderWrapper();

  MojoResponderWrapper(const MojoResponderWrapper&) = delete;
  MojoResponderWrapper& operator=(const MojoResponderWrapper&) = delete;

  // Sends a response message using the wrapped C++ responder.
  bool Accept(std::unique_ptr<mojo::rust::ScopedMessageHandleWrapper>
                  message_wrapper) const;

  // Returns true if this wrapper can be used to send a response message.
  // If false, it can only be used to register new endpoints.
  bool CanSendResponse() const;

  // Register a new associated interface with the underlying router. If
  // `interface_id` is `mojo::kInvalidInterfaceId`, a new interface ID
  // will be created; otherwise `interface_id` is used.
  std::unique_ptr<AssociatedEndpointRustAdapter> RegisterNewEndpoint(
      uint32_t interface_id) const;

  // Returns a copy of this wrapper that can register new endpoints with
  // the underlying router, but can't send response messages.
  std::unique_ptr<MojoResponderWrapper> CloneAsRegistrar() const;

 private:
  class ResponderHolder;
  base::SequenceBound<ResponderHolder> responder_;
  scoped_refptr<mojo::AssociatedGroupController> group_controller_;
};

}  // namespace mojo::rust::bindings

#endif  // MOJO_PUBLIC_RUST_BINDINGS_MULTIPLEX_ROUTER_CPP_INTEROP_MOJO_RESPONDER_WRAPPER_H_
