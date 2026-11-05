// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_HANDLE_SERIALIZATION_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_HANDLE_SERIALIZATION_H_

#include "base/component_export.h"
#include "mojo/public/cpp/bindings/lib/bindings_internal.h"
#include "mojo/public/cpp/bindings/lib/serialization_forward.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/platform/platform_handle.h"
#include "mojo/public/cpp/system/handle.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "mojo/public/cpp/system/platform_handle.h"

// This header defines helpers used by generated bindings to stash various types
// of handles and interface endpoints within a Message object while also
// encoding appropriate data within the message to reference the attached
// object as needed.

namespace mojo {
namespace internal {

struct PendingReceiverState;
struct PendingRemoteState;

// Takes ownership of `handle` and transfers it into `message`.
COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE)
void SerializeRawHandle(Handle handle, Message& message, Handle_Data& data);

COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE)
void SerializeHandle(ScopedHandle handle, Message& message, Handle_Data& data);

// Takes ownership of `handle` and transfers it into `message`.
COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE)
void SerializeRawInterfaceInfo(MessagePipeHandle handle,
                               uint32_t version,
                               Message& message,
                               Interface_Data& data);

COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE)
void SerializeInterfaceInfo(ScopedMessagePipeHandle handle,
                            uint32_t version,
                            Message& message,
                            Interface_Data& data);

COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE)
void SerializeAssociatedEndpoint(ScopedInterfaceEndpointHandle handle,
                                 Message& message,
                                 AssociatedEndpointHandle_Data& data);

COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE)
void SerializeAssociatedInterfaceInfo(ScopedInterfaceEndpointHandle handle,
                                      uint32_t version,
                                      Message& message,
                                      AssociatedInterface_Data& data);

// Releases ownership of the handle from `message` and returns it to the caller.
COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE)
Handle DeserializeRawHandle(const Handle_Data& data, Message& message);

COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE)
ScopedHandle DeserializeHandle(const Handle_Data& data, Message& message);

COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE)
void DeserializeHandleAsReceiver(const Handle_Data& data,
                                 Message& message,
                                 PendingReceiverState& receiver_state);

COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE)
void DeserializeHandleAsRemote(const Interface_Data& data,
                               Message& message,
                               PendingRemoteState& remote_state);

COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE)
ScopedInterfaceEndpointHandle DeserializeAssociatedEndpointHandle(
    const AssociatedEndpointHandle_Data& data,
    Message& message);

template <typename T>
ScopedHandleBase<T> DeserializeHandleAs(const Handle_Data& data,
                                        Message& message) {
  return ScopedHandleBase<T>(T(DeserializeRawHandle(data, message).value()));
}

template <typename T>
struct Serializer<ScopedHandleBase<T>, ScopedHandleBase<T>> {
  static void Serialize(ScopedHandleBase<T>& input,
                        Handle_Data* output,
                        Message* message) {
    SerializeRawHandle(Handle(input.release().value()), *message, *output);
  }

  static bool Deserialize(Handle_Data* input,
                          ScopedHandleBase<T>* output,
                          Message* message) {
    output->reset(T(DeserializeRawHandle(*input, *message).value()));
    return true;
  }
};

template <>
struct Serializer<PlatformHandle, PlatformHandle> {
  static void Serialize(PlatformHandle& input,
                        Handle_Data* output,
                        Message* message) {
    [[maybe_unused]] const bool input_was_valid = input.is_valid();
    ScopedHandle handle = WrapPlatformHandle(std::move(input));
    DCHECK_EQ(handle.is_valid(), input_was_valid);
    SerializeRawHandle(handle.release(), *message, *output);
  }

  static bool Deserialize(Handle_Data* input,
                          PlatformHandle* output,
                          Message* message) {
    *output =
        UnwrapPlatformHandle(DeserializeHandleAs<Handle>(*input, *message));
    return true;
  }
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_HANDLE_SERIALIZATION_H_
