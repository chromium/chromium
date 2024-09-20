// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_RENDERERS_RENDERER_IMPL_H_
#define MEDIA_RENDERERS_RENDERER_IMPL_H_

#include <memory>
#include <vector>

#include "base/cancelable_callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/clock.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/buffering_state.h"
#include "media/base/decryptor.h"
#include "media/base/demuxer_stream.h"
#include "media/base/media_export.h"
#include "media/base/pipeline_status.h"
#include "media/base/renderer.h"
#include "media/base/tuneable.h"
#include "media/base/video_decoder_config.h"
#include "media/base/waiting.h"
#include "ui/gfx/geometry/size.h"

namespace media {

class AudioRenderer;
class MediaResource;
class TimeSource;
class VideoRenderer;
class WallClockTimeSource;

class MEDIA_EXPORT RendererImpl final : public Renderer {
 public:
  // Renders audio/video streams using |audio_renderer| and |video_renderer|
  // provided. All methods except for GetMediaTime() run on the |task_runner|.
  // GetMediaTime() runs on the render main thread because it's part of JS sync
  // API.
  RendererImpl(const scoped_refptr<base::SequencedTaskRunner>& task_runner,
               std::unique_ptr<AudioRenderer> audio_renderer,
               std::unique_ptr<VideoRenderer> video_renderer);

  RendererImpl(const RendererImpl&) = delete;
  RendererImpl& operator=(const RendererImpl&) = delete;

  ~RendererImpl() final;

  // Renderer implementation.
  void Initialize(MediaResource* media_resource,
                  RendererClient* client,
                  PipelineStatusCallback init_cb) final;
  void SetCdm(CdmContext* cdm_context, CdmAttachedCB cdm_attached_cb) final;
  void SetLatencyHint(std::optional<base::TimeDelta> latency_hint) final;
  void SetPreservesPitch(bool preserves_pitch) final;
  void SetWasPlayedWithUserActivationAndHighMediaEngagement(
      bool was_played_with_user_activation_and_high_media_engagement) final;
  void Flush(base::OnceClosure flush_cb) final;
  void StartPlayingFrom(base::TimeDelta time) final;
  void SetPlaybackRate(double playback_rate) final;
  void SetVolume(float volume) final;
  base::TimeDelta GetMediaTime() final;
  void OnSelectedVideoTracksChanged(
      const std::vector<DemuxerStream*>& enabled_tracks,
      base::OnceClosure change_completed_cb) final;
  void OnEnabledAudioTracksChanged(
      const std::vector<DemuxerStream*>& enabled_tracks,
      base::OnceClosure change_completed_cb) final;
  RendererType GetRendererType() final;

  // Helper functions for testing purposes. Must be called before Initialize().
  void DisableUnderflowForTesting();
  void EnableClocklessVideoPlaybackForTesting();
  void set_time_source_for_testing(TimeSource* time_source) {
    time_source_ = time_source;
  }
  void set_video_underflow_threshold_for_testing(base::TimeDelta threshold) {
    video_underflow_threshold_.set_for_testing(threshold);
  }

 private:
  class RendererClientInternal;

  enum State {
    STATE_UNINITIALIZED,
    STATE_INIT_PENDING_CDM,  // Initialization is waiting for the CDM to be set.
    STATE_INITIALIZING,      // Initializing audio/video renderers.
    STATE_FLUSHING,          // Flushing is in progress.
    STATE_FLUSHED,           // After initialization or after flush completed.
    STATE_PLAYING,           // After StartPlayingFrom has been called.
    STATE_ERROR
  };

  bool GetWallClockTimes(const std::vector<base::TimeDelta>& media_timestamps,
                         std::vector<base::TimeTicks>* wall_clock_times);

  bool HasEncryptedStream();

  void FinishInitialization(PipelineStatus status);
  void FinishFlush();

  // Helper functions and callbacks for Initialize().
  void InitializeAudioRenderer();
  void OnAudioRendererInitializeDone(PipelineStatus status);
  void InitializeVideoRenderer();
  void OnVideoRendererInitializeDone(PipelineStatus status);

  // Helper functions and callbacks for Flush().
  void FlushInternal();
  void FlushAudioRenderer();
  void OnAudioRendererFlushDone();
  void FlushVideoRenderer();
  void OnVideoRendererFlushDone();

  // Reinitialize audio/video renderer during a demuxer stream switching. The
  // renderer must be flushed first, and when the re-init is completed the
  // corresponding callback will be invoked to restart playback.
  // The |stream| parameter specifies the new demuxer stream, and the |time|
  // parameter specifies the time on media timeline where the switch occurred.
  void ReinitializeAudioRenderer(DemuxerStream* stream,
                                 base::TimeDelta time,
                                 base::OnceClosure reinitialize_completed_cb);
  void OnAudioRendererReinitialized(DemuxerStream* stream,
                                    base::TimeDelta time,
                                    base::OnceClosure reinitialize_completed_cb,
                                    PipelineStatus status);
  void ReinitializeVideoRenderer(DemuxerStream* stream,
                                 base::TimeDelta time,
                                 base::OnceClosure restart_completed_cb);
  void OnVideoRendererReinitialized(DemuxerStream* stream,
                                    base::TimeDelta time,
                                    base::OnceClosure restart_completed_cb,
                                    PipelineStatus status);

