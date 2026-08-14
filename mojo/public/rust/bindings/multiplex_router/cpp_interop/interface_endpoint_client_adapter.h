// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_RUST_BINDINGS_MULTIPLEX_ROUTER_CPP_INTEROP_INTERFACE_ENDPOINT_CLIENT_ADAPTER_H_
#define MOJO_PUBLIC_RUST_BINDINGS_MULTIPLEX_ROUTER_CPP_INTEROP_INTERFACE_ENDPOINT_CLIENT_ADAPTER_H_

#include <memory>
#include <optional>

#include "base/memory/raw_ptr_exclusion.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/task/sequenced_task_runner.h"
#include "mojo/public/cpp/bindings/associated_group_controller.h"
#include "mojo/public/cpp/bindings/interface_endpoint_client.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/scoped_interface_endpoint_handle.h"
#include "mojo/public/rust/system/scoped_handle_interop.h"
#include "third_party/rust/cxx/v1/cxx.h"

// `InterfaceEndpointClientAdapter` connects C++'s routing infrastructure to
// Rust's associated endpoint handlers.
//
// In C++, associated interfaces are multiplexed on a primary message pipe using
// an `InterfaceEndpointClient`. `InterfaceEndpointClientAdapter` acts as the
// C++ `MessageReceiver` for the endpoint: it passes incoming Mojo IPC messages
// into a Rust message handler, and sends outgoing messages from Rust via the
// underlying pipe.
//
// Because Rust endpoints may be dropped or closed on arbitrary background
// threads, `InterfaceEndpointClientAdapter` inherits from
// `base::RefCountedDeleteOnSequence`. This guarantees that disconnect
// callbacks, teardowns, and destruction of Mojo C++ client state always run on
// the endpoint's target `SequencedTaskRunner`.

namespace mojo::rust::bindings {

// Defined in Rust, exposed in the cxx bridge
struct RustAssociatedEndpointState;

class InterfaceEndpointClientAdapter
    : public base::RefCountedDeleteOnSequence<InterfaceEndpointClientAdapter>,
      public mojo::MessageReceiverWithResponderStatus {
 public:
  InterfaceEndpointClientAdapter(
      mojo::ScopedInterfaceEndpointHandle handle,
      ::rust::Box<RustAssociatedEndpointState> state,
      scoped_refptr<base::SequencedTaskRunner> runner);

  // Receives an incoming one-way IPC message from InterfaceEndpointClient, and
  // invokes the Rust incoming callback without a responder.
  bool Accept(mojo::Message* message) override;

  // Receives an incoming request IPC message from InterfaceEndpointClient with
  // a responder, and invokes the Rust incoming callback with the responder
  // wrapper.
  bool AcceptWithResponder(
      mojo::Message* message,
      std::unique_ptr<mojo::MessageReceiverWithStatus> responder) override;

  // Invoked by InterfaceEndpointClient on pipe disconnection or error; calls
  // the Rust disconnect callback.
  void OnConnectionError();

  // Unwraps an outgoing message handle from Rust and passes it to
  // InterfaceEndpointClient::Accept() to send over the pipe.
  void SendMessage(
      std::unique_ptr<mojo::rust::ScopedMessageHandleWrapper> message_wrapper);

  uint32_t id() const { return id_; }

  mojo::AssociatedGroupController* group_controller() const {
    return group_controller_.get();
  }

  base::SequencedTaskRunner* task_runner() const { return task_runner_.get(); }

  // Resets the client, closing the endpoint and sending a disconnect
  // notification over the IPC pipe immediately.
  void Close();

 private:
  friend class base::RefCountedDeleteOnSequence<InterfaceEndpointClientAdapter>;
  friend class base::DeleteHelper<InterfaceEndpointClientAdapter>;

  ~InterfaceEndpointClientAdapter() override;

  // Rust performs its own validation, so don't bother doing anything in C++.
  class NoOpValidator : public mojo::MessageReceiver {
   public:
    bool Accept(mojo::Message* message) override;
  };

  // The associated interface ID of this endpoint
  uint32_t id_;

  // Pointer to data that Rust needs to run its handlers
  std::optional<::rust::Box<RustAssociatedEndpointState>> state_;

  // Sequence on which to run methods
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // The actual underlying C++ object that manages routing. Used so that our
  // enclosing `AssociatedEndpointRustAdapter` can register new associated
  // endpoints.
  scoped_refptr<mojo::AssociatedGroupController> group_controller_;

  // A connection to the group controller that's specific to this associated
  // interface. Embeds the interface ID; for sending and receiving messages.
  mojo::InterfaceEndpointClient client_;
};

}  // namespace mojo::rust::bindings

#endif  // MOJO_PUBLIC_RUST_BINDINGS_MULTIPLEX_ROUTER_CPP_INTEROP_INTERFACE_ENDPOINT_CLIENT_ADAPTER_H_
