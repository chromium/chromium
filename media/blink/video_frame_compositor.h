// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BLINK_VIDEO_FRAME_COMPOSITOR_H_
#define MEDIA_BLINK_VIDEO_FRAME_COMPOSITOR_H_

#include <utility>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/trace_event/auto_open_close_event.h"
#include "cc/layers/surface_layer.h"
#include "cc/layers/video_frame_provider.h"
#include "media/base/video_renderer_sink.h"
#include "media/blink/media_blink_export.h"
#include "media/blink/webmediaplayer_params.h"
#include "third_party/blink/public/platform/web_video_frame_submitter.h"
#include "ui/gfx/geometry/size.h"

namespace viz {
class SurfaceId;
}

namespace media {
class VideoFrame;

// VideoFrameCompositor acts as a bridge between the media and cc layers for
// rendering video frames. I.e. a media::VideoRenderer will talk to this class
// from the media side, while a cc::VideoFrameProvider::Client will talk to it
// from the cc side.
//
// This class is responsible for requesting new frames from a video renderer in
// response to requests from the VFP::Client. Since the VFP::Client may stop
// issuing requests in response to visibility changes it is also responsible for
// ensuring the "freshness" of the current frame for programmatic frame
// requests; e.g., Canvas.drawImage() requests
//
// This class is also responsible for detecting frames dropped by the compositor
// after rendering and signaling that information to a RenderCallback. It
// detects frames not dropped by verifying each GetCurrentFrame() is followed
// by a PutCurrentFrame() before the next UpdateCurrentFrame() call.
//
// VideoRenderSink::RenderCallback implementations must call Start() and Stop()
// once new frames are expected or are no longer expected to be ready; this data
// is relayed to the compositor to avoid extraneous callbacks.
//
// VideoFrameCompositor is also responsible for pumping UpdateCurrentFrame()
// callbacks in the background when |client_| has decided to suspend them.
//
// VideoFrameCompositor must live on the same thread as the compositor, though
// it may be constructed on any thread.
class MEDIA_BLINK_EXPORT VideoFrameCompositor : public VideoRendererSink,
                                                public cc::VideoFrameProvider {
 public:
  // Used to report back the time when the new frame has been processed.
  using OnNewProcessedFrameCB = base::OnceCallback<void(base::TimeTicks)>;

  using OnNewFramePresentedCB =
      base::OnceCallback<void(scoped_refptr<VideoFrame> presented_frame,
                              base::TimeTicks presentation_time,
                              base::TimeTicks expected_presentation_time,
                              uint32_t presentation_counter)>;

  // |task_runner| is the task runner on which this class will live,
  // though it may be constructed on any thread.
  VideoFrameCompositor(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
      std::unique_ptr<blink::WebVideoFrameSubmitter> submitter);

  // Destruction must happen on the compositor thread; Stop() must have been
  // called before destruction starts.
  ~VideoFrameCompositor() override;

  // Can be called from any thread.
  cc::UpdateSubmissionStateCB GetUpdateSubmissionStateCallback();

  // Signals the VideoFrameSubmitter to prepare to receive BeginFrames and
  // submit video frames given by VideoFrameCompositor.
  virtual void EnableSubmission(
      const viz::SurfaceId& id,
      base::TimeTicks local_surface_id_allocation_time,
      VideoRotation rotation,
      bool force_submit);

  // cc::VideoFrameProvider implementation. These methods must be called on the
  // |task_runner_|.
  void SetVideoFrameProviderClient(
      cc::VideoFrameProvider::Client* client) override;
  bool UpdateCurrentFrame(base::TimeTicks deadline_min,
                          base::TimeTicks deadline_max) override;
  bool HasCurrentFrame() override;
  scoped_refptr<VideoFrame> GetCurrentFrame() override;
  void PutCurrentFrame() override;
  base::TimeDelta GetPreferredRenderInterval() override;

  // Returns |current_frame_|, without offering a guarantee as to how recently
  // it was updated. In certain applications, one might need to periodically
  // call UpdateCurrentFrameIfStale on |task_runner_| to drive the updates.
  // Can be called from any thread.
  virtual scoped_refptr<VideoFrame> GetCurrentFrameOnAnyThread();

  // VideoRendererSink implementation. These methods must be called from the
  // same thread (typically the media thread).
  void Start(RenderCallback* callback) override;
  void Stop() override;
  void PaintSingleFrame(scoped_refptr<VideoFrame> frame,
                        bool repaint_duplicate_frame = false) override;

  // If |client_| is not set, |callback_| is set, and |is_background_rendering_|
  // is true, it requests a new frame from |callback_|. Uses the elapsed time
  // between calls to this function as the render interval, defaulting to 16.6ms
  // if no prior calls have been made. A cap of 250Hz (4ms) is in place to
  // prevent clients from accidentally (or intentionally) spamming the rendering
  // pipeline.
  //
  // This method is primarily to facilitate canvas and WebGL based applications
  // where the <video> tag is invisible (possibly not even in the DOM) and thus
  // does not receive a |client_|.  In this case, frame acquisition is driven by
  // the frequency of canvas or WebGL paints requested via JavaScript.
  void UpdateCurrentFrameIfStale();

  // Sets the callback to be run when the new frame has been processed. The
  // callback is only run once and then reset.
  // Must be called on the compositor thread.
  virtual void SetOnNewProcessedFrameCallback(OnNewProcessedFrameCB cb);

  virtual void SetOnFramePresentedCallback(OnNewFramePresentedCB present_cb);

  // Updates the rotation information for frames given to |submitter_|.
  void UpdateRotation(VideoRotation rotation);

  // Should be called when page visibility changes. Notifies |submitter_|.
  virtual void SetIsPageVisible(bool is_visible);

  // Notifies the |submitter_| that the frames must be submitted.
  void SetForceSubmit(bool force_submit);

  void set_tick_clock_for_testing(const base::TickClock* tick_clock) {
    tick_clock_ = tick_clock;
  }

  // Enables or disables background rendering. If |enabled|, |timeout| is the
  // amount of time to wait after the last Render() call before starting the
  // background rendering mode.  Note, this can not disable the background
  // rendering call issues when a sink is started.
  void set_background_rendering_for_testing(bool enabled) {
    background_rendering_enabled_ = enabled;
  }

  void set_submitter_for_test(
      std::unique_ptr<blink::WebVideoFrameSubmitter> submitter) {
    submitter_ = std::move(submitter);
  }

 private:
  // TracingCategory name for |auto_open_close_|.
  static constexpr const char kTracingCategory[] = "media,rail";

  // Ran on the |task_runner_| to initialize |submitter_|;
  void InitializeSubmitter();

  // Signals the VideoFrameSubmitter to stop submitting frames. Sets whether the
  // video surface is visible within the view port.
  void SetIsSurfaceVisible(bool is_visible);

  // Indicates whether the endpoint for the VideoFrame exists.
  bool IsClientSinkAvailable();

  // Called on the compositor thread in response to Start() or Stop() calls;
  // must be used to change |rendering_| state.
  void OnRendererStateUpdate(bool new_state);

  // Handles setting of |current_frame_|.
  bool ProcessNewFrame(scoped_refptr<VideoFrame> frame,
                       base::TimeTicks presentation_time,
                       bool repaint_duplicate_frame);

  void SetCurrentFrame(scoped_refptr<VideoFrame> frame);

  // Called by |background_rendering_timer_| when enough time elapses where we
  // haven't seen a Render() call.
  void BackgroundRender();

  // If |callback_| is available, calls Render() with the provided properties.
  // Updates |is_background_rendering_|, |last_interval_|, and resets
  // |background_rendering_timer_|. Returns true if there's a new frame
  // available via GetCurrentFrame().
  bool CallRender(base::TimeTicks deadline_min,
                  base::TimeTicks deadline_max,
                  bool background_rendering);

  // This will run tasks on the compositor thread. If
  // kEnableSurfaceLayerForVideo is enabled, it will instead run tasks on the
  // media thread.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  const base::TickClock* tick_clock_;

  // Allows tests to disable the background rendering task.
  bool background_rendering_enabled_ = true;

  // Manages UpdateCurrentFrame() callbacks if |client_| has stopped sending
  // them for various reasons.  Runs on |task_runner_| and is reset
  // after each successful UpdateCurrentFrame() call.
  base::RetainingOneShotTimer background_rendering_timer_;

  // These values are only set and read on the compositor thread.
  cc::VideoFrameProvider::Client* client_ = nullptr;
  bool rendering_ = false;
  bool rendered_last_frame_ = false;
  bool is_background_rendering_ = false;
  bool new_background_frame_ = false;

  // Assume 60Hz before the first UpdateCurrentFrame() call.
  base::TimeDelta last_interval_ = base::TimeDelta::FromSecondsD(1.0 / 60);

  base::TimeTicks last_background_render_;
  OnNewProcessedFrameCB new_processed_frame_cb_;
  OnNewFramePresentedCB new_presented_frame_cb_;
  cc::UpdateSubmissionStateCB update_submission_state_callback_;

  // Set on the compositor thread, but also read on the media thread. Lock is
  // not used when reading |current_frame_| on the compositor thread.
  base::Lock current_frame_lock_;
  scoped_refptr<VideoFrame> current_frame_;

  // These values are updated and read from the media and compositor threads.
  base::Lock callback_lock_;
  VideoRendererSink::RenderCallback* callback_ GUARDED_BY(callback_lock_) =
      nullptr;

  uint32_t presentation_counter_ = 0u;

  // AutoOpenCloseEvent for begin/end events.
  std::unique_ptr<base::trace_event::AutoOpenCloseEvent<kTracingCategory>>
      auto_open_close_;
  std::unique_ptr<blink::WebVideoFrameSubmitter> submitter_;

  base::WeakPtrFactory<VideoFrameCompositor> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(VideoFrameCompositor);
};

}  // namespace media

#endif  // MEDIA_BLINK_VIDEO_FRAME_COMPOSITOR_H_
