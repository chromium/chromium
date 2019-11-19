// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/lib/serialization_context.h"

#include <limits>

#include "base/logging.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/system/core.h"

namespace mojo {
namespace internal {

SerializationContext::SerializationContext() = default;

SerializationContext::~SerializationContext() = default;

void SerializationContext::AddHandle(mojo::ScopedHandle handle,
                                     Handle_Data* out_data) {
  if (!handle.is_valid()) {
    out_data->value = kEncodedInvalidHandleValue;
  } else {
    DCHECK_LT(handles_.size(), std::numeric_limits<uint32_t>::max());
    out_data->value = static_cast<uint32_t>(handles_.size());
    handles_.emplace_back(std::move(handle));
  }
}

void SerializationContext::AddInterfaceInfo(
    mojo::ScopedMessagePipeHandle handle,
    uint32_t version,
    Interface_Data* out_data) {
  AddHandle(ScopedHandle::From(std::move(handle)), &out_data->handle);
  out_data->version = version;
}

void SerializationContext::AddAssociatedEndpoint(
    ScopedInterfaceEndpointHandle handle,
    AssociatedEndpointHandle_Data* out_data) {
  if (!handle.is_valid()) {
    out_data->value = kEncodedInvalidHandleValue;
  } else {
    DCHECK_LT(associated_endpoint_handles_.size(),
              std::numeric_limits<uint32_t>::max());
    out_data->value =
        static_cast<uint32_t>(associated_endpoint_handles_.size());
    associated_endpoint_handles_.emplace_back(std::move(handle));
  }
}

void SerializationContext::AddAssociatedInterfaceInfo(
    ScopedInterfaceEndpointHandle handle,
    uint32_t version,
    AssociatedInterface_Data* out_data) {
  AddAssociatedEndpoint(std::move(handle), &out_data->handle);
  out_data->version = version;
}

void SerializationContext::TakeHandlesFromMessage(Message* message) {
  receiver_connection_group_ = message->receiver_connection_group();
  handles_.swap(*message->mutable_handles());
  associated_endpoint_handles_.swap(
      *message->mutable_associated_endpoint_handles());
}

mojo::ScopedHandle SerializationContext::TakeHandle(
    const Handle_Data& encoded_handle) {
  if (!encoded_handle.is_valid())
    return mojo::ScopedHandle();
  DCHECK_LT(encoded_handle.value, handles_.size());
  return std::move(handles_[encoded_handle.value]);
}

void SerializationContext::TakeHandleAsReceiver(
    const Handle_Data& encoded_handle,
    PendingReceiverState* receiver_state) {
  receiver_state->pipe = TakeHandleAs<MessagePipeHandle>(encoded_handle);
  if (receiver_connection_group_)
    receiver_state->connection_group = *receiver_connection_group_;
}

mojo::ScopedInterfaceEndpointHandle
SerializationContext::TakeAssociatedEndpointHandle(
    const AssociatedEndpointHandle_Data& encoded_handle) {
  if (!encoded_handle.is_valid())
    return mojo::ScopedInterfaceEndpointHandle();
  DCHECK_LT(encoded_handle.value, associated_endpoint_handles_.size());
  return std::move(associated_endpoint_handles_[encoded_handle.value]);
}

}  // namespace internal
}  // namespace mojo
