// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_FRAME_BUFFER_POOL_H_
#define MEDIA_FILTERS_FRAME_BUFFER_POOL_H_

#include <stdint.h>

#include <vector>

#include "base/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "base/time/default_tick_clock.h"
#include "base/trace_event/memory_dump_provider.h"
#include "media/base/media_export.h"

namespace base {
class SequencedTaskRunner;
}

namespace media {

// FrameBufferPool is a pool of simple CPU memory. This class needs to be ref-
// counted since frames created using this memory may live beyond the lifetime
// of the caller to this class.
class MEDIA_EXPORT FrameBufferPool
    : public base::RefCountedThreadSafe<FrameBufferPool>,
      public base::trace_event::MemoryDumpProvider {
 public:
  FrameBufferPool();

  FrameBufferPool(const FrameBufferPool&) = delete;
  FrameBufferPool& operator=(const FrameBufferPool&) = delete;

  // Called when a frame buffer allocation is needed. Upon return |fb_priv| will
  // be set to a private value used to identify the buffer in future calls and a
  // buffer of at least |min_size| will be returned.
  uint8_t* GetFrameBuffer(size_t min_size, void** fb_priv);

  // Called when a frame buffer allocation is no longer needed.
  void ReleaseFrameBuffer(void* fb_priv);

  // Allocates (or reuses) room for an alpha plane on a given frame buffer.
  // |fb_priv| must be a value previously returned by GetFrameBuffer().
  uint8_t* AllocateAlphaPlaneForFrameBuffer(size_t min_size, void* fb_priv);

  // Generates a "no_longer_needed" closure that holds a reference to this pool;
  // |fb_priv| must be a value previously returned by GetFrameBuffer().
  base::OnceClosure CreateFrameCallback(void* fb_priv);

  size_t get_pool_size_for_testing() const { return frame_buffers_.size(); }

  void set_tick_clock_for_testing(const base::TickClock* tick_clock) {
    tick_clock_ = tick_clock;
  }

  void force_allocation_error_for_testing() { force_allocation_error_ = true; }

  // Called when no more GetFrameBuffer() calls are expected. All unused memory
  // is released at this time. As frames are returned their memory is released.
  // This should not be called until anything that might call GetFrameBuffer()
  // has been destroyed.
  void Shutdown();

  enum { kStaleFrameLimitSecs = 10 };

 private:
  friend class base::RefCountedThreadSafe<FrameBufferPool>;
  ~FrameBufferPool() override;

  // Internal structure holding memory for decoding.
  struct FrameBuffer;

  // base::MemoryDumpProvider.
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

  static bool IsUsed(const FrameBuffer* buf);

  // Drop all entries in |frame_buffers_| that report !IsUsed().
  void EraseUnusedResources();

  // Method that gets called when a VideoFrame that references this pool gets
  // destroyed.
  void OnVideoFrameDestroyed(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      FrameBuffer* frame_buffer);

  // Allocated frame buffers.
  std::vector<std::unique_ptr<FrameBuffer>> frame_buffers_;

  bool in_shutdown_ = false;

  bool registered_dump_provider_ = false;

  bool force_allocation_error_ = false;

  // |tick_clock_| is always a DefaultTickClock outside of testing.
  raw_ptr<const base::TickClock> tick_clock_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace media

#endif  // MEDIA_FILTERS_FRAME_BUFFER_POOL_H_
