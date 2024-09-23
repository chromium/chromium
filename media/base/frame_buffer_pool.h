// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_FRAME_BUFFER_POOL_H_
#define MEDIA_BASE_FRAME_BUFFER_POOL_H_

#include <stdint.h>

#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "base/time/default_tick_clock.h"
#include "base/trace_event/memory_dump_provider.h"
#include "media/base/media_export.h"

namespace media {

// FrameBufferPool is a pool of simple CPU memory. This class needs to be ref-
// counted since frames created using this memory may live beyond the lifetime
// of the caller to this class. This class is thread-safe.
class MEDIA_EXPORT FrameBufferPool
    : public base::RefCountedThreadSafe<FrameBufferPool> {
 public:
  // Must be called on the same thread that `Shutdown()` is called on.  If
  // `zero_initialize_memory` is true, then initial allocations will be
  // cleared.  This does not affect reused buffers, which are never cleared.
  explicit FrameBufferPool(bool zero_initialize_memory = false);

  FrameBufferPool(const FrameBufferPool&) = delete;
  FrameBufferPool& operator=(const FrameBufferPool&) = delete;

  // Called when a frame buffer allocation is needed. Upon return |fb_priv| will
  // be set to a private value used to identify the buffer in future calls and a
  // buffer of at least |min_size| will be returned.
  //
  // WARNING: To release the FrameBuffer, clients must either call Shutdown() or
  // ReleaseFrameBuffer() in addition to any callbacks returned by
  // CreateFrameCallback() (if any are created).
  uint8_t* GetFrameBuffer(size_t min_size, void** fb_priv);

  // Called when a frame buffer allocation is no longer needed.
  void ReleaseFrameBuffer(void* fb_priv);

  // Allocates (or reuses) room for an alpha plane on a given frame buffer.
  // |fb_priv| must be a value previously returned by GetFrameBuffer().
  uint8_t* AllocateAlphaPlaneForFrameBuffer(size_t min_size, void* fb_priv);

  // Generates a "no_longer_needed" closure that holds a reference to this pool;
  // |fb_priv| must be a value previously returned by GetFrameBuffer(). The
  // callback may be called on any thread.
  base::OnceClosure CreateFrameCallback(void* fb_priv);

  size_t get_pool_size_for_testing() const {
    base::AutoLock lock(lock_);
    return frame_buffers_.size();
  }

  void set_tick_clock_for_testing(const base::TickClock* tick_clock) {
    base::AutoLock lock(lock_);
    tick_clock_ = tick_clock;
  }

  void force_allocation_error_for_testing() {
    base::AutoLock lock(lock_);
    force_allocation_error_ = true;
  }

  // Called when no more GetFrameBuffer() calls are expected. All unused memory
  // is released at this time. As frames are returned their memory is released.
  // This should not be called until anything that might call GetFrameBuffer()
  // has been destroyed. Must be called on the same thread as the ctor.
  void Shutdown();

  enum { kStaleFrameLimitSecs = 10 };

 private:
  friend class base::RefCountedThreadSafe<FrameBufferPool>;
  ~FrameBufferPool();

  // Internal structure holding memory for decoding.
  struct FrameBuffer;

  // base::MemoryDumpProvider.
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd);

  // Not safe to concurrent modifications to `buf`.  While the method is static,
  // it's expected, but unchecked, that the caller holds `locked_` to prevent
  // any modifications to `buf` during this call.
  static bool IsUsedLocked(const FrameBuffer* buf);

  // Drop all entries in |frame_buffers_| that report !IsUsedLocked().  Must be
  // called with `lock_` held.
  void EraseUnusedResourcesLocked();

  // Method that gets called when a VideoFrame that references this pool gets
  // destroyed.
  void OnVideoFrameDestroyed(FrameBuffer* frame_buffer);

  // Should we allocate memory and clear it, or just allocate it?  Note that
  // memory is never cleared when reusing a previously returned buffer; only
  // the initial allocation is affected.
  const bool zero_initialize_memory_ = false;

  // Here at the framebuffer cafe, all our dining philosophers share one fork.
  mutable base::Lock lock_;

  // Allocated frame buffers.
  std::vector<std::unique_ptr<FrameBuffer>> frame_buffers_ GUARDED_BY(lock_);

  bool in_shutdown_ GUARDED_BY(lock_) = false;

  bool force_allocation_error_ GUARDED_BY(lock_) = false;

  // |tick_clock_| is always a DefaultTickClock outside of testing.
  raw_ptr<const base::TickClock> tick_clock_ GUARDED_BY(lock_);

  // Maintains ownership of the memory dumping infrastructure. Holds a ref on
  // FrameBufferPool until Shutdown().
  class FrameBufferMemoryDumpProviderImpl;
  std::unique_ptr<FrameBufferMemoryDumpProviderImpl> memory_dump_impl_
      GUARDED_BY(lock_);
};

}  // namespace media

#endif  // MEDIA_BASE_FRAME_BUFFER_POOL_H_
