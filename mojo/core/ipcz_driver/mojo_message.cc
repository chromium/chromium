// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/core/ipcz_driver/mojo_message.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <utility>

#include "base/containers/span.h"
#include "base/numerics/safe_conversions.h"
#include "mojo/core/ipcz_api.h"
#include "mojo/core/ipcz_driver/data_pipe.h"
#include "third_party/ipcz/include/ipcz/ipcz.h"

namespace mojo::core::ipcz_driver {

MojoMessage::MojoMessage() = default;

MojoMessage::MojoMessage(std::vector<uint8_t> data,
                         std::vector<IpczHandle> handles) {
  SetContents(std::move(data), std::move(handles), IPCZ_INVALID_HANDLE);
}

MojoMessage::~MojoMessage() {
  for (IpczHandle handle : handles_) {
    if (handle != IPCZ_INVALID_HANDLE) {
      GetIpczAPI().Close(handle, IPCZ_NO_FLAGS, nullptr);
    }
  }

  if (validator_ != IPCZ_INVALID_HANDLE) {
    GetIpczAPI().Close(validator_, IPCZ_NO_FLAGS, nullptr);
  }

  if (destructor_) {
    destructor_(context_);
  }
}

bool MojoMessage::SetContents(std::vector<uint8_t> data,
                              std::vector<IpczHandle> handles,
                              IpczHandle validator) {
  const size_t size = data.size();
  if (size >= kMinBufferSize) {
    data_storage_ = std::move(data);
  } else {
    data_storage_.resize(kMinBufferSize);
    std::copy(data.begin(), data.end(), data_storage_.begin());
  }

  validator_ = validator;
  data_ = base::make_span(data_storage_).first(size);
  size_committed_ = true;
  if (handles.empty()) {
    return true;
  }

  // If there are any serialized DataPipe objects, accumulate them so we can
  // pluck their portals off the end of `handles`. Their portals were
  // attached the end of `handles` when the sender finalized the message in
  // MojoWriteMessageIpcz().
  std::vector<DataPipe*> data_pipes;
  for (IpczHandle handle : handles) {
    if (auto* data_pipe = DataPipe::FromBox(handle)) {
      data_pipes.push_back(data_pipe);
    }
  }

  if (handles.size() / 2 < data_pipes.size()) {
    // There must be at least enough handles for each DataPipe box AND its
    // portal.
    return false;
  }

  // The last N handles are portals for the pipes in `data_pipes`, in order.
  // Remove them from the message's handles and give them to their data pipes.
  const size_t first_data_pipe_portal = handles.size() - data_pipes.size();
  for (size_t i = 0; i < data_pipes.size(); ++i) {
    const IpczHandle handle = handles[first_data_pipe_portal + i];
    if (ObjectBase::FromBox(handle)) {
      // The handle in this position needs to be a portal. If it's a driver
      // object, something is wrong.
      return false;
    }

    data_pipes[i]->AdoptPortal(handle);
  }
  handles.resize(first_data_pipe_portal);
  handles_ = std::move(handles);
  return true;
}

MojoResult MojoMessage::AppendData(uint32_t additional_num_bytes,
                                   const MojoHandle* handles,
                                   uint32_t num_handles,
                                   void** buffer,
                                   uint32_t* buffer_size,
                                   bool commit_size) {
  if (context_ || size_committed_) {
    return MOJO_RESULT_FAILED_PRECONDITION;
  }

  const size_t new_data_size = data_.size() + additional_num_bytes;
  const size_t required_storage_size = std::max(new_data_size, kMinBufferSize);
  if (required_storage_size > data_storage_.size()) {
    data_storage_.resize(std::max(data_.size() * 2, required_storage_size));
  }
  data_ = base::make_span(data_storage_).first(new_data_size);

  handles_.reserve(handles_.size() + num_handles);
  for (MojoHandle handle : base::make_span(handles, num_handles)) {
    handles_.push_back(handle);
  }
  if (buffer) {
    *buffer = data_storage_.data();
  }
  if (buffer_size) {
    *buffer_size = base::checked_cast<uint32_t>(data_storage_.size());
  }
  size_committed_ = commit_size;
  return MOJO_RESULT_OK;
}

IpczResult MojoMessage::GetData(void** buffer,
                                uint32_t* num_bytes,
                                MojoHandle* handles,
                                uint32_t* num_handles,
                                bool consume_handles) {
  if (context_ || !size_committed_) {
    return MOJO_RESULT_FAILED_PRECONDITION;
  }
  if (consume_handles && handles_consumed_) {
    return MOJO_RESULT_NOT_FOUND;
  }

  if (buffer) {
    *buffer = data_storage_.data();
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

  std::copy(handles_.begin(), handles_.end(), handles);
  handles_.clear();
  handles_consumed_ = true;
  return MOJO_RESULT_OK;
}

void MojoMessage::AttachDataPipePortals() {
  const size_t base_num_handles = handles_.size();
  for (size_t i = 0; i < base_num_handles; ++i) {
    if (auto* data_pipe = ipcz_driver::DataPipe::FromBox(handles_[i])) {
      handles_.push_back(data_pipe->TakePortal());
    }
  }
}

MojoResult MojoMessage::SetContext(uintptr_t context,
                                   MojoMessageContextSerializer serializer,
                                   MojoMessageContextDestructor destructor) {
  if (context_ && context) {
    return MOJO_RESULT_ALREADY_EXISTS;
  }
  if (!data_storage_.empty() || !handles_.empty()) {
    return MOJO_RESULT_FAILED_PRECONDITION;
  }

  context_ = context;
  serializer_ = serializer;
  destructor_ = destructor;
  return MOJO_RESULT_OK;
}

MojoResult MojoMessage::Serialize() {
  if (!data_storage_.empty() || !handles_.empty()) {
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
