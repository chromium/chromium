// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/core/platform_shared_memory_mapping.h"

#include <utility>

#include "base/check_op.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/memory/writable_shared_memory_region.h"
#include "base/system/sys_info.h"
#include "build/build_config.h"

#if defined(OS_NACL)
// For getpagesize() on NaCl.
#include <unistd.h>
#endif

namespace mojo {
namespace core {

namespace {

size_t GetPageSize() {
#if defined(OS_NACL)
  // base::SysInfo isn't available under NaCl.
  return getpagesize();
#else
  return base::SysInfo::VMAllocationGranularity();
#endif
}

}  // namespace

PlatformSharedMemoryMapping::PlatformSharedMemoryMapping(
    base::subtle::PlatformSharedMemoryRegion* region,
    size_t offset,
    size_t length)
    : type_(region->GetMode() ==
                    base::subtle::PlatformSharedMemoryRegion::Mode::kReadOnly
                ? Type::kReadOnly
                : Type::kWritable),
      offset_(offset),
      length_(length) {
  // Mojo shared buffers can be mapped at any offset, but //base shared memory
  // regions must be mapped at a page boundary. We calculate the nearest whole
  // page offset and map from there.
  size_t offset_rounding = offset_ % GetPageSize();
  off_t real_offset = static_cast<off_t>(offset_ - offset_rounding);
  size_t real_length = length_ + offset_rounding;
  void* mapped_memory = nullptr;
  if (type_ == Type::kReadOnly) {
    auto read_only_region =
        base::ReadOnlySharedMemoryRegion::Deserialize(std::move(*region));
    auto read_only_mapping = read_only_region.MapAt(real_offset, real_length);
    mapped_memory = const_cast<void*>(read_only_mapping.memory());
    mapping_ = std::make_unique<base::ReadOnlySharedMemoryMapping>(
        std::move(read_only_mapping));
    *region = base::ReadOnlySharedMemoryRegion::TakeHandleForSerialization(
        std::move(read_only_region));
  } else if (region->GetMode() ==
             base::subtle::PlatformSharedMemoryRegion::Mode::kUnsafe) {
    auto unsafe_region =
        base::UnsafeSharedMemoryRegion::Deserialize(std::move(*region));
    auto writable_mapping = unsafe_region.MapAt(real_offset, real_length);
    mapped_memory = writable_mapping.memory();
    mapping_ = std::make_unique<base::WritableSharedMemoryMapping>(
        std::move(writable_mapping));
    *region = base::UnsafeSharedMemoryRegion::TakeHandleForSerialization(
        std::move(unsafe_region));
  } else {
    DCHECK_EQ(region->GetMode(),
              base::subtle::PlatformSharedMemoryRegion::Mode::kWritable);
    auto writable_region =
        base::WritableSharedMemoryRegion::Deserialize(std::move(*region));
    auto writable_mapping = writable_region.MapAt(real_offset, real_length);
    mapped_memory = writable_mapping.memory();
    mapping_ = std::make_unique<base::WritableSharedMemoryMapping>(
        std::move(writable_mapping));
    *region = base::WritableSharedMemoryRegion::TakeHandleForSerialization(
        std::move(writable_region));
  }

  base_ = static_cast<char*>(mapped_memory) + offset_rounding;
}

PlatformSharedMemoryMapping::~PlatformSharedMemoryMapping() = default;

bool PlatformSharedMemoryMapping::IsValid() const {
  return mapping_ && mapping_->IsValid();
}

void* PlatformSharedMemoryMapping::GetBase() const {
  return base_;
}

size_t PlatformSharedMemoryMapping::GetLength() const {
  return length_;
}

}  // namespace core
}  // namespace mojo