  // Restart audio/video renderer playback after a demuxer stream switch or
  // after a demuxer stream has been disabled and re-enabled. The |stream|
  // parameter specifies which stream needs to be restarted. The |time|
  // parameter specifies the position on the media timeline where the playback
  // needs to be restarted. It is necessary for demuxers with independent
  // streams (e.g. MSE / ChunkDemuxer) to synchronize data reading between those
  // streams.
  void RestartAudioRenderer(DemuxerStream* stream,
                            base::TimeDelta time,
                            base::OnceClosure restart_completed_cb);
  void RestartVideoRenderer(DemuxerStream* stream,
                            base::TimeDelta time,
                            base::OnceClosure restart_completed_cb);

  // Fix state booleans after the stream switching is finished.
  void CleanUpTrackChange(base::OnceClosure on_finished,
                          bool* ended,
                          bool* playing);

  // Callback executed by filters to update statistics.
  void OnStatisticsUpdate(const PipelineStatistics& stats);

  // Collection of callback methods and helpers for tracking changes in
  // buffering state and transition from paused/underflow states and playing
  // states.
  //
  // While in the kPlaying state:
  //   - A waiting to non-waiting transition indicates preroll has completed
  //     and StartPlayback() should be called
  //   - A non-waiting to waiting transition indicates underflow has occurred
  //     and PausePlayback() should be called
  void OnBufferingStateChange(DemuxerStream::Type type,
                              BufferingState new_buffering_state,
                              BufferingStateChangeReason reason);

  // Handles the buffering notifications that we might get while an audio or a
  // video stream is being restarted. In those cases we don't want to report
  // underflows immediately and instead give decoders a chance to catch up with
  // currently playing stream. Returns true if the buffering nofication has been
  // handled and no further processing is necessary, returns false to indicate
  // that we should fall back to the regular OnBufferingStateChange logic.
  bool HandleRestartedStreamBufferingChanges(
      DemuxerStream::Type type,
      BufferingState new_buffering_state);

  bool WaitingForEnoughData() const;
  void PausePlayback();
  void StartPlayback();

  // Callbacks executed when a renderer has ended.
  void OnRendererEnded(DemuxerStream::Type type);
  bool PlaybackHasEnded() const;
  void RunEndedCallbackIfNeeded();

  // Callback executed when a runtime error happens.
  void OnError(PipelineStatus error);

  // Callback executed when there is a fallback somewhere in the pipeline which
  // should be recorded for metrics analysis.
  void OnFallback(PipelineStatus fallback);

  void OnWaiting(WaitingReason reason);
  void OnVideoNaturalSizeChange(const gfx::Size& size);
  void OnAudioConfigChange(const AudioDecoderConfig& config);
  void OnVideoConfigChange(const VideoDecoderConfig& config);
  void OnVideoOpacityChange(bool opaque);
  void OnVideoFrameRateChange(std::optional<int> fps);

  void OnStreamRestartCompleted();

  State state_;

  // Task runner used to execute pipeline tasks.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  raw_ptr<MediaResource> media_resource_;
  raw_ptr<RendererClient> client_;

  // Temporary callback used for Initialize() and Flush().
  PipelineStatusCallback init_cb_;
  base::OnceClosure flush_cb_;

  std::unique_ptr<RendererClientInternal> audio_renderer_client_;
  std::unique_ptr<RendererClientInternal> video_renderer_client_;
  std::unique_ptr<AudioRenderer> audio_renderer_;
  std::unique_ptr<VideoRenderer> video_renderer_;

  raw_ptr<DemuxerStream> current_audio_stream_;
  raw_ptr<DemuxerStream> current_video_stream_;

  // Renderer-provided time source used to control playback.
  raw_ptr<TimeSource, DanglingUntriaged> time_source_;
  std::unique_ptr<WallClockTimeSource> wall_clock_time_source_;
  bool time_ticking_;
  double playback_rate_;

  // The time to start playback from after starting/seeking has completed.
  base::TimeDelta start_time_;

  BufferingState audio_buffering_state_;
  BufferingState video_buffering_state_;

  // Whether we've received the audio/video ended events.
  bool audio_ended_;
  bool video_ended_;
  bool audio_playing_;
  bool video_playing_;

  raw_ptr<CdmContext> cdm_context_;

  bool underflow_disabled_for_testing_;
  bool clockless_video_playback_enabled_for_testing_;

  // Used to defer underflow for video when audio is present.
  base::CancelableOnceClosure deferred_video_underflow_cb_;

  // We cannot use `!deferred_video_underflow_cb_.IsCancelled()` as that changes
  // when the callback is run, even if not explicitly cancelled.
  bool has_deferred_buffering_state_change_ = false;

  // The amount of time to wait before declaring underflow if the video renderer
  // runs out of data but the audio renderer still has enough.
  Tuneable<base::TimeDelta> video_underflow_threshold_ = {
      "MediaVideoUnderflowThreshold", base::Milliseconds(1000),
      base::Milliseconds(3000), base::Milliseconds(8000)};

  // Lock used to protect access to the |restarting_audio_| flag and
  // |restarting_audio_time_|.
  // TODO(servolk): Get rid of the lock and replace restarting_audio_ with
  // std::atomic<bool> when atomics are unbanned in Chromium.
  base::Lock restarting_audio_lock_;
  bool pending_audio_track_change_ = false;
  base::TimeDelta restarting_audio_time_ = kNoTimestamp;

  bool pending_video_track_change_ = false;

  base::WeakPtr<RendererImpl> weak_this_;
  base::WeakPtrFactory<RendererImpl> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_RENDERERS_RENDERER_IMPL_H_
