// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_ANDROID_CODEC_BUFFER_WAIT_COORDINATOR_H_
#define MEDIA_GPU_ANDROID_CODEC_BUFFER_WAIT_COORDINATOR_H_

#include "base/memory/ref_counted.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "gpu/command_buffer/service/ref_counted_lock.h"
#include "gpu/command_buffer/service/texture_owner.h"
#include "media/base/tuneable.h"
#include "media/gpu/media_gpu_export.h"

namespace media {

struct FrameAvailableEvent;

// This class supports waiting for codec buffers to be released/rendered before
// using them. This class is RefCountedThreadSafe to make sure it's safe to
// keep and drop refptrs to it on any thread. Note that when DrDc is
// enabled(kEnableDrDc), a per codec dr-dc lock is expected to be held while
// calling methods of this class. This is ensured by adding
// AssertAcquiredDrDcLock() to those methods.
class MEDIA_GPU_EXPORT CodecBufferWaitCoordinator
    : public base::RefCountedThreadSafe<CodecBufferWaitCoordinator>,
      public gpu::RefCountedLockHelperDrDc {
 public:
  explicit CodecBufferWaitCoordinator(
      scoped_refptr<gpu::TextureOwner> texture_owner,
      scoped_refptr<gpu::RefCountedLock> drdc_lock);

  CodecBufferWaitCoordinator(const CodecBufferWaitCoordinator&) = delete;
  CodecBufferWaitCoordinator& operator=(const CodecBufferWaitCoordinator&) =
      delete;

  scoped_refptr<gpu::TextureOwner> texture_owner() const {
    DCHECK(texture_owner_);
    return texture_owner_;
  }

  // Codec buffer wait management apis.
  // Sets the expectation of onFrameAVailable for a new frame because a buffer
  // was just released to this surface.
  virtual void SetReleaseTimeToNow();

  // Whether we're expecting onFrameAvailable. True when SetReleaseTimeToNow()
  // was called but WaitForFrameAvailable() have not been called since.
  virtual bool IsExpectingFrameAvailable();

  // Waits for onFrameAvailable until it's been 5ms since the buffer was
  // released. This must only be called if IsExpectingFrameAvailable().
  virtual void WaitForFrameAvailable();

  scoped_refptr<base::SingleThreadTaskRunner> task_runner() {
    return task_runner_;
  }

 protected:
  virtual ~CodecBufferWaitCoordinator();

 private:
  friend class base::RefCountedThreadSafe<CodecBufferWaitCoordinator>;

  scoped_refptr<gpu::TextureOwner> texture_owner_;

  base::TimeTicks release_time_;
  scoped_refptr<FrameAvailableEvent> frame_available_event_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // 5msec covers >99.9% of cases, so just wait for up to that much before
  // giving up. If an error occurs, we might not ever get a notification.
  Tuneable<base::TimeDelta> max_wait_ = {
      "MediaCodecOutputBufferMaxWaitTime", base::Milliseconds(0),
      base::Milliseconds(5), base::Milliseconds(20)};
};

}  // namespace media

#endif  // MEDIA_GPU_ANDROID_CODEC_BUFFER_WAIT_COORDINATOR_H_
