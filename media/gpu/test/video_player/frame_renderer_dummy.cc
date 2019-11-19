// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/test/video_player/frame_renderer_dummy.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/memory/ptr_util.h"
#include "media/gpu/macros.h"
#include "media/gpu/test/video_frame_helpers.h"

namespace media {
namespace test {

FrameRendererDummy::FrameRendererDummy(base::TimeDelta frame_duration,
                                       base::TimeDelta vsync_interval_duration)
    : frame_duration_(frame_duration),
      vsync_interval_duration_(vsync_interval_duration),
      frames_to_drop_decoding_slow_(0),
      frames_to_drop_rendering_slow_(0),
      frames_dropped_(0),
      renderer_thread_("FrameRendererThread"),
      renderer_cv_(&renderer_lock_) {
  DETACH_FROM_SEQUENCE(client_sequence_checker_);
  DETACH_FROM_SEQUENCE(renderer_sequence_checker_);
}

FrameRendererDummy::~FrameRendererDummy() {
  Destroy();
}

// static
std::unique_ptr<FrameRendererDummy> FrameRendererDummy::Create(
    base::TimeDelta frame_duration,
    base::TimeDelta vsync_interval_duration) {
  auto frame_renderer = base::WrapUnique(
      new FrameRendererDummy(frame_duration, vsync_interval_duration));
  if (!frame_renderer->Initialize()) {
    return nullptr;
  }
  return frame_renderer;
}

bool FrameRendererDummy::Initialize() {
  if (!renderer_thread_.Start()) {
    LOG(ERROR) << "Failed to start frame renderer thread";
    return false;
  }

  base::WaitableEvent done;
  renderer_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&FrameRendererDummy::InitializeTask,
                                base::Unretained(this), &done));
  done.Wait();

  return true;
}

void FrameRendererDummy::Destroy() {
  base::WaitableEvent done;
  renderer_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&FrameRendererDummy::DestroyTask,
                                base::Unretained(this), &done));
  done.Wait();

  renderer_thread_.Stop();

  base::AutoLock auto_lock(renderer_lock_);
  DCHECK(pending_frames_.empty());
}

bool FrameRendererDummy::AcquireGLContext() {
  // As no actual rendering is done we don't have a GLContext to acquire.
  return true;
}

gl::GLContext* FrameRendererDummy::GetGLContext() {
  // As no actual rendering is done we don't have a GLContext.
  return nullptr;
}

void FrameRendererDummy::RenderFrame(scoped_refptr<VideoFrame> video_frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);

  // If the frame duration is 0 we are decoding as fast as possible, immediately
  // return the frame to the decoder for reuse.
  if (frame_duration_.is_zero())
    return;

  // If rendering or decoding is running behind, drop the frame so the buffer is
  // immediately released for reuse. Only frames dropped due to slow decoding
  // will be counted as dropped frames.
  base::AutoLock auto_lock(renderer_lock_);
  if (!video_frame->metadata()->IsTrue(VideoFrameMetadata::END_OF_STREAM)) {
    if (frames_to_drop_rendering_slow_ > 0) {
      frames_to_drop_rendering_slow_--;
      return;
    } else if (frames_to_drop_decoding_slow_ > 0) {
      DVLOGF(4) << "Dropping frame: decoder too slow";
      frames_to_drop_decoding_slow_--;
      frames_dropped_++;
      return;
    }
  }

  pending_frames_.push(std::move(video_frame));

  // If this is the first frame decoded, render it immediately.
  if (next_frame_time_.is_null()) {
    next_frame_time_ = base::TimeTicks::Now();
    renderer_thread_.task_runner()->PostTask(FROM_HERE,
                                             render_task_.callback());
  }
}

void FrameRendererDummy::WaitUntilRenderingDone() {
  base::AutoLock auto_lock(renderer_lock_);
  while (!pending_frames_.empty()) {
    renderer_cv_.Wait();
  }
}

