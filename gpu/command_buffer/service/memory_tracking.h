// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_MEMORY_TRACKING_H_
#define GPU_COMMAND_BUFFER_SERVICE_MEMORY_TRACKING_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "gpu/command_buffer/common/command_buffer_id.h"
#include "gpu/gpu_export.h"
#include "gpu/ipc/common/gpu_peak_memory.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace gpu {

// A MemoryTracker is used to propagate per-ContextGroup memory usage
// statistics to the global GpuMemoryManager.
class GPU_EXPORT MemoryTracker {
 public:
  // Observe all changes in memory notified to this MemoryTracker.
  class Observer {
   public:
    Observer() = default;
    virtual ~Observer() = default;

    virtual void OnMemoryAllocatedChange(
        CommandBufferId id,
        uint64_t old_size,
        uint64_t new_size,
        GpuPeakMemoryAllocationSource source) = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(Observer);
  };

  virtual ~MemoryTracker() = default;
  virtual void TrackMemoryAllocatedChange(int64_t delta) = 0;
  virtual uint64_t GetSize() const = 0;

  // Raw ID identifying the GPU client for whom memory is being allocated.
  virtual int ClientId() const = 0;

  // Tracing id which identifies the GPU client for whom memory is being
  // allocated.
  virtual uint64_t ClientTracingId() const = 0;

  // Returns an ID that uniquely identifies the context group.
  virtual uint64_t ContextGroupTracingId() const = 0;
};

// A MemoryTypeTracker tracks the use of a particular type of memory (buffer,
// texture, or renderbuffer) and forward the result to a specified
// MemoryTracker. MemoryTypeTracker is thread-safe, but it must not outlive the
// MemoryTracker which will be notified on the sequence the MemoryTypeTracker
// was created on (if base::SequencedTaskRunnerHandle::IsSet()), or on the task
// runner specified (for testing).
class GPU_EXPORT MemoryTypeTracker {
 public:
  explicit MemoryTypeTracker(MemoryTracker* memory_tracker);
  // For testing.
  MemoryTypeTracker(MemoryTracker* memory_tracker,
                    scoped_refptr<base::SequencedTaskRunner> task_runner);
  ~MemoryTypeTracker();

  void TrackMemAlloc(size_t bytes);
  void TrackMemFree(size_t bytes);
  size_t GetMemRepresented() const;

 private:
  void TrackMemoryAllocatedChange(int64_t delta);

  MemoryTracker* const memory_tracker_;

  size_t mem_represented_ GUARDED_BY(lock_) = 0;
  mutable base::Lock lock_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  base::WeakPtrFactory<MemoryTypeTracker> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(MemoryTypeTracker);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_MEMORY_TRACKING_H_
