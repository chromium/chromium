// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_TEST_VIDEO_PLAYER_FRAME_RENDERER_DUMMY_H_
#define MEDIA_GPU_TEST_VIDEO_PLAYER_FRAME_RENDERER_DUMMY_H_

#include <memory>

#include "base/cancelable_callback.h"
#include "base/containers/queue.h"
#include "base/sequence_checker.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "media/base/video_types.h"
#include "ui/gfx/geometry/size.h"

namespace media {

class VideoFrame;

namespace test {

// The dummy frame renderer can be used when we're not interested in rendering
// the decoded frames to screen or file. The renderer can either consume frames
// immediately, or introduce a delay to simulate actual rendering.
class FrameRendererDummy {
 public:
  FrameRendererDummy(const FrameRendererDummy&) = delete;
  FrameRendererDummy& operator=(const FrameRendererDummy&) = delete;

  ~FrameRendererDummy();

  // Create an instance of the dummy frame renderer. |frame_duration| specifies
  // how long we will simulate displaying each frame, typically the inverse of
  // the stream's frame rate. If 0 no rendering will be simulated and frames
  // will be returned to the decoder immediately. |vsync_interval_duration|
  // specifies the desired VSync interval. If 0 VSync will be disabled.
  static std::unique_ptr<FrameRendererDummy> Create(
      base::TimeDelta frame_duration = base::TimeDelta(),
      base::TimeDelta vsync_interval_duration = base::TimeDelta());

  // Render the specified video frame. Once rendering is done the reference to
  // the |video_frame| should be dropped so the video frame can be reused. If
  // the specified frame is an EOS frame, the frame renderer will assume the
  // next frame received is unrelated to the previous one, and any internal
  // state can be reset. This is e.g. important when calculating the frame
  // drop rate.
  void RenderFrame(scoped_refptr<VideoFrame> video_frame);
  // Wait until all currently queued frames are rendered. This function might
  // take some time to complete, depending on the number of frames queued.
  void WaitUntilRenderingDone();
  // Create a texture-backed video frame with specified |pixel_format|, |size|
  // and |texture_target|. The texture's id will be put in |texture_id|.
  // TODO(dstaessens@) Remove when allocate mode is removed.
  scoped_refptr<VideoFrame> CreateVideoFrame(VideoPixelFormat pixel_format,
                                             const gfx::Size& size,
                                             uint32_t texture_target,
                                             uint32_t* texture_id);

  // Get the number of frames dropped due to the decoder running behind.
  uint64_t FramesDropped() const;

 private:
  FrameRendererDummy(base::TimeDelta frame_duration,
                     base::TimeDelta vsync_interval_duration);

  // Initialize the frame renderer, performs all rendering-related setup.
  bool Initialize();
  void Destroy();

  // Tasks run on the renderer thread.
  void InitializeTask(base::WaitableEvent* done);
  void DestroyTask(base::WaitableEvent* done);
  void RenderFrameTask();
  void ScheduleNextRenderFrameTask() EXCLUSIVE_LOCKS_REQUIRED(renderer_lock_);

  // Target duration we will simulate rendering each frame.
  const base::TimeDelta frame_duration_;
  // The VSync interval to be used.
  const base::TimeDelta vsync_interval_duration_;
  // Time at which we started displaying frames.
  base::TimeTicks vsync_timebase_;

  // The frame being displayed, only accessed on |renderer_thread_|.
  scoped_refptr<VideoFrame> active_frame_;
  // The queue of decoded video frames pending to be displayed.
  base::queue<scoped_refptr<VideoFrame>> pending_frames_
      GUARDED_BY(renderer_lock_);
  // Time at which the next decoded frame should be rendered.
  base::TimeTicks next_frame_time_ GUARDED_BY(renderer_lock_);

  // Number of frames that need to be dropped because the decoder is too slow.
  uint64_t frames_to_drop_decoding_slow_ GUARDED_BY(renderer_lock_);
  // Number of frames that need to be dropped because we're rendering at a lower
  // rate then the video's frame rate.
  uint64_t frames_to_drop_rendering_slow_ GUARDED_BY(renderer_lock_);
  // Number of frames dropped due to decoder running behind.
  uint64_t frames_dropped_ GUARDED_BY(renderer_lock_);

  // Task that simulates rendering a frame to screen.
  base::CancelableRepeatingClosure render_task_;
  // Thread on which rendering video frames is simulated.
  base::Thread renderer_thread_;
  mutable base::Lock renderer_lock_;
  base::ConditionVariable renderer_cv_;

  SEQUENCE_CHECKER(client_sequence_checker_);
  SEQUENCE_CHECKER(renderer_sequence_checker_);
};

}  // namespace test
}  // namespace media

#endif  // MEDIA_GPU_TEST_VIDEO_PLAYER_FRAME_RENDERER_DUMMY_H_
