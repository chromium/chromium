// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_SHARED_MEMORY_POOL_H_
#define MEDIA_BASE_SHARED_MEMORY_POOL_H_

#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/synchronization/lock.h"
#include "media/base/media_export.h"

namespace media {

// SharedMemoryPool manages allocation and pooling of UnsafeSharedMemoryRegions.
// It is thread-safe.
// May return bigger regions than requested.
// If a requested size is increased, all stored regions are purged.
// Regions are returned to the buffer on destruction of |SharedMemoryHandle| if
// they are of a correct size.
class MEDIA_EXPORT SharedMemoryPool
    : public base::RefCountedThreadSafe<SharedMemoryPool> {
 public:
  // Used to store the allocation result.
  // This class returns memory to the pool upon destruction.
  class MEDIA_EXPORT SharedMemoryHandle {
   public:
    SharedMemoryHandle(base::UnsafeSharedMemoryRegion region,
                       base::WritableSharedMemoryMapping mapping,
                       scoped_refptr<SharedMemoryPool> pool);
    ~SharedMemoryHandle();
    // Disallow copy and assign.
    SharedMemoryHandle(const SharedMemoryHandle&) = delete;
    SharedMemoryHandle& operator=(const SharedMemoryHandle&) = delete;

    base::UnsafeSharedMemoryRegion* GetRegion();

    base::WritableSharedMemoryMapping* GetMapping();

   private:
    base::UnsafeSharedMemoryRegion region_;
    base::WritableSharedMemoryMapping mapping_;
    scoped_refptr<SharedMemoryPool> pool_;
  };

  SharedMemoryPool();
  // Disallow copy and assign.
  SharedMemoryPool(const SharedMemoryPool&) = delete;
  SharedMemoryPool& operator=(const SharedMemoryPool&) = delete;

  // Allocates a region of the given |size| or reuses a previous allocation if
  // possible.
  std::unique_ptr<SharedMemoryHandle> MaybeAllocateBuffer(size_t size);

  // Shuts down the pool, freeing all currently unused allocations and freeing
  // outstanding ones as they are returned.
  void Shutdown();

 private:
  friend class base::RefCountedThreadSafe<SharedMemoryPool>;
  ~SharedMemoryPool();

  void ReleaseBuffer(base::UnsafeSharedMemoryRegion region,
                     base::WritableSharedMemoryMapping mapping);

  base::Lock lock_;
  size_t region_size_ GUARDED_BY(lock_) = 0u;
  std::vector<base::UnsafeSharedMemoryRegion> regions_ GUARDED_BY(lock_);
  std::vector<base::WritableSharedMemoryMapping> mappings_ GUARDED_BY(lock_);
  bool is_shutdown_ GUARDED_BY(lock_) = false;
};

}  // namespace media

#endif  // MEDIA_BASE_SHARED_MEMORY_POOL_H_
