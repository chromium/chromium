// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_STREAM_WEBMEDIAPLAYER_MS_COMPOSITOR_H_
#define CONTENT_RENDERER_MEDIA_STREAM_WEBMEDIAPLAYER_MS_COMPOSITOR_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <vector>

#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "cc/layers/surface_layer.h"
#include "cc/layers/video_frame_provider.h"
#include "content/common/content_export.h"
#include "media/base/media_log.h"
#include "media/blink/webmediaplayer_params.h"
#include "third_party/blink/public/platform/web_video_frame_submitter.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace blink {
class WebMediaStream;
}

namespace gfx {
class Size;
}

namespace media {
class VideoRendererAlgorithm;
}

namespace viz {
class SurfaceId;
}

namespace content {
class WebMediaPlayerMS;

// This class is designed to handle the work load on compositor thread for
// WebMediaPlayerMS. It will be instantiated on the main thread, but destroyed
// on the thread holding the last reference.
//
// WebMediaPlayerMSCompositor utilizes VideoRendererAlgorithm to store the
// incoming frames and select the best frame for rendering to maximize the
// smoothness, if REFERENCE_TIMEs are populated for incoming VideoFrames.
// Otherwise, WebMediaPlayerMSCompositor will simply store the most recent
// frame, and submit it whenever asked by the compositor.
class CONTENT_EXPORT WebMediaPlayerMSCompositor
    : public cc::VideoFrameProvider,
      public base::RefCountedDeleteOnSequence<WebMediaPlayerMSCompositor> {
 public:
  // This |url| represents the media stream we are rendering. |url| is used to
  // find out what web stream this WebMediaPlayerMSCompositor is playing, and
  // together with flag "--disable-rtc-smoothness-algorithm" determine whether
  // we enable algorithm or not.
  WebMediaPlayerMSCompositor(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
      const blink::WebMediaStream& web_stream,
      std::unique_ptr<blink::WebVideoFrameSubmitter> submitter,
      blink::WebMediaPlayer::SurfaceLayerMode surface_layer_mode,
      const base::WeakPtr<WebMediaPlayerMS>& player);

  // Can be called from any thread.
  cc::UpdateSubmissionStateCB GetUpdateSubmissionStateCallback() {
    return update_submission_state_callback_;
  }

  void EnqueueFrame(scoped_refptr<media::VideoFrame> frame);

  // Statistical data
  gfx::Size GetCurrentSize();
  base::TimeDelta GetCurrentTime();
  size_t total_frame_count();
  size_t dropped_frame_count();

  // Signals the VideoFrameSubmitter to prepare to receive BeginFrames and
  // submit video frames given by WebMediaPlayerMSCompositor.
  virtual void EnableSubmission(
      const viz::SurfaceId& id,
      base::TimeTicks local_surface_id_allocation_time,
      media::VideoRotation rotation,
      bool force_submit,
      bool is_opaque,
      blink::WebFrameSinkDestroyedCallback frame_sink_destroyed_callback);

  // Notifies the |submitter_| that the frames must be submitted.
  void SetForceSubmit(bool force_submit);

  // VideoFrameProvider implementation.
  void SetVideoFrameProviderClient(
      cc::VideoFrameProvider::Client* client) override;
  bool UpdateCurrentFrame(base::TimeTicks deadline_min,
                          base::TimeTicks deadline_max) override;
  bool HasCurrentFrame() override;
  scoped_refptr<media::VideoFrame> GetCurrentFrame() override;
  void PutCurrentFrame() override;

  // Return the current frame being rendered.
  // Difference between GetCurrentFrame(): GetCurrentFrame() is designed for
  // chrome compositor to pull frame from WebMediaPlayerMSCompositor, and thus
  // calling GetCurrentFrame() will affect statistics like |dropped_frames_|
  // etc. Calling this function has no side effect.
  scoped_refptr<media::VideoFrame> GetCurrentFrameWithoutUpdatingStatistics();

  void StartRendering();
  void StopRendering();
  void ReplaceCurrentFrameWithACopy();

  // Tell |video_frame_provider_client_| to stop using this instance in
  // preparation for dtor.
  void StopUsingProvider();

 private:
  friend class base::RefCountedDeleteOnSequence<WebMediaPlayerMSCompositor>;
  friend class base::DeleteHelper<WebMediaPlayerMSCompositor>;
  friend class WebMediaPlayerMSTest;

  ~WebMediaPlayerMSCompositor() override;

  // Ran on the |video_frame_compositor_task_runner_| to initialize
  // |submitter_|
  void InitializeSubmitter();

  // Signals the VideoFrameSubmitter to stop submitting frames.
  void UpdateSubmissionState(bool);

  bool MapTimestampsToRenderTimeTicks(
      const std::vector<base::TimeDelta>& timestamps,
      std::vector<base::TimeTicks>* wall_clock_times);

  // For algorithm enabled case only: given the render interval, call
  // SetCurrentFrame() if a new frame is available.
  // |video_frame_provider_client_| gets notified about the new frame when it
  // calls UpdateCurrentFrame().
  void RenderUsingAlgorithm(base::TimeTicks deadline_min,
                            base::TimeTicks deadline_max);

  // For algorithm disabled case only: call SetCurrentFrame() with the current
  // frame immediately. |video_frame_provider_client_| gets notified about the
  // new frame with a DidReceiveFrame() call.
  void RenderWithoutAlgorithm(const scoped_refptr<media::VideoFrame>& frame);
  void RenderWithoutAlgorithmOnCompositor(
      const scoped_refptr<media::VideoFrame>& frame);

  // Update |current_frame_| and |dropped_frame_count_|
  void SetCurrentFrame(const scoped_refptr<media::VideoFrame>& frame);
  // Following the update to |current_frame_|, this will check for changes that
  // require updating video layer.
  void CheckForFrameChanges(const scoped_refptr<media::VideoFrame>& old_frame,
                            const scoped_refptr<media::VideoFrame>& new_frame);

  void StartRenderingInternal();
  void StopRenderingInternal();
  void StopUsingProviderInternal();
  void ReplaceCurrentFrameWithACopyInternal();

  void SetAlgorithmEnabledForTesting(bool algorithm_enabled);

  // Used for DCHECKs to ensure method calls executed in the correct thread,
  // which is renderer main thread in this class.
  base::ThreadChecker thread_checker_;

  const scoped_refptr<base::SingleThreadTaskRunner>
      video_frame_compositor_task_runner_;
  const scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;
  base::MessageLoop* main_message_loop_;

  base::WeakPtr<WebMediaPlayerMS> player_;

  // TODO(qiangchen, emircan): It might be nice to use a real MediaLog here from
  // the WebMediaPlayerMS instance, but it owns the MediaLog and this class has
  // non-deterministic destruction paths (either compositor or IO).
  media::MediaLog media_log_;

  size_t serial_;

  // A pointer back to the compositor to inform it about state changes. This
  // is not |nullptr| while the compositor is actively using this
  // VideoFrameProvider. This will be set to |nullptr| when the compositor stops
  // serving this VideoFrameProvider.
  cc::VideoFrameProvider::Client* video_frame_provider_client_;

  // |current_frame_| is updated only on compositor thread. The object it
  // holds can be freed on the compositor thread if it is the last to hold a
  // reference but media::VideoFrame is a thread-safe ref-pointer. It is
  // however read on the compositor and main thread so locking is required
  // around all modifications and all reads on the any thread.
  scoped_refptr<media::VideoFrame> current_frame_;

  // |rendering_frame_buffer_| stores the incoming frames, and provides a frame
  // selection method which returns the best frame for the render interval.
  std::unique_ptr<media::VideoRendererAlgorithm> rendering_frame_buffer_;

  // |current_frame_rendered_| is updated on compositor thread only.
  // It's used to track whether |current_frame_| was painted for detecting
  // when to increase |dropped_frame_count_|. It is also used when checking if
  // new frame for display is available in UpdateCurrentFrame().
  bool current_frame_rendered_;

  // Historical data about last rendering. These are for detecting whether
  // rendering is paused (one reason is that the tab is not in the front), in
  // which case we need to do background rendering.
  base::TimeTicks last_deadline_max_;
  base::TimeDelta last_render_length_;

  size_t total_frame_count_;
  size_t dropped_frame_count_;

  bool stopped_;
  bool render_started_;

  std::unique_ptr<blink::WebVideoFrameSubmitter> submitter_;

  std::map<base::TimeDelta, base::TimeTicks> timestamps_to_clock_times_;

  cc::UpdateSubmissionStateCB update_submission_state_callback_;

  // |current_frame_lock_| protects |current_frame_|, |rendering_frame_buffer_|,
  // |dropped_frame_count_|, and |render_started_|.
  base::Lock current_frame_lock_;

  base::WeakPtrFactory<WebMediaPlayerMSCompositor> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebMediaPlayerMSCompositor);
};
}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_STREAM_WEBMEDIAPLAYER_MS_COMPOSITOR_H_
