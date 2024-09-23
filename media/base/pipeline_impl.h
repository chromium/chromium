// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_PIPELINE_IMPL_H_
#define MEDIA_BASE_PIPELINE_IMPL_H_

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "media/base/media_export.h"
#include "media/base/pipeline.h"
#include "media/base/renderer.h"
#include "media/base/renderer_factory_selector.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace media {

class MediaLog;

// Callbacks used for Renderer creation. When the RendererType is nullopt, the
// current base one will be created.
using CreateRendererCB = base::RepeatingCallback<std::unique_ptr<Renderer>(
    std::optional<RendererType>)>;
using RendererCreatedCB = base::OnceCallback<void(std::unique_ptr<Renderer>)>;
using AsyncCreateRendererCB =
    base::RepeatingCallback<void(std::optional<RendererType>,
                                 RendererCreatedCB)>;

// Pipeline runs the media pipeline.  Filters are created and called on the
// task runner injected into this object. Pipeline works like a state
// machine to perform asynchronous initialization, pausing, seeking and playing.
//
// Here's a state diagram that describes the lifetime of this object.
//
//   [ *Created ]                       [ Any State ]
//         | Start()                         | Stop()
//         V                                 V
//   [ Starting ]                       [ Stopping ]
//         |                                 |
//         V                                 V
//   [ Playing ] <---------.            [ Stopped ]
//     |  |  | Seek()      |
//     |  |  V             |
//     |  | [ Seeking ] ---'
//     |  |                ^
//     |  | *TrackChange() |
//     |  V                |
//     | [ Switching ] ----'
//     |                   ^
//     | Suspend()         |
//     V                   |
//   [ Suspending ]        |
//     |                   |
//     V                   |
//   [ Suspended ]         |
//     | Resume()          |
//     V                   |
//   [ Resuming ] ---------'
//
// Initialization is a series of state transitions from "Created" through each
// filter initialization state.  When all filter initialization states have
// completed, we simulate a Seek() to the beginning of the media to give filters
// a chance to preroll. From then on the normal Seek() transitions are carried
// out and we start playing the media.
//
// If Stop() is ever called, this object will transition to "Stopped" state.
// Pipeline::Stop() is never called from within PipelineImpl. It's |client_|'s
// responsibility to call stop when appropriate.
//
// TODO(sandersd): It should be possible to pass through Suspended when going
// from InitDemuxer to InitRenderer, thereby eliminating the Resuming state.
// Some annoying differences between the two paths need to be removed first.
class MEDIA_EXPORT PipelineImpl : public Pipeline {
 public:
  // Constructs a pipeline that will execute media tasks on |media_task_runner|.
  // |create_renderer_cb|: to create renderers when starting and resuming.
  PipelineImpl(scoped_refptr<base::SequencedTaskRunner> media_task_runner,
               scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
               CreateRendererCB create_renderer_cb,
               MediaLog* media_log);

  PipelineImpl(const PipelineImpl&) = delete;
  PipelineImpl& operator=(const PipelineImpl&) = delete;

  ~PipelineImpl() override;

  // Pipeline implementation.
  void Start(StartType start_type,
             Demuxer* demuxer,
             Client* client,
             PipelineStatusCallback seek_cb) override;
  void Stop() override;
  void Seek(base::TimeDelta time, PipelineStatusCallback seek_cb) override;
  void Suspend(PipelineStatusCallback suspend_cb) override;
  void Resume(base::TimeDelta time, PipelineStatusCallback seek_cb) override;
  bool IsRunning() const override;
  bool IsSuspended() const override;
  double GetPlaybackRate() const override;
  void SetPlaybackRate(double playback_rate) override;
  float GetVolume() const override;
  void SetVolume(float volume) override;
  void SetLatencyHint(std::optional<base::TimeDelta> latency_hint) override;
  void SetPreservesPitch(bool preserves_pitch) override;
  void SetWasPlayedWithUserActivationAndHighMediaEngagement(
      bool was_played_with_user_activation_and_high_media_engagement) override;
  base::TimeDelta GetMediaTime() const override;
  Ranges<base::TimeDelta> GetBufferedTimeRanges() const override;
  base::TimeDelta GetMediaDuration() const override;
  bool DidLoadingProgress() override;
  PipelineStatistics GetStatistics() const override;
  void SetCdm(CdmContext* cdm_context, CdmAttachedCB cdm_attached_cb) override;

