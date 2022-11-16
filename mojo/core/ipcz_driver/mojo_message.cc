// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/core/ipcz_driver/mojo_message.h"

#include <cstddef>
#include <cstdint>
#include <utility>

#include "base/containers/span.h"
#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
#include "mojo/core/ipcz_api.h"
#include "mojo/core/ipcz_driver/data_pipe.h"
#include "mojo/core/scoped_ipcz_handle.h"
#include "third_party/ipcz/include/ipcz/ipcz.h"

namespace mojo::core::ipcz_driver {

MojoMessage::MojoMessage() = default;

MojoMessage::MojoMessage(std::vector<uint8_t> data,
                         std::vector<IpczHandle> handles)
    : handles_(std::move(handles)) {
  data_storage_.reset(
      static_cast<uint8_t*>(base::AllocNonScannable(data.size())));
  data_storage_size_ = data.size();
  base::ranges::copy(data, data_storage_.get());
}

MojoMessage::~MojoMessage() {
  for (IpczHandle handle : handles_) {
    if (handle != IPCZ_INVALID_HANDLE) {
      GetIpczAPI().Close(handle, IPCZ_NO_FLAGS, nullptr);
    }
  }

  if (destructor_) {
    destructor_(context_);
  }
}

bool MojoMessage::SetParcel(ScopedIpczHandle parcel) {
  DCHECK(!data_storage_);
  DCHECK(!parcel_.is_valid());

  parcel_ = std::move(parcel);

  const void* data;
  size_t num_bytes;
  size_t num_handles;
  IpczResult result = GetIpczAPI().BeginGet(
      parcel_.get(), IPCZ_NO_FLAGS, nullptr, &data, &num_bytes, &num_handles);
  if (result != IPCZ_RESULT_OK) {
    return false;
  }

  // Grab only the handles.
  handles_.resize(num_handles);
  result = GetIpczAPI().EndGet(parcel_.get(), 0, num_handles, IPCZ_NO_FLAGS,
                               nullptr, handles_.data());
  if (result != IPCZ_RESULT_OK) {
    return false;
  }

  // Now start a new two-phase get, which we'll leave active indefinitely for
  // `data_` to reference.
  result = GetIpczAPI().BeginGet(parcel_.get(), IPCZ_NO_FLAGS, nullptr, &data,
                                 &num_bytes, &num_handles);
  if (result != IPCZ_RESULT_OK) {
    return false;
  }

  DCHECK_EQ(0u, num_handles);
  data_ = base::make_span(static_cast<uint8_t*>(const_cast<void*>(data)),
                          num_bytes);

  // If there are any serialized DataPipe objects, accumulate them so we can
  // pluck their portals off the end of `handles`. Their portals were
  // attached the end of `handles` when the sender finalized the message in
  // MojoWriteMessageIpcz().
  std::vector<DataPipe*> data_pipes;
  for (IpczHandle handle : handles_) {
    if (auto* data_pipe = DataPipe::FromBox(handle)) {
      data_pipes.push_back(data_pipe);
    }
  }

  if (handles_.size() / 2 < data_pipes.size()) {
    // There must be at least enough handles for each DataPipe box AND its
    // portal.
    return false;
  }

  // The last N handles are portals for the pipes in `data_pipes`, in order.
  // Remove them from the message's handles and give them to their data pipes.
  const size_t first_data_pipe_portal = handles_.size() - data_pipes.size();
  for (size_t i = 0; i < data_pipes.size(); ++i) {
    const IpczHandle handle = handles_[first_data_pipe_portal + i];
    if (ObjectBase::FromBox(handle)) {
      // The handle in this position needs to be a portal. If it's a driver
      // object, something is wrong.
      return false;
    }

    data_pipes[i]->AdoptPortal(ScopedIpczHandle(handle));
  }
  handles_.resize(first_data_pipe_portal);
  return true;
}

MojoResult MojoMessage::AppendData(uint32_t additional_num_bytes,
                                   const MojoHandle* handles,
                                   uint32_t num_handles,
                                   void** buffer,
                                   uint32_t* buffer_size,
                                   bool commit_size) {
  DCHECK(!parcel_.is_valid());
  if (context_ || size_committed_) {
    return MOJO_RESULT_FAILED_PRECONDITION;
  }

  const size_t data_size = data_.size();
  const size_t new_data_size = data_size + additional_num_bytes;
  const size_t required_storage_size = std::max(new_data_size, kMinBufferSize);
  if (required_storage_size > data_storage_size_) {
    const size_t copy_size = std::min(new_data_size, data_storage_size_);
    data_storage_size_ = std::max(data_size * 2, required_storage_size);
    DataPtr new_storage(
        static_cast<uint8_t*>(base::AllocNonScannable(data_storage_size_)));
    base::ranges::copy(base::make_span(data_storage_.get(), copy_size),
                       new_storage.get());
    data_storage_ = std::move(new_storage);
  }
  data_ = base::make_span(data_storage_.get(), new_data_size);

  handles_.reserve(handles_.size() + num_handles);
  for (MojoHandle handle : base::make_span(handles, num_handles)) {
    handles_.push_back(handle);
  }
  if (buffer) {
    *buffer = data_storage_.get();
  }
  if (buffer_size) {
    *buffer_size = base::checked_cast<uint32_t>(data_storage_size_);
  }
  size_committed_ = commit_size;
  return MOJO_RESULT_OK;
}

IpczResult MojoMessage::GetData(void** buffer,
                                uint32_t* num_bytes,
                                MojoHandle* handles,
                                uint32_t* num_handles,
                                bool consume_handles) {
  if (context_ || (!parcel_.is_valid() && !size_committed_)) {
    return MOJO_RESULT_FAILED_PRECONDITION;
  }
  if (consume_handles && handles_consumed_) {
    return MOJO_RESULT_NOT_FOUND;
  }

  if (buffer) {
    *buffer = data_.data();
  }
  if (num_bytes) {
    *num_bytes = base::checked_cast<uint32_t>(data_.size());
  }

  if (!consume_handles || handles_.empty()) {
    return MOJO_RESULT_OK;
  }

  uint32_t capacity = num_handles ? *num_handles : 0;
  uint32_t required_capacity = base::checked_cast<uint32_t>(handles_.size());
  if (num_handles) {
    *num_handles = required_capacity;
  }
  if (!handles || capacity < required_capacity) {
    return MOJO_RESULT_RESOURCE_EXHAUSTED;
  }

  base::ranges::copy(handles_, handles);
  handles_.clear();
  handles_consumed_ = true;
  return MOJO_RESULT_OK;
}

void MojoMessage::AttachDataPipePortals() {
  const size_t base_num_handles = handles_.size();
  for (size_t i = 0; i < base_num_handles; ++i) {
    if (auto* data_pipe = ipcz_driver::DataPipe::FromBox(handles_[i])) {
      handles_.push_back(data_pipe->TakePortal().release());
    }
  }
}

MojoResult MojoMessage::SetContext(uintptr_t context,
                                   MojoMessageContextSerializer serializer,
                                   MojoMessageContextDestructor destructor) {
  if (context_ && context) {
    return MOJO_RESULT_ALREADY_EXISTS;
  }
  if (parcel_.is_valid() || data_storage_ || !handles_.empty()) {
    return MOJO_RESULT_FAILED_PRECONDITION;
  }

  context_ = context;
  serializer_ = serializer;
  destructor_ = destructor;
  return MOJO_RESULT_OK;
}

MojoResult MojoMessage::Serialize() {
  if (parcel_.is_valid() || data_storage_ || !handles_.empty()) {
    return MOJO_RESULT_FAILED_PRECONDITION;
  }
  if (!serializer_) {
    return MOJO_RESULT_NOT_FOUND;
  }

  const uintptr_t context = std::exchange(context_, 0);
  const MojoMessageContextSerializer serializer =
      std::exchange(serializer_, nullptr);
  const MojoMessageContextDestructor destructor =
      std::exchange(destructor_, nullptr);
  serializer(handle(), context);
  if (destructor) {
    destructor(context);
  }
  return MOJO_RESULT_OK;
}

}  // namespace mojo::core::ipcz_driver
