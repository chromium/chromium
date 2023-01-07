// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/lib/handle_serialization.h"

#include "base/numerics/safe_conversions.h"
#include "mojo/public/cpp/bindings/lib/bindings_internal.h"
#include "mojo/public/cpp/bindings/lib/pending_receiver_state.h"

namespace mojo {
namespace internal {

void SerializeHandle(ScopedHandle handle, Message& message, Handle_Data& data) {
  if (!handle.is_valid()) {
    data.value = kEncodedInvalidHandleValue;
    return;
  }

  auto& handles = *message.mutable_handles();
  data.value = base::checked_cast<uint32_t>(handles.size());
  handles.push_back(std::move(handle));
}

void SerializeInterfaceInfo(ScopedMessagePipeHandle handle,
                            uint32_t version,
                            Message& message,
                            Interface_Data& data) {
  SerializeHandle(ScopedHandle::From(std::move(handle)), message, data.handle);
  data.version = version;
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

void SerializeAssociatedInterfaceInfo(ScopedInterfaceEndpointHandle handle,
                                      uint32_t version,
                                      Message& message,
                                      AssociatedInterface_Data& data) {
  SerializeAssociatedEndpoint(std::move(handle), message, data.handle);
  data.version = version;
}

ScopedHandle DeserializeHandle(const Handle_Data& data, Message& message) {
  if (!data.is_valid())
    return {};

  auto& handles = *message.mutable_handles();
  DCHECK_LT(data.value, handles.size());
  return std::move(handles[data.value]);
}

void DeserializeHandleAsReceiver(const Handle_Data& data,
                                 Message& message,
                                 PendingReceiverState& receiver_state) {
  receiver_state.pipe =
      ScopedMessagePipeHandle::From(DeserializeHandle(data, message));
  if (message.receiver_connection_group())
    receiver_state.connection_group = *message.receiver_connection_group();
}

ScopedInterfaceEndpointHandle DeserializeAssociatedEndpointHandle(
    const AssociatedEndpointHandle_Data& data,
    Message& message) {
  if (!data.is_valid())
    return {};

  auto& handles = *message.mutable_associated_endpoint_handles();
  DCHECK_LT(data.value, handles.size());
  return std::move(handles[data.value]);
}

}  // namespace internal
}  // namespace mojo
