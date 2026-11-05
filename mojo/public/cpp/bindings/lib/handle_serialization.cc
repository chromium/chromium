// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/lib/handle_serialization.h"

#include "base/numerics/safe_conversions.h"
#include "mojo/public/cpp/bindings/lib/bindings_internal.h"
#include "mojo/public/cpp/bindings/lib/pending_receiver_state.h"
#include "mojo/public/cpp/bindings/lib/pending_remote_state.h"

namespace mojo {
namespace internal {

void SerializeRawHandle(Handle handle, Message& message, Handle_Data& data) {
  if (!handle.is_valid()) {
    data.value = kEncodedInvalidHandleValue;
    return;
  }

  auto& handles = *message.mutable_handles();
  data.value = base::checked_cast<uint32_t>(handles.size());
  handles.emplace_back(handle);
}

void SerializeHandle(ScopedHandle handle, Message& message, Handle_Data& data) {
  SerializeRawHandle(handle.release(), message, data);
}

NOINLINE void SerializeRawInterfaceInfo(MessagePipeHandle handle,
                                        uint32_t version,
                                        Message& message,
                                        Interface_Data& data) {
  SerializeRawHandle(Handle(handle.value()), message, data.handle);
  data.version = version;
}

void SerializeInterfaceInfo(ScopedMessagePipeHandle handle,
                            uint32_t version,
                            Message& message,
                            Interface_Data& data) {
  SerializeRawInterfaceInfo(handle.release(), version, message, data);
}

void SerializeAssociatedEndpoint(ScopedInterfaceEndpointHandle handle,
                                 Message& message,
                                 AssociatedEndpointHandle_Data& data) {
  if (!handle.is_valid()) {
    data.value = kEncodedInvalidHandleValue;
    return;
  }

  auto& handles = *message.mutable_associated_endpoint_handles();
  data.value = base::checked_cast<uint32_t>(handles.size());
  handles.push_back(std::move(handle));
}

NOINLINE void SerializeAssociatedInterfaceInfo(
    ScopedInterfaceEndpointHandle handle,
    uint32_t version,
    Message& message,
    AssociatedInterface_Data& data) {
  SerializeAssociatedEndpoint(std::move(handle), message, data.handle);
  data.version = version;
}

NOINLINE Handle DeserializeRawHandle(const Handle_Data& data,
                                     Message& message) {
  if (!data.is_valid()) {
    return {};
  }

  auto& handles = *message.mutable_handles();
  DCHECK_LT(data.value, handles.size());
  return handles[data.value].release();
}

ScopedHandle DeserializeHandle(const Handle_Data& data, Message& message) {
  return ScopedHandle(DeserializeRawHandle(data, message));
}

NOINLINE void DeserializeHandleAsReceiver(
    const Handle_Data& data,
    Message& message,
    PendingReceiverState& receiver_state) {
  receiver_state.pipe.reset(
      MessagePipeHandle(DeserializeRawHandle(data, message).value()));
  if (message.receiver_connection_group()) {
    receiver_state.connection_group = *message.receiver_connection_group();
  }
}

NOINLINE void DeserializeHandleAsRemote(const Interface_Data& data,
                                        Message& message,
                                        PendingRemoteState& remote_state) {
  remote_state.pipe.reset(
      MessagePipeHandle(DeserializeRawHandle(data.handle, message).value()));
  remote_state.version = data.version;
}

NOINLINE ScopedInterfaceEndpointHandle
DeserializeAssociatedEndpointHandle(const AssociatedEndpointHandle_Data& data,
                                    Message& message) {
  if (!data.is_valid()) {
    return {};
  }

  auto& handles = *message.mutable_associated_endpoint_handles();
  DCHECK_LT(data.value, handles.size());
  return std::move(handles[data.value]);
}

}  // namespace internal
}  // namespace mojo
