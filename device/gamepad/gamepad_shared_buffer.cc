// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/gamepad_shared_buffer.h"

namespace device {

GamepadSharedBuffer::GamepadSharedBuffer() {
  base::MappedReadOnlyRegion mapped_region =
      base::ReadOnlySharedMemoryRegion::Create(sizeof(GamepadHardwareBuffer));
  CHECK(mapped_region.IsValid());
  shared_memory_region_ = std::move(mapped_region.region);
  shared_memory_mapping_ = std::move(mapped_region.mapping);

  void* mem = shared_memory_mapping_.memory();
  DCHECK(mem);
  hardware_buffer_ = new (mem) GamepadHardwareBuffer();
  memset(&(hardware_buffer_->data), 0, sizeof(Gamepads));
}

GamepadSharedBuffer::~GamepadSharedBuffer() = default;

base::ReadOnlySharedMemoryRegion
GamepadSharedBuffer::DuplicateSharedMemoryRegion() {
  return shared_memory_region_.Duplicate();
}

Gamepads* GamepadSharedBuffer::buffer() {
  return &(hardware_buffer()->data);
}

GamepadHardwareBuffer* GamepadSharedBuffer::hardware_buffer() {
  return hardware_buffer_;
}

void GamepadSharedBuffer::WriteBegin() {
  hardware_buffer_->seqlock.WriteBegin();
}

void GamepadSharedBuffer::WriteEnd() {
  hardware_buffer_->seqlock.WriteEnd();
}

}  // namespace device