  // |enabled_track_ids| contains track ids of enabled audio tracks.
  void OnEnabledAudioTracksChanged(
      const std::vector<MediaTrack::Id>& enabled_track_ids,
      base::OnceClosure change_completed_cb) override;

  // |selected_track_id| is either empty, which means no video track is
  // selected, or contains the selected video track id.
  void OnSelectedVideoTrackChanged(
      std::optional<MediaTrack::Id> selected_track_id,
      base::OnceClosure change_completed_cb) override;

  void OnExternalVideoFrameRequest() override;

 private:
  friend class MediaLog;
  class RendererWrapper;

  // Create a Renderer asynchronously. Must be called on the main task runner
  // and the callback will be called on the main task runner as well.
  void AsyncCreateRenderer(std::optional<RendererType> renderer_type,
                           RendererCreatedCB renderer_created_cb);

  // Notifications from RendererWrapper.
  void OnError(PipelineStatus error);
  void OnFallback(PipelineStatus fallback);
  void OnEnded();
  void OnMetadata(const PipelineMetadata& metadata);
  void OnBufferingStateChange(BufferingState state,
                              BufferingStateChangeReason reason);
  void OnDurationChange(base::TimeDelta duration);
  void OnWaiting(WaitingReason reason);
  void OnAudioConfigChange(const AudioDecoderConfig& config);
  void OnVideoConfigChange(const VideoDecoderConfig& config);
  void OnVideoNaturalSizeChange(const gfx::Size& size);
  void OnVideoOpacityChange(bool opaque);
  void OnVideoAverageKeyframeDistanceUpdate();
  void OnAudioPipelineInfoChange(const AudioPipelineInfo& info);
  void OnVideoPipelineInfoChange(const VideoPipelineInfo& info);
  void OnRemotePlayStateChange(MediaStatus::State state);
  void OnVideoFrameRateChange(std::optional<int> fps);

  // Task completion callbacks from RendererWrapper.
  void OnSeekDone(bool is_suspended);
  void OnSuspendDone();

  // Parameters passed in the constructor.
  const scoped_refptr<base::SequencedTaskRunner> media_task_runner_;
  CreateRendererCB create_renderer_cb_;
  const raw_ptr<MediaLog> media_log_;

  // Pipeline client. Valid only while the pipeline is running.
  raw_ptr<Client> client_;

  // RendererWrapper instance that runs on the media thread.
  std::unique_ptr<RendererWrapper> renderer_wrapper_;

  // Temporary callback used for Start(), Seek(), and Resume().
  PipelineStatusCallback seek_cb_;

  // Temporary callback used for Suspend().
  PipelineStatusCallback suspend_cb_;

  // Current playback rate (>= 0.0). This value is set immediately via
  // SetPlaybackRate() and a task is dispatched on the task runner to notify
  // the filters.
  double playback_rate_;

  // Current volume level (from 0.0f to 1.0f). This value is set immediately
  // via SetVolume() and a task is dispatched on the task runner to notify the
  // filters.
  float volume_;

  // Current duration as reported by Demuxer.
  base::TimeDelta duration_;

  // Set by GetMediaTime(), used to prevent the current media time value as
  // reported to JavaScript from going backwards in time.
  mutable base::TimeDelta last_media_time_;

  // Set by Seek(), used in place of asking the renderer for current media time
  // while a seek is pending. Renderer's time cannot be trusted until the seek
  // has completed.
  base::TimeDelta seek_time_;

  // Cached suspension state for the RendererWrapper.
  bool is_suspended_;

  // 'external_video_frame_request_signaled_' tracks whether we've called
  // OnExternalVideoFrameRequest on the current renderer. Calls which may
  // create a new renderer in the RendererWrapper (Start, Resume, SetCdm) will
  // reset this member. There is no guarantee to the client that
  // OnExternalVideoFrameRequest will be called only once.
  bool external_video_frame_request_signaled_ = false;

  base::ThreadChecker thread_checker_;
  base::WeakPtrFactory<PipelineImpl> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_BASE_PIPELINE_IMPL_H_
