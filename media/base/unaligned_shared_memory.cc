// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/unaligned_shared_memory.h"

#include <limits>

#include "base/logging.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/system/sys_info.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace media {

namespace {

bool CalculateMisalignmentAndOffset(size_t size,
                                    off_t offset,
                                    size_t* misalignment,
                                    off_t* adjusted_offset) {
  /* |   |   |   |   |   |  shm pages
   *       |                offset (may exceed max size_t)
   *       |-----------|    size
   *     |-|                misalignment
   *     |                  adjusted offset
   *     |-------------|    requested mapping
   */

  // Note: result of % computation may be off_t or size_t, depending on the
  // relative ranks of those types. In any case we assume that
  // VMAllocationGranularity() fits in both types, so the final result does too.
  DCHECK_GE(offset, 0);
  *misalignment = offset % base::SysInfo::VMAllocationGranularity();

  // Above this |max_size|, |size| + |*misalignment| overflows.
  size_t max_size = std::numeric_limits<size_t>::max() - *misalignment;
  if (size > max_size) {
    DLOG(ERROR) << "Invalid size";
    return false;
  }

  *adjusted_offset = offset - static_cast<off_t>(*misalignment);

  return true;
}

}  // namespace

UnalignedSharedMemory::UnalignedSharedMemory(
    base::subtle::PlatformSharedMemoryRegion region,
    size_t size,
    bool read_only)
    : region_(std::move(region)), read_only_(read_only), size_(size) {}

UnalignedSharedMemory::~UnalignedSharedMemory() = default;

bool UnalignedSharedMemory::MapAt(off_t offset, size_t size) {
  if (offset < 0) {
    DLOG(ERROR) << "Invalid offset";
    return false;
  }

  size_t misalignment;
  off_t adjusted_offset;

  if (!CalculateMisalignmentAndOffset(size, offset, &misalignment,
                                      &adjusted_offset)) {
    return false;
  }

  if (read_only_) {
    auto shm =
        base::ReadOnlySharedMemoryRegion::Deserialize(std::move(region_));
    read_only_mapping_ = shm.MapAt(adjusted_offset, size + misalignment);
    if (!read_only_mapping_.IsValid()) {
      DLOG(ERROR) << "Failed to map shared memory";
      return false;
    }
    // TODO(crbug.com/849207): this ugly const cast will go away when uses of
    // UnalignedSharedMemory are converted to
    // {Writable,ReadOnly}UnalignedMapping.
    mapping_ptr_ = const_cast<uint8_t*>(
        static_cast<const uint8_t*>(read_only_mapping_.memory()));
  } else {
    auto shm = base::UnsafeSharedMemoryRegion::Deserialize(std::move(region_));
    writable_mapping_ = shm.MapAt(adjusted_offset, size + misalignment);
    if (!writable_mapping_.IsValid()) {
      DLOG(ERROR) << "Failed to map shared memory";
      return false;
    }
    mapping_ptr_ = static_cast<uint8_t*>(writable_mapping_.memory());
  }

  DCHECK(mapping_ptr_);
  // There should be no way for the IsValid() checks above to succeed and yet
  // |mapping_ptr_| remain null. However, since an invalid but non-null pointer
  // could be disastrous an extra-careful check is done.
  if (mapping_ptr_)
    mapping_ptr_ += misalignment;
  return true;
}

WritableUnalignedMapping::WritableUnalignedMapping(
    const base::UnsafeSharedMemoryRegion& region,
    size_t size,
    off_t offset)
    : size_(size), misalignment_(0) {
  if (!region.IsValid()) {
    DLOG(ERROR) << "Invalid region";
    return;
  }

  if (offset < 0) {
    DLOG(ERROR) << "Invalid offset";
    return;
  }

  off_t adjusted_offset;
  if (!CalculateMisalignmentAndOffset(size_, offset, &misalignment_,
                                      &adjusted_offset)) {
    return;
  }

  mapping_ = region.MapAt(adjusted_offset, size_ + misalignment_);
  if (!mapping_.IsValid()) {
    DLOG(ERROR) << "Failed to map shared memory " << adjusted_offset << "("
                << offset << ")"
                << "@" << size << "/\\" << misalignment_ << " on "
                << region.GetSize();

    return;
  }
}

WritableUnalignedMapping::~WritableUnalignedMapping() = default;

void* WritableUnalignedMapping::memory() const {
  if (!IsValid()) {
    return nullptr;
  }
  return mapping_.GetMemoryAs<uint8_t>() + misalignment_;
}

ReadOnlyUnalignedMapping::ReadOnlyUnalignedMapping(
    const base::ReadOnlySharedMemoryRegion& region,
    size_t size,
    off_t offset)
    : size_(size), misalignment_(0) {
  if (!region.IsValid()) {
    DLOG(ERROR) << "Invalid region";
    return;
  }

  if (offset < 0) {
    DLOG(ERROR) << "Invalid offset";
    return;
  }

  off_t adjusted_offset;
  if (!CalculateMisalignmentAndOffset(size_, offset, &misalignment_,
                                      &adjusted_offset)) {
    return;
  }

  mapping_ = region.MapAt(adjusted_offset, size_ + misalignment_);
  if (!mapping_.IsValid()) {
    DLOG(ERROR) << "Failed to map shared memory " << adjusted_offset << "("
                << offset << ")"
                << "@" << size << "/\\" << misalignment_ << " on "
                << region.GetSize();

    return;
  }
}

ReadOnlyUnalignedMapping::~ReadOnlyUnalignedMapping() = default;

const void* ReadOnlyUnalignedMapping::memory() const {
  if (!IsValid()) {
    return nullptr;
  }
  return mapping_.GetMemoryAs<uint8_t>() + misalignment_;
}

}  // namespace media