scoped_refptr<VideoFrame> FrameRendererDummy::CreateVideoFrame(
    VideoPixelFormat pixel_format,
    const gfx::Size& size,
    uint32_t texture_target,
    uint32_t* texture_id) {
  *texture_id = 0;

  // Create a dummy video frame. No actual rendering will be done but the video
  // frame's properties such as timestamp will be used.
  // TODO(dstaessens): Remove this function when allocate mode is deprecated.
  base::Optional<VideoFrameLayout> layout =
      CreateVideoFrameLayout(pixel_format, size);
  DCHECK(layout);
  return VideoFrame::WrapExternalDataWithLayout(*layout, gfx::Rect(size), size,
                                                nullptr, 0, base::TimeDelta());
}

uint64_t FrameRendererDummy::FramesDropped() const {
  base::AutoLock auto_lock(renderer_lock_);
  return frames_dropped_;
}

void FrameRendererDummy::InitializeTask(base::WaitableEvent* done) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(renderer_sequence_checker_);

  render_task_.Reset(base::BindRepeating(&FrameRendererDummy::RenderFrameTask,
                                         base::Unretained(this)));
  done->Signal();
}

void FrameRendererDummy::DestroyTask(base::WaitableEvent* done) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(renderer_sequence_checker_);

  render_task_.Cancel();
  done->Signal();
}

void FrameRendererDummy::RenderFrameTask() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(renderer_sequence_checker_);

  // Display the next frame. If no new frames are available yet, decoding is
  // running behind. Keep displaying the current frame and immediately drop the
  // next frame that arrives.
  base::AutoLock auto_lock(renderer_lock_);
  if (pending_frames_.size() > 0) {
    active_frame_ = std::move(pending_frames_.front());
    pending_frames_.pop();
  } else {
    frames_to_drop_decoding_slow_++;
  }

  if (!active_frame_->metadata()->IsTrue(VideoFrameMetadata::END_OF_STREAM)) {
    ScheduleNextRenderFrameTask();
  } else {
    next_frame_time_ = base::TimeTicks();
    // Frames-to-drop might be higher than 0 if decoding is running behind.
    // We don't know the number of frames in the stream until we see an EOS
    // frame, so the renderer keeps trying to render frames if the EOS frame is
    // late. This means we might schedule more frames to be dropped than the
    // video actually has.
    frames_to_drop_decoding_slow_ = 0;
    frames_to_drop_rendering_slow_ = 0;
  }

  renderer_cv_.Signal();
}

void FrameRendererDummy::ScheduleNextRenderFrameTask() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(renderer_sequence_checker_);

  base::TimeTicks now = base::TimeTicks::Now();

  // If this is the first frame, set the base time to be used for vsync. All
  // frames will be rendered at |vsync_timebase_| + (x * |vsync_interval_|)
  if (vsync_timebase_.is_null())
    vsync_timebase_ = now;

  next_frame_time_ += frame_duration_;
  base::TimeTicks next_render_time;

  if (vsync_interval_duration_.is_zero()) {
    // No Vsync is present. Render the next frame at the expected frame time.
    next_render_time = std::max(now, next_frame_time_);
  } else {
    // - Video frame rate < Vsync rate: Render the next frame at the latest
    //   Vsync before the |next_frame_render_time_|.
    // - Video frame rate > Vsync rate: Render the first frame after the next
    //   Vsync. This might result in one or more frames getting dropped.
    next_render_time =
        std::max(now + vsync_interval_duration_, next_frame_time_);
    int64_t num_intervals_timebase =
        (next_render_time - vsync_timebase_) / vsync_interval_duration_;
    next_render_time =
        vsync_timebase_ + (num_intervals_timebase * vsync_interval_duration_);
  }

  // The time at which the next frame will be rendered might be later then the
  // time at which the next frame appears in the stream. This means we are
  // displaying less frames than are being decoded and we should drop frames.
  // e.g. Vsync at 30fps, video frame rate 60fps. We shouldn't increase
  // |dropped_frames_| in this case, as the decoder is not falling behind.
  while (next_frame_time_ < next_render_time) {
    next_frame_time_ += frame_duration_;
    if (pending_frames_.size() > 0) {
      pending_frames_.pop();
    } else {
      // The next video frame has not been decoded yet. Drop it immediately
      // when RenderFrame() is called.
      frames_to_drop_rendering_slow_++;
    }
  }

  renderer_thread_.task_runner()->PostDelayedTask(
      FROM_HERE, render_task_.callback(), (next_render_time - now));
}

}  // namespace test
}  // namespace media
