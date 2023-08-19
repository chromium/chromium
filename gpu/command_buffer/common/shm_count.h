// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_COMMON_SHM_COUNT_H_
#define GPU_COMMAND_BUFFER_COMMON_SHM_COUNT_H_

#include <atomic>

#include "base/memory/raw_ptr.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "gpu/gpu_export.h"

namespace gpu {

// Base class for GpuProcessShmCount and GpuProcessHostShmCount,
// can not be used directly.
class GPU_EXPORT ShmCountBase {
 public:
  using CountType = int32_t;

 protected:
  using AtomicType = std::atomic<CountType>;

  ShmCountBase();
  ShmCountBase(ShmCountBase&& other);
  ~ShmCountBase();

  void Initialize(base::UnsafeSharedMemoryRegion region);
  bool IsInitialized() const;
  const base::UnsafeSharedMemoryRegion& region() const { return region_; }

  volatile AtomicType* AsAtomic();

 private:
  base::UnsafeSharedMemoryRegion region_;
  base::WritableSharedMemoryMapping mapping_;
};

// Provides write-only access to the count for the gpu process.
// It is safe to use ScopedIncrement from multiple threads on the same instance
// of GpuProcessShmCount.
class GPU_EXPORT GpuProcessShmCount : public ShmCountBase {
 public:
  class ScopedIncrement {
   public:
    explicit ScopedIncrement(GpuProcessShmCount* shm_count)
        : shm_count_(shm_count) {
      shm_count_->Increment();
    }
    ~ScopedIncrement() { shm_count_->Decrement(); }

   private:
    raw_ptr<GpuProcessShmCount> shm_count_;
  };

  GpuProcessShmCount();
  GpuProcessShmCount(GpuProcessShmCount&& other);
  explicit GpuProcessShmCount(base::UnsafeSharedMemoryRegion region);

 private:
  void Increment();
  void Decrement();
};

// Provides read-only access to the count for the browser process.
// GpuProcessHostShmCount will initialize a new mojo shared buffer. The
// handle to this buffer should be passed to the GPU process via CloneHandle.
// The GPU process will then increment the count, which can be read via this
// class.
class GPU_EXPORT GpuProcessHostShmCount : public ShmCountBase {
 public:
  GpuProcessHostShmCount();

  CountType GetCount();
  base::UnsafeSharedMemoryRegion CloneRegion() { return region().Duplicate(); }
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_COMMON_SHM_COUNT_H_
