// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_UNALIGNED_SHARED_MEMORY_H_
#define MEDIA_BASE_UNALIGNED_SHARED_MEMORY_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/memory/platform_shared_memory_region.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "media/base/media_export.h"

namespace media {

// Wrapper over base::PlatformSharedMemoryRegion that can be mapped at unaligned
// offsets.
// DEPRECATED! See https://crbug.com/795291.
class MEDIA_EXPORT UnalignedSharedMemory {
 public:
  // Creates an |UnalignedSharedMemory| instance from a
  // |PlatformSharedMemoryRegion|. |size| sets the maximum size that may be
  // mapped. This instance will own the handle.
  UnalignedSharedMemory(base::subtle::PlatformSharedMemoryRegion region,
                        size_t size,
                        bool read_only);

  ~UnalignedSharedMemory();

  // Map the shared memory region. Note that the passed |size| parameter should
  // be less than or equal to |size()|.
  bool MapAt(off_t offset, size_t size);
  size_t size() const { return size_; }
  void* memory() const { return mapping_ptr_; }

 private:
  // Only one of the mappings is active, depending on the value of |read_only_|.
  // These variables are held to keep the shared memory mapping valid for the
  // lifetime of this instance.
  base::subtle::PlatformSharedMemoryRegion region_;
  base::WritableSharedMemoryMapping writable_mapping_;
  base::ReadOnlySharedMemoryMapping read_only_mapping_;

  // If the mapping should be made read-only.
  bool read_only_;

  // The size of the region associated with |region_|.
  size_t size_;

  // Pointer to the unaligned data in the shared memory mapping.
  uint8_t* mapping_ptr_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(UnalignedSharedMemory);
};

// Wrapper over base::WritableSharedMemoryMapping that is mapped at unaligned
// offsets.
class MEDIA_EXPORT WritableUnalignedMapping {
 public:
  // Creates an |WritableUnalignedMapping| instance from a
  // |UnsafeSharedMemoryRegion|. |size| sets the maximum size that may be mapped
  // within |region| and |offset| is the offset that will be mapped. |region| is
  // not retained and is used only in the constructor.
  WritableUnalignedMapping(const base::UnsafeSharedMemoryRegion& region,
                           size_t size,
                           off_t offset);

  ~WritableUnalignedMapping();

  size_t size() const { return size_; }
  void* memory() const;

  // True if the mapping backing the memory is valid.
  bool IsValid() const { return mapping_.IsValid(); }

 private:
  base::WritableSharedMemoryMapping mapping_;

  // The size of the region associated with |mapping_|.
  size_t size_;

  // Difference between actual offset within |mapping_| where data has been
  // mapped and requested offset; strictly less than
  // base::SysInfo::VMAllocationGranularity().
  size_t misalignment_;

  DISALLOW_COPY_AND_ASSIGN(WritableUnalignedMapping);
};

// Wrapper over base::ReadOnlySharedMemoryMapping that is mapped at unaligned
// offsets.
class MEDIA_EXPORT ReadOnlyUnalignedMapping {
 public:
  // Creates an |WritableUnalignedMapping| instance from a
  // |ReadOnlySharedMemoryRegion|. |size| sets the maximum size that may be
  // mapped within |region| and |offset| is the offset that will be mapped.
  // |region| is not retained and is used only in the constructor.
  ReadOnlyUnalignedMapping(const base::ReadOnlySharedMemoryRegion& region,
                           size_t size,
                           off_t offset);

  ~ReadOnlyUnalignedMapping();

  size_t size() const { return size_; }
  const void* memory() const;

  // True if the mapping backing the memory is valid.
  bool IsValid() const { return mapping_.IsValid(); }

 private:
  base::ReadOnlySharedMemoryMapping mapping_;

  // The size of the region associated with |mapping_|.
  size_t size_;

  // Difference between actual offset within |mapping_| where data has been
  // mapped and requested offset; strictly less than
  // base::SysInfo::VMAllocationGranularity().
  size_t misalignment_;

  DISALLOW_COPY_AND_ASSIGN(ReadOnlyUnalignedMapping);
};

}  // namespace media

#endif  // MEDIA_BASE_UNALIGNED_SHARED_MEMORY_H_
