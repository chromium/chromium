// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MOJO_CDM_ALLOCATOR_H_
#define MEDIA_MOJO_SERVICES_MOJO_CDM_ALLOCATOR_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>

#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "media/cdm/cdm_allocator.h"
#include "media/mojo/services/media_mojo_export.h"
#include "mojo/public/cpp/system/buffer.h"

namespace media {

// This is a CdmAllocator that creates buffers using mojo shared memory.
class MEDIA_MOJO_EXPORT MojoCdmAllocator final : public CdmAllocator {
 public:
  MojoCdmAllocator();

  MojoCdmAllocator(const MojoCdmAllocator&) = delete;
  MojoCdmAllocator& operator=(const MojoCdmAllocator&) = delete;

  ~MojoCdmAllocator() final;

  // CdmAllocator implementation.
  cdm::Buffer* CreateCdmBuffer(size_t capacity) final;
  std::unique_ptr<VideoFrameImpl> CreateCdmVideoFrame() final;

 private:
  friend class MojoCdmAllocatorTest;

  // Map of available buffers. Done as a mapping of capacity to shmem regions to
  // make it efficient to find an available buffer of a particular size.
  // Regions in the map are unmapped.
  using AvailableRegionMap =
      std::multimap<size_t, std::unique_ptr<base::MappedReadOnlyRegion>>;

  // Allocates a shmem region of at least |capacity| bytes.
  std::unique_ptr<base::MappedReadOnlyRegion> AllocateNewRegion(
      size_t capacity);

  // Returns |region| to the map of available buffers, ready to be used the
  // next time CreateCdmBuffer() is called.
  void AddRegionToAvailableMap(
      std::unique_ptr<base::MappedReadOnlyRegion> region);

  // Returns the base::MappedReadOnlyRegion for a cdm::Buffer allocated by this
  // class.
  const base::MappedReadOnlyRegion& GetRegionForTesting(
      cdm::Buffer* buffer) const;

  // Returns the number of buffers in |available_regions_|.
  size_t GetAvailableRegionCountForTesting();

  // Map of available, already allocated buffers.
  AvailableRegionMap available_regions_;

  // Confirms single-threaded access.
  base::ThreadChecker thread_checker_;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<MojoCdmAllocator> weak_ptr_factory_{this};
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MOJO_CDM_ALLOCATOR_H_
