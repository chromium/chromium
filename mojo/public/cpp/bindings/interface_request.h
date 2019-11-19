// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_INTERFACE_REQUEST_H_
#define MOJO_PUBLIC_CPP_BINDINGS_INTERFACE_REQUEST_H_

#include <cstddef>
#include <string>
#include <utility>

#include "base/macros.h"
#include "base/optional.h"
#include "base/sequenced_task_runner.h"
#include "mojo/public/cpp/bindings/connection_group.h"
#include "mojo/public/cpp/bindings/disconnect_reason.h"
#include "mojo/public/cpp/bindings/interface_ptr.h"
#include "mojo/public/cpp/bindings/lib/bindings_internal.h"
#include "mojo/public/cpp/bindings/lib/pending_receiver_state.h"
#include "mojo/public/cpp/bindings/lib/serialization_context.h"
#include "mojo/public/cpp/bindings/pipe_control_message_proxy.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace mojo {

// Represents a request from a remote client for an implementation of Interface
// over a specified message pipe. The implementor of the interface should
// remove the message pipe by calling PassMessagePipe() and bind it to the
// implementation. If this is not done, the InterfaceRequest will automatically
// close the pipe on destruction. Can also represent the absence of a request
// if the client did not provide a message pipe.
template <typename Interface>
class InterfaceRequest {
 public:
  // Constructs an empty InterfaceRequest, representing that the client is not
  // requesting an implementation of Interface.
  InterfaceRequest() {}
  InterfaceRequest(std::nullptr_t) {}

  explicit InterfaceRequest(ScopedMessagePipeHandle handle)
      : state_(std::move(handle)) {}

  // Takes the message pipe from another InterfaceRequest.
  InterfaceRequest(InterfaceRequest&& other) = default;

  InterfaceRequest& operator=(InterfaceRequest&& other) = default;

  // Assigning to nullptr resets the InterfaceRequest to an empty state,
  // closing the message pipe currently bound to it (if any).
  InterfaceRequest& operator=(std::nullptr_t) {
    state_.reset();
    return *this;
  }

  // Indicates whether the request currently contains a valid message pipe.
  bool is_pending() const { return state_.pipe.is_valid(); }

  explicit operator bool() const { return is_pending(); }

  // Removes the message pipe from the request and returns it.
  ScopedMessagePipeHandle PassMessagePipe() { return std::move(state_.pipe); }

  bool Equals(const InterfaceRequest& other) const {
    if (this == &other)
      return true;

    // Now that the two refer to different objects, they are equivalent if
    // and only if they are both invalid.
    return !is_pending() && !other.is_pending();
  }

  void ResetWithReason(uint32_t custom_reason, const std::string& description) {
    if (!is_pending())
      return;

    Message message =
        PipeControlMessageProxy::ConstructPeerEndpointClosedMessage(
            kMasterInterfaceId, DisconnectReason(custom_reason, description));
    MojoResult result =
        WriteMessageNew(state_.pipe.get(), message.TakeMojoMessage(),
                        MOJO_WRITE_MESSAGE_FLAG_NONE);
    DCHECK_EQ(MOJO_RESULT_OK, result);

    state_.reset();
  }

  // Assigns this InterfaceRequest to the ConnectionGroup referenced by |ref|.
  // Any Receiver which binds this InterfaceRequest will inherit the Ref.
  void set_connection_group(ConnectionGroup::Ref ref) {
    state_.connection_group = std::move(ref);
  }

  const ConnectionGroup::Ref& connection_group() const {
    return state_.connection_group;
  }

  // Passes ownership of this InterfaceRequest's ConnectionGroup Ref, removing
  // it from its group.
  ConnectionGroup::Ref PassConnectionGroupRef() {
    return std::move(state_.connection_group);
  }

  // For internal Mojo use only.
  internal::PendingReceiverState* internal_state() { return &state_; }

 private:
  internal::PendingReceiverState state_;

  DISALLOW_COPY_AND_ASSIGN(InterfaceRequest);
};

// Creates a new message pipe over which Interface is to be served. Binds the
// specified InterfacePtr to one end of the message pipe, and returns an
// InterfaceRequest bound to the other. The InterfacePtr should be passed to
// the client, and the InterfaceRequest should be passed to whatever will
// provide the implementation. The implementation should typically be bound to
// the InterfaceRequest using the Binding or StrongBinding classes. The client
// may begin to issue calls even before an implementation has been bound, since
// messages sent over the pipe will just queue up until they are consumed by
// the implementation.
//
// Example #1: Requesting a remote implementation of an interface.
// ===============================================================
//
// Given the following interface:
//
//   interface Database {
//     OpenTable(Table& table);
//   }
//
// The client would have code similar to the following:
//
//   DatabasePtr database = ...;  // Connect to database.
//   TablePtr table;
//   database->OpenTable(MakeRequest(&table));
//
// Upon return from MakeRequest, |table| is ready to have methods called on it.
//
// Example #2: Registering a local implementation with a remote service.
// =====================================================================
//
// Given the following interface
//   interface Collector {
//     RegisterSource(Source source);
//   }
//
// The client would have code similar to the following:
//
//   CollectorPtr collector = ...;  // Connect to Collector.
//   SourcePtr source;
//   InterfaceRequest<Source> source_request(&source);
//   collector->RegisterSource(std::move(source));
//   CreateSource(std::move(source_request));  // Create implementation locally.
//
template <typename Interface>
InterfaceRequest<Interface> MakeRequest(
    InterfacePtr<Interface>* ptr,
    scoped_refptr<base::SequencedTaskRunner> runner = nullptr) {
  MessagePipe pipe;
  ptr->Bind(InterfacePtrInfo<Interface>(std::move(pipe.handle0), 0u),
            std::move(runner));
  return InterfaceRequest<Interface>(std::move(pipe.handle1));
}

// Similar to the constructor above, but binds one end of the message pipe to
// an InterfacePtrInfo instance.
template <typename Interface>
InterfaceRequest<Interface> MakeRequest(InterfacePtrInfo<Interface>* ptr_info) {
  MessagePipe pipe;
  ptr_info->set_handle(std::move(pipe.handle0));
  ptr_info->set_version(0u);
  return InterfaceRequest<Interface>(std::move(pipe.handle1));
}

// Fuses an InterfaceRequest<T> endpoint with an InterfacePtrInfo<T> endpoint.
// Returns |true| on success or |false| on failure.
template <typename Interface>
bool FuseInterface(InterfaceRequest<Interface> request,
                   InterfacePtrInfo<Interface> proxy_info) {
  MojoResult result = FuseMessagePipes(request.PassMessagePipe(),
                                       proxy_info.PassHandle());
  return result == MOJO_RESULT_OK;
}

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_INTERFACE_REQUEST_H_
