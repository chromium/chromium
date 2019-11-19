// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/media_stream_buffer_manager.h"

#include <stddef.h>

#include <utility>

#include "base/logging.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/shared_impl/media_stream_buffer.h"

namespace ppapi {

MediaStreamBufferManager::Delegate::~Delegate() {}

void MediaStreamBufferManager::Delegate::OnNewBufferEnqueued() {}

MediaStreamBufferManager::MediaStreamBufferManager(Delegate* delegate)
    : delegate_(delegate), buffer_size_(0), number_of_buffers_(0) {
  DCHECK(delegate_);
}

MediaStreamBufferManager::~MediaStreamBufferManager() {}

bool MediaStreamBufferManager::SetBuffers(int32_t number_of_buffers,
                                          int32_t buffer_size,
                                          base::UnsafeSharedMemoryRegion region,
                                          bool enqueue_all_buffers) {
  DCHECK(region.IsValid());
  DCHECK_GT(number_of_buffers, 0);
  DCHECK_GT(buffer_size,
            static_cast<int32_t>(sizeof(MediaStreamBuffer::Header)));
  DCHECK_EQ(buffer_size & 0x3, 0);

  number_of_buffers_ = number_of_buffers;
  buffer_size_ = buffer_size;

  size_t size = number_of_buffers_ * buffer_size;
  region_ = std::move(region);
  mapping_ = region_.MapAt(0, size);
  if (!mapping_.IsValid())
    return false;

  buffer_queue_.clear();
  buffers_.clear();
  uint8_t* p = mapping_.GetMemoryAsSpan<uint8_t>().data();
  for (int32_t i = 0; i < number_of_buffers; ++i) {
    if (enqueue_all_buffers)
      buffer_queue_.push_back(i);
    buffers_.push_back(reinterpret_cast<MediaStreamBuffer*>(p));
    p += buffer_size_;
  }
  return true;
}

int32_t MediaStreamBufferManager::DequeueBuffer() {
  if (buffer_queue_.empty())
    return PP_ERROR_FAILED;
  int32_t buffer = buffer_queue_.front();
  buffer_queue_.pop_front();
  return buffer;
}

std::vector<int32_t> MediaStreamBufferManager::DequeueBuffers() {
  std::vector<int32_t> buffers(buffer_queue_.begin(), buffer_queue_.end());
  buffer_queue_.clear();
  return buffers;
}

void MediaStreamBufferManager::EnqueueBuffer(int32_t index) {
  CHECK_GE(index, 0) << "Invalid buffer index";
  CHECK_LT(index, number_of_buffers_) << "Invalid buffer index";
  buffer_queue_.push_back(index);
  delegate_->OnNewBufferEnqueued();
}

bool MediaStreamBufferManager::HasAvailableBuffer() {
  return !buffer_queue_.empty();
}

MediaStreamBuffer* MediaStreamBufferManager::GetBufferPointer(int32_t index) {
  if (index < 0 || index >= number_of_buffers_)
    return NULL;
  return buffers_[index];
}

}  // namespace ppapi
