// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_RUST_BINDINGS_MULTIPLEX_ROUTER_CPP_INTEROP_MOJO_RESPONDER_WRAPPER_H_
#define MOJO_PUBLIC_RUST_BINDINGS_MULTIPLEX_ROUTER_CPP_INTEROP_MOJO_RESPONDER_WRAPPER_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/rust/system/scoped_handle_interop.h"

namespace mojo::rust::bindings {

// A struct which allows Rust to respond to a message using a C++-provided
// `MessageReceiverWithStatus`.
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
      scoped_refptr<base::SequencedTaskRunner> runner);
  ~MojoResponderWrapper();

  MojoResponderWrapper(const MojoResponderWrapper&) = delete;
  MojoResponderWrapper& operator=(const MojoResponderWrapper&) = delete;

  bool Accept(std::unique_ptr<mojo::rust::ScopedMessageHandleWrapper>
                  message_wrapper) const;

 private:
  class ResponderHolder;
  base::SequenceBound<ResponderHolder> responder_;
};

}  // namespace mojo::rust::bindings

#endif  // MOJO_PUBLIC_RUST_BINDINGS_MULTIPLEX_ROUTER_CPP_INTEROP_MOJO_RESPONDER_WRAPPER_H_
