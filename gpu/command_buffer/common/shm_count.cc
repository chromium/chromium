// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/common/shm_count.h"

#include "base/memory/shared_memory_mapping.h"
#include "base/memory/unsafe_shared_memory_region.h"

namespace gpu {

ShmCountBase::ShmCountBase() = default;
ShmCountBase::ShmCountBase(ShmCountBase&& other) = default;
ShmCountBase::~ShmCountBase() = default;

void ShmCountBase::Initialize(base::UnsafeSharedMemoryRegion region) {
  region_ = std::move(region);
  mapping_ = region_.Map();
}

bool ShmCountBase::IsInitialized() const {
  return region().IsValid();
}

volatile ShmCountBase::AtomicType* ShmCountBase::AsAtomic() {
  return reinterpret_cast<volatile AtomicType*>(mapping_.memory());
}

GpuProcessShmCount::GpuProcessShmCount() = default;
GpuProcessShmCount::GpuProcessShmCount(GpuProcessShmCount&& other) = default;

GpuProcessShmCount::GpuProcessShmCount(base::UnsafeSharedMemoryRegion region) {
  // In cases where we are running without a GpuProcessHost, we may not
  // have a valid handle. In this case, just return.
  if (!region.IsValid()) {
    return;
  }

  Initialize(std::move(region));
}

void GpuProcessShmCount::Increment() {
  // In cases where we are running without a GpuProcessHost, we may not
  // initialize the GpuProcessShmCount. In this case, just return.
  if (!IsInitialized()) {
    return;
  }

  AsAtomic()->fetch_add(1, std::memory_order_release);
}

void GpuProcessShmCount::Decrement() {
  // In cases where we are running without a GpuProcessHost, we may not
  // initialize the GpuProcessShmCount. In this case, just return.
  if (!IsInitialized()) {
    return;
  }

  CountType old_value = AsAtomic()->fetch_add(-1, std::memory_order_release);
  CHECK_GE(old_value, 1);
}

GpuProcessHostShmCount::GpuProcessHostShmCount() {
  Initialize(base::UnsafeSharedMemoryRegion::Create(sizeof(AtomicType)));
}

GpuProcessShmCount::CountType GpuProcessHostShmCount::GetCount() {
  DCHECK(IsInitialized());
  return AsAtomic()->load(std::memory_order_acquire);
}

}  // namespace gpu
