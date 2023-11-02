// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/common/activity_flags.h"

#include "base/memory/shared_memory_mapping.h"
#include "base/memory/unsafe_shared_memory_region.h"

namespace gpu {

ActivityFlagsBase::ActivityFlagsBase() = default;
ActivityFlagsBase::ActivityFlagsBase(ActivityFlagsBase&& other) = default;
ActivityFlagsBase::~ActivityFlagsBase() = default;

void ActivityFlagsBase::Initialize(base::UnsafeSharedMemoryRegion region) {
  region_ = std::move(region);
  mapping_ = region_.Map();
}

volatile base::subtle::Atomic32* ActivityFlagsBase::AsAtomic() {
  return reinterpret_cast<volatile base::subtle::Atomic32*>(mapping_.memory());
}

GpuProcessActivityFlags::GpuProcessActivityFlags() = default;
GpuProcessActivityFlags::GpuProcessActivityFlags(
    GpuProcessActivityFlags&& other) = default;

GpuProcessActivityFlags::GpuProcessActivityFlags(
    base::UnsafeSharedMemoryRegion region) {
  // In cases where we are running without a GpuProcessHost, we may not
  // have a valid handle. In this case, just return.
  if (!region.IsValid())
    return;

  Initialize(std::move(region));
}

void GpuProcessActivityFlags::SetFlag(Flag flag) {
  // In cases where we are running without a GpuProcessHost, we may not
  // initialize the GpuProcessActivityFlags. In this case, just return.
  if (!is_initialized())
    return;

  base::subtle::Atomic32 old_value = base::subtle::NoBarrier_Load(AsAtomic());
  base::subtle::Atomic32 new_value = old_value | flag;

  // These flags are only written by a single process / single thread.
  // We should never double-set them.
  DCHECK(!(old_value & flag));
  base::subtle::Release_Store(AsAtomic(), new_value);
}

void GpuProcessActivityFlags::UnsetFlag(Flag flag) {
  // In cases where we are running without a GpuProcessHost, we may not
  // initialize the GpuProcessActivityFlags. In this case, just return.
  if (!is_initialized())
    return;

  base::subtle::Atomic32 old_value = base::subtle::NoBarrier_Load(AsAtomic());
  base::subtle::Atomic32 new_value = old_value ^ flag;

  // These flags are only written by a single process / single thread.
  // We should never double-unset them.
  DCHECK(!!(old_value & flag));
  base::subtle::Release_Store(AsAtomic(), new_value);
}

GpuProcessHostActivityFlags::GpuProcessHostActivityFlags() {
  Initialize(base::UnsafeSharedMemoryRegion::Create(sizeof(Flag)));
}

bool GpuProcessHostActivityFlags::IsFlagSet(Flag flag) {
  DCHECK(is_initialized());
  return !!(base::subtle::Acquire_Load(AsAtomic()) & flag);
}

}  // namespace gpu
