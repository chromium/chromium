// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/vpn_provider_util.h"

namespace ppapi {

VpnProviderSharedBuffer::VpnProviderSharedBuffer(
    uint32_t capacity,
    uint32_t packet_size,
    base::UnsafeSharedMemoryRegion shm,
    base::WritableSharedMemoryMapping mapping)
    : capacity_(capacity),
      max_packet_size_(packet_size),
      shm_(std::move(shm)),
      shm_mapping_(std::move(mapping)),
      available_(capacity, true) {
  DCHECK(shm_.IsValid() && shm_mapping_.IsValid());
}

VpnProviderSharedBuffer::~VpnProviderSharedBuffer() {}

bool VpnProviderSharedBuffer::GetAvailable(uint32_t* id) {
  for (uint32_t i = 0; i < capacity_; i++) {
    if (available_[i]) {
      if (id) {
        *id = i;
      }
      return true;
    }
  }
  return false;
}

void VpnProviderSharedBuffer::SetAvailable(uint32_t id, bool value) {
  if (id >= capacity_) {
    NOTREACHED();
    return;
  }
  available_[id] = value;
}

void* VpnProviderSharedBuffer::GetBuffer(uint32_t id) {
  if (id >= capacity_) {
    NOTREACHED();
    return nullptr;
  }
  return shm_mapping_.GetMemoryAsSpan<char>()
      .subspan(max_packet_size_ * id)
      .data();
}

base::UnsafeSharedMemoryRegion VpnProviderSharedBuffer::DuplicateRegion()
    const {
  return shm_.Duplicate();
}

}  // namespace ppapi
