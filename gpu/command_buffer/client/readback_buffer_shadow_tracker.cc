// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "gpu/command_buffer/client/readback_buffer_shadow_tracker.h"

#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "gpu/command_buffer/client/gles2_cmd_helper.h"
#include "gpu/command_buffer/client/mapped_memory.h"

namespace gpu {
namespace gles2 {

// ReadbackBufferShadowTracker::Buffer

ReadbackBufferShadowTracker::Buffer::Buffer(GLuint buffer_id,
                                            MappedMemoryManager* mapped_memory,
                                            GLES2CmdHelper* helper)
    : buffer_id_(buffer_id), mapped_memory_(mapped_memory), helper_(helper) {}

ReadbackBufferShadowTracker::Buffer::~Buffer() {
  Free();
}

uint32_t ReadbackBufferShadowTracker::Buffer::Alloc(int32_t* shm_id,
                                                    uint32_t* shm_offset,
                                                    bool* already_allocated) {
  *already_allocated = readback_shm_address_ != nullptr;
  if (!readback_shm_address_) {
    readback_shm_address_ =
        mapped_memory_->Alloc(size_, &shm_id_, &shm_offset_);
  }
  *shm_id = shm_id_;
  *shm_offset = shm_offset_;
  return size_;
}

void ReadbackBufferShadowTracker::Buffer::Free() {
  if (readback_shm_address_) {
    mapped_memory_->FreePendingToken(readback_shm_address_,
                                     helper_->InsertToken());
  }
  readback_shm_address_ = nullptr;
}

void* ReadbackBufferShadowTracker::Buffer::MapReadbackShm(uint32_t offset,
                                                          uint32_t map_size) {
  DCHECK(!is_mapped_);
  if (serial_of_readback_data_ != serial_of_last_write_) {
    return nullptr;
  }
  if (!readback_shm_address_) {
    return nullptr;
  }
  if (map_size > size_) {
    return nullptr;
  }
  DCHECK_GE(size_, map_size);
  if (offset > size_ - map_size) {
    return nullptr;
  }
  is_mapped_ = true;
  return &static_cast<uint8_t*>(readback_shm_address_)[offset];
}

bool ReadbackBufferShadowTracker::Buffer::UnmapReadbackShm() {
  Free();

  bool was_mapped = is_mapped_;
  is_mapped_ = false;
  return was_mapped;
}

void ReadbackBufferShadowTracker::Buffer::UpdateSerialTo(uint64_t serial) {
  DCHECK_LT(serial_of_readback_data_, serial);
  serial_of_readback_data_ = serial;
}

// ReadbackBufferShadowTracker

ReadbackBufferShadowTracker::ReadbackBufferShadowTracker(
    MappedMemoryManager* mapped_memory,
    GLES2CmdHelper* helper)
    : mapped_memory_(mapped_memory), helper_(helper) {}

ReadbackBufferShadowTracker::~ReadbackBufferShadowTracker() {}

ReadbackBufferShadowTracker::Buffer*
ReadbackBufferShadowTracker::GetOrCreateBuffer(GLuint id, GLuint size) {
  Buffer* buffer = GetBuffer(id);
  if (buffer) {
    buffer->Free();
  } else {
    buffer = new Buffer(id, mapped_memory_, helper_);
    buffers_.emplace(id, base::WrapUnique(buffer));
  }
  buffer->size_ = size;
  OnBufferWrite(id);

  return buffer;
}

void ReadbackBufferShadowTracker::OnBufferWrite(GLuint id) {
  Buffer* buffer = GetBuffer(id);
  if (!buffer) {
    // Buffer is not tracked by the ReadbackBufferShadowTracker.
    return;
  }

  DCHECK(buffer->serial_of_last_write_ <= buffer_shadow_serial_);
  buffer->serial_of_last_write_ = buffer_shadow_serial_;

  for (const auto& b : buffers_written_but_not_fenced_) {
    if (b.get() == buffer) {
      return;
    }
  }
  buffers_written_but_not_fenced_.push_back(buffer->AsWeakPtr());
}

ReadbackBufferShadowTracker::BufferList
ReadbackBufferShadowTracker::TakeUnfencedBufferList() {
  BufferList buffers;
  std::swap(buffers, buffers_written_but_not_fenced_);
  return buffers;
}

}  // namespace gles2
}  // namespace gpu
