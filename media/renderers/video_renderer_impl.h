// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_RENDERERS_VIDEO_RENDERER_IMPL_H_
#define MEDIA_RENDERERS_VIDEO_RENDERER_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/cancelable_callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "media/base/decryptor.h"
#include "media/base/demuxer_stream.h"
#include "media/base/frame_rate_estimator.h"
#include "media/base/limits.h"
#include "media/base/media_log.h"
#include "media/base/pipeline_status.h"
#include "media/base/tuneable.h"
#include "media/base/video_decoder.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_frame.h"
#include "media/base/video_renderer.h"
#include "media/base/video_renderer_sink.h"
#include "media/filters/decoder_stream.h"
#include "media/filters/video_renderer_algorithm.h"
#include "media/renderers/renderer_impl_factory.h"
#include "media/video/gpu_memory_buffer_video_frame_pool.h"

namespace base {
class TickClock;
}  // namespace base

namespace media {

// VideoRendererImpl handles reading from a VideoDecoderStream storing the
// results in a queue of decoded frames and executing a callback when a frame is
// ready for rendering.
class MEDIA_EXPORT VideoRendererImpl
    : public VideoRenderer,
      public VideoRendererSink::RenderCallback {
 public:
  // |decoders| contains the VideoDecoders to use when initializing.
  //
  // Implementors should avoid doing any sort of heavy work in this method and
  // instead post a task to a common/worker thread to handle rendering.  Slowing
  // down the video thread may result in losing synchronization with audio.
  //
  // Setting |drop_frames_| to true causes the renderer to drop expired frames.
  VideoRendererImpl(
      const scoped_refptr<base::SequencedTaskRunner>& media_task_runner,
      VideoRendererSink* sink,
      const CreateVideoDecodersCB& create_video_decoders_cb,
      bool drop_frames,
      MediaLog* media_log,
      std::unique_ptr<GpuMemoryBufferVideoFramePool> gmb_pool,
      MediaPlayerLoggingID media_player_id);

  VideoRendererImpl(const VideoRendererImpl&) = delete;
  VideoRendererImpl& operator=(const VideoRendererImpl&) = delete;

  ~VideoRendererImpl() override;

  // VideoRenderer implementation.
  void Initialize(DemuxerStream* stream,
                  CdmContext* cdm_context,
                  RendererClient* client,
                  const TimeSource::WallClockTimeCB& wall_clock_time_cb,
                  PipelineStatusCallback init_cb) override;
  void Flush(base::OnceClosure callback) override;
  void StartPlayingFrom(base::TimeDelta timestamp) override;
  void OnTimeProgressing() override;
  void OnTimeStopped() override;
  void SetLatencyHint(std::optional<base::TimeDelta> latency_hint) override;

  void SetTickClockForTesting(const base::TickClock* tick_clock);
  size_t frames_queued_for_testing() const {
    return algorithm_->frames_queued();
  }
  size_t effective_frames_queued_for_testing() const {
    return algorithm_->effective_frames_queued();
  }
  int min_buffered_frames_for_testing() const { return min_buffered_frames_; }
  int max_buffered_frames_for_testing() const { return max_buffered_frames_; }

  // VideoRendererSink::RenderCallback implementation.
  scoped_refptr<VideoFrame> Render(base::TimeTicks deadline_min,
                                   base::TimeTicks deadline_max,
                                   RenderingMode rendering_mode) override;
  void OnFrameDropped() override;
  base::TimeDelta GetPreferredRenderInterval() override;

 private:
  // Callback for |video_decoder_stream_| initialization.
  void OnVideoDecoderStreamInitialized(bool success);

  void FinishInitialization(PipelineStatus status);
  void FinishFlush();

  // Functions to notify certain events to the RendererClient.
  void OnPlaybackError(PipelineStatus error);
  void OnPlaybackEnded();
  void OnStatisticsUpdate(const PipelineStatistics& stats);
  void OnBufferingStateChange(BufferingState state);
  void OnWaiting(WaitingReason reason);

  // Called by the VideoDecoderStream when a config change occurs. Will notify
  // RenderClient of the new config.
  void OnConfigChange(const VideoDecoderConfig& config);

  // Called when the decoder stream and selector have a fallback after failed
  // decode.
  void OnFallback(PipelineStatus status);

  // Callback for |video_decoder_stream_| to deliver decoded video frames and
  // report video decoding status.
  void FrameReady(VideoDecoderStream::ReadResult result);

  // Helper method for enqueueing a frame to |alogorithm_|.
  void AddReadyFrame_Locked(scoped_refptr<VideoFrame> frame);

  // Helper method that schedules an asynchronous read from the
  // |video_decoder_stream_| as long as there isn't a pending read and we have
  // capacity.
  void AttemptRead_Locked();

  // Called when VideoDecoderStream::Reset() completes.
  void OnVideoDecoderStreamResetDone();

  // Returns true if the renderer has enough data for playback purposes.
  // Note that having enough data may be due to reaching end of stream.
  bool HaveEnoughData_Locked() const;
  void TransitionToHaveEnough_Locked();
  void TransitionToHaveNothing();
  void TransitionToHaveNothing_Locked();

  // Runs |statistics_cb_| with |frames_decoded_| and |frames_dropped_|, resets
  // them to 0. If |force_update| is true, sends an update even if no frames
  // have been decoded since the last update.
  void UpdateStats_Locked(bool force_update = false);

  // Notifies |client_| if the current frame rate has changed since it was last
  // reported, and tracks what the most recently reported frame rate was.
  void ReportFrameRateIfNeeded_Locked();

  // Update |min_buffered_frames_| and |max_buffered_frames_| using the latest
  // |average_frame_duration|. Should only be called when |latency_hint_| > 0.
  void UpdateLatencyHintBufferingCaps_Locked(
      base::TimeDelta average_frame_duration);

  // Returns true if algorithm_->effective_frames_queued() >= |buffering_cap|,
  // or when the number of ineffective frames >= kAbsoluteMaxFrames.
  bool HaveReachedBufferingCap(size_t buffering_cap) const;

  // Starts or stops |sink_| respectively. Do not call while |lock_| is held.
  void StartSink();
  void StopSink();

  // Fires |ended_cb_| if there are no remaining usable frames and
  // |received_end_of_stream_| is true.  Sets |rendered_end_of_stream_| if it
  // does so.
  //
  // When called from the media thread, |time_progressing| should reflect the
  // value of |time_progressing_|.  When called from Render() on the sink
  // callback thread, |time_progressing| must be true since Render() could not
  // have been called otherwise.
  void MaybeFireEndedCallback_Locked(bool time_progressing);

  // Helper method for converting a single media timestamp to wall clock time.
  base::TimeTicks ConvertMediaTimestamp(base::TimeDelta media_timestamp);

  base::TimeTicks GetCurrentMediaTimeAsWallClockTime();

  // Helper method for checking if a frame timestamp plus the frame's expected
  // duration is before |start_timestamp_|.
  bool IsBeforeStartTime(const VideoFrame& frame);

  // Helper method for checking if we have the best possible first frame to
  // paint in the queue.
  bool HasBestFirstFrame(const VideoFrame& frame);

  // Attempts to remove frames which are no longer effective for rendering when
  // |buffering_state_| == BUFFERING_HAVE_NOTHING or |was_background_rendering_|
  // is true.  If the current media time as provided by |wall_clock_time_cb_| is
  // null, no frame expiration will be done.
  //
  // When background rendering the method will expire all frames before the
  // current wall clock time since it's expected that there will be long delays
  // between each Render() call in this state.
  //
  // When in the underflow state the method will first attempt to remove expired
  // frames before the current media time plus duration. If |sink_started_| is
  // true, nothing more can be done. However, if false, and there are still no
  // effective frames in the queue, the entire frame queue will be released to
  // avoid any stalling.
  void RemoveFramesForUnderflowOrBackgroundRendering();

  // Notifies |client_| in the event of frame size or opacity changes. Must be
  // called on |task_runner_|.
  void CheckForMetadataChanges(VideoPixelFormat pixel_format,
                               const gfx::Size& natural_size);

  // Both calls AttemptRead_Locked() and CheckForMetadataChanges(). Must be
  // called on |task_runner_|.
  void AttemptReadAndCheckForMetadataChanges(VideoPixelFormat pixel_format,
                                             const gfx::Size& natural_size);

  // Paints first frame and sets `painted_first_frame_` to true if
  // `painted_first_frame_` is false.
  void PaintFirstFrame();
  void PaintFirstFrame_Locked();

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // Sink which calls into VideoRendererImpl via Render() for video frames.  Do
  // not call any methods on the sink while |lock_| is held or the two threads
  // might deadlock. Do not call Start() or Stop() on the sink directly, use
  // StartSink() and StopSink() to ensure background rendering is started.  Only
  // access these values on |task_runner_|.
  const raw_ptr<VideoRendererSink, AcrossTasksDanglingUntriaged> sink_;
  bool sink_started_;

  // Stores the last decoder config that was passed to
  // RendererClient::OnVideoConfigChange. Used to prevent signaling config
  // to the upper layers when when the new config is the same.
  VideoDecoderConfig current_decoder_config_;

  // Used for accessing data members.
  base::Lock lock_;

  raw_ptr<RendererClient> client_;

  // Pool of GpuMemoryBuffers and resources used to create hardware frames.
  // Ensure this is destructed after |algorithm_| for optimal memory release
  // when a frames are still held by the compositor. Must be destructed after
  // |video_decoder_stream_| since it holds a callback to the pool.
  std::unique_ptr<GpuMemoryBufferVideoFramePool> gpu_memory_buffer_pool_;

  // Provides video frames to VideoRendererImpl.
  std::unique_ptr<VideoDecoderStream> video_decoder_stream_;

  // Passed in during Initialize().
  raw_ptr<DemuxerStream> demuxer_stream_;

  // This dangling raw_ptr occurred in:
  // webkit_unit_tests: WebMediaPlayerImplTest.MediaPositionState_Playing
  // https://ci.chromium.org/ui/p/chromium/builders/try/linux-rel/1425143/test-results?q=ExactID%3Aninja%3A%2F%2Fthird_party%2Fblink%2Frenderer%2Fcontroller%3Ablink_unittests%2FWebMediaPlayerImplTest.MediaPositionState_Playing+VHash%3A896f1103f2d1008d&sortby=&groupby=
  raw_ptr<MediaLog, FlakyDanglingUntriaged> media_log_;

  MediaPlayerLoggingID player_id_;

  // Flag indicating low-delay mode.
  bool low_delay_;

  // Keeps track of whether we received the end of stream buffer and finished
  // rendering.
  bool received_end_of_stream_;
  bool rendered_end_of_stream_;

  // Important detail: being in kPlaying doesn't imply that video is being
  // rendered. Rather, it means that the renderer is ready to go. The actual
  // rendering of video is controlled by time advancing via |get_time_cb_|.
  // Video renderer can be reinitialized completely by calling Initialize again
  // when it is in a kFlushed state with video sink stopped.
  //
  //    kUninitialized
  //  +------> | Initialize()
  //  |        |
  //  |        V
  //  |   kInitializing
  //  |        | Decoders initialized
  //  |        |
  //  |        V            Decoders reset
  //  ---- kFlushed <------------------ kFlushing
  //           | StartPlayingFrom()         ^
  //           |                            |
  //           |                            | Flush()
  //           `---------> kPlaying --------'
  enum State { kUninitialized, kInitializing, kFlushing, kFlushed, kPlaying };
  State state_;

  // TODO(servolk): Consider using DecoderFactory here instead of the
  // CreateVideoDecodersCB.
  CreateVideoDecodersCB create_video_decoders_cb_;

  // Keep track of the outstanding read on the VideoDecoderStream. Flushing can
  // only complete once the read has completed.
  bool pending_read_;

  bool drop_frames_;

  BufferingState buffering_state_;

  // Playback operation callbacks.
  PipelineStatusCallback init_cb_;
  base::OnceClosure flush_cb_;
  TimeSource::WallClockTimeCB wall_clock_time_cb_;

  base::TimeDelta start_timestamp_;

  // Keeps track of the number of frames decoded and dropped since the
  // last call to |statistics_cb_|. These must be accessed under lock.
  PipelineStatistics stats_;

  raw_ptr<const base::TickClock> tick_clock_;

  // Algorithm for selecting which frame to render; manages frames and all
  // timing related information. Ensure this is destructed before
  // |gpu_memory_buffer_pool_| for optimal memory release when a frames are
  // still held by the compositor.
  std::unique_ptr<VideoRendererAlgorithm> algorithm_;

  // Indicates that Render() was called with |background_rendering| set to true,
  // so we've entered a background rendering mode where dropped frames are not
  // counted.  Must be accessed under |lock_| once |sink_| is started.
  bool was_background_rendering_;

  // Indicates whether or not media time is currently progressing or not.  Must
  // only be accessed from |task_runner_|.
  bool time_progressing_;

  // Indicates if a frame has been processed by CheckForMetadataChanges().
  bool have_renderered_frames_;

  // Tracks last frame properties to detect and notify client of any changes.
  gfx::Size last_frame_natural_size_;
  bool last_frame_opaque_;

  // The last value from |video_decoder_stream_->AverageDuration()|.
  base::TimeDelta last_decoder_stream_avg_duration_;

  // Indicates if we've painted the first valid frame after StartPlayingFrom().
  bool painted_first_frame_;

  // Used to paint the first frame if we don't receive the best one in time and
  // aren't guaranteed to receive anymore.
  base::CancelableOnceClosure paint_first_frame_cb_;

  // The initial value for |min_buffered_frames_| and |max_buffered_frames_|.
  Tuneable<size_t> initial_buffering_size_ = {
      "MediaInitialBufferingSizeForHaveEnough", 3, limits::kMaxVideoFrames, 10};

  // The number of frames required to transition from BUFFERING_HAVE_NOTHING to
  // BUFFERING_HAVE_ENOUGH.
  size_t min_buffered_frames_;

  // The maximum number of frames to buffer. Always >= |min_buffered_frames_|.
  // May be greater-than when |latency_hint_| set that decreases the minimum
  // buffering limit.
  size_t max_buffered_frames_;

  // Last Render() and last FrameReady() times respectively. Used to avoid
  // triggering underflow when background rendering.
  base::TimeTicks last_render_time_;
  base::TimeTicks last_frame_ready_time_;

  // Running average of frame durations.
  FrameRateEstimator fps_estimator_;

  // Last FPS, if any, reported to the client.
  std::optional<int> last_reported_fps_;

  // Value saved from last call to SetLatencyHint(). Used to recompute buffering
  // limits as framerate fluctuates.
  std::optional<base::TimeDelta> latency_hint_;

  // When latency_hint_ > 0, we make regular adjustments to buffering caps as
  // |algorithm_->average_frame_duration()| fluctuates, but we only want to emit
  // one MEDIA_LOG.
  bool is_latency_hint_media_logged_ = false;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<VideoRendererImpl> weak_factory_{this};

  // Weak factory used to invalidate certain queued callbacks on reset().
  // This is useful when doing video frame copies asynchronously since we
  // want to discard video frames that might be received after the stream has
  // been reset.
  base::WeakPtrFactory<VideoRendererImpl> cancel_on_flush_weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_RENDERERS_VIDEO_RENDERER_IMPL_H_
