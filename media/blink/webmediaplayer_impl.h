// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BLINK_WEBMEDIAPLAYER_IMPL_H_
#define MEDIA_BLINK_WEBMEDIAPLAYER_IMPL_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/cancelable_callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/threading/thread.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "cc/layers/surface_layer.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "media/base/cdm_config.h"
#include "media/base/encryption_scheme.h"
#include "media/base/media_observer.h"
#include "media/base/media_tracks.h"
#include "media/base/overlay_info.h"
#include "media/base/pipeline_impl.h"
#include "media/base/renderer_factory_selector.h"
#include "media/base/simple_watch_timer.h"
#include "media/base/text_track.h"
#include "media/blink/buffered_data_source_host_impl.h"
#include "media/blink/learning_experiment_helper.h"
#include "media/blink/media_blink_export.h"
#include "media/blink/multibuffer_data_source.h"
#include "media/blink/power_status_helper.h"
#include "media/blink/smoothness_helper.h"
#include "media/blink/video_frame_compositor.h"
#include "media/blink/webmediaplayer_params.h"
#include "media/filters/pipeline_controller.h"
#include "media/learning/common/media_learning_tasks.h"
#include "media/mojo/mojom/playback_events_recorder.mojom.h"
#include "media/renderers/paint_canvas_video_renderer.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/media_session/public/cpp/media_position.h"
#include "third_party/blink/public/platform/media/webmediaplayer_delegate.h"
#include "third_party/blink/public/platform/web_audio_source_provider.h"
#include "third_party/blink/public/platform/web_content_decryption_module_result.h"
#include "third_party/blink/public/platform/web_media_player.h"
#include "third_party/blink/public/platform/web_surface_layer_bridge.h"
#include "third_party/blink/public/web/modules/media/webmediaplayer_util.h"
#include "url/gurl.h"

namespace blink {
class WebAudioSourceProviderImpl;
class WebLocalFrame;
class WebMediaPlayerClient;
class WebMediaPlayerEncryptedMediaClient;
}  // namespace blink

namespace base {
class SingleThreadTaskRunner;
class TaskRunner;
}  // namespace base

namespace cc {
class VideoLayer;
}

namespace gpu {
namespace gles2 {
class GLES2Interface;
}
}  // namespace gpu

namespace media {
class CdmContextRef;
class ChunkDemuxer;
class VideoDecodeStatsReporter;
class MediaLog;
class MemoryDumpProviderProxy;
class UrlIndex;
class VideoFrameCompositor;
class WatchTimeReporter;

// The canonical implementation of blink::WebMediaPlayer that's backed by
// Pipeline. Handles normal resource loading, Media Source, and
// Encrypted Media.
class MEDIA_BLINK_EXPORT WebMediaPlayerImpl
    : public blink::WebMediaPlayer,
      public blink::WebMediaPlayerDelegate::Observer,
      public Pipeline::Client,
      public MediaObserverClient,
      public blink::WebSurfaceLayerBridgeObserver,
      public SmoothnessHelper::Client {
 public:
  // Constructs a WebMediaPlayer implementation using Chromium's media stack.
  // |delegate| and |renderer_factory_selector| must not be null.
  WebMediaPlayerImpl(
      blink::WebLocalFrame* frame,
      blink::WebMediaPlayerClient* client,
      blink::WebMediaPlayerEncryptedMediaClient* encrypted_client,
      blink::WebMediaPlayerDelegate* delegate,
      std::unique_ptr<RendererFactorySelector> renderer_factory_selector,
      UrlIndex* url_index,
      std::unique_ptr<VideoFrameCompositor> compositor,
      std::unique_ptr<WebMediaPlayerParams> params);
  ~WebMediaPlayerImpl() override;

  // WebSurfaceLayerBridgeObserver implementation.
  void OnWebLayerUpdated() override;
  void RegisterContentsLayer(cc::Layer* layer) override;
  void UnregisterContentsLayer(cc::Layer* layer) override;
  void OnSurfaceIdUpdated(viz::SurfaceId surface_id) override;

  WebMediaPlayer::LoadTiming Load(LoadType load_type,
                                  const blink::WebMediaPlayerSource& source,
                                  CorsMode cors_mode) override;

  // Playback controls.
  void Play() override;
  void Pause() override;
  void Seek(double seconds) override;
  void SetRate(double rate) override;
  void SetVolume(double volume) override;
  void SetLatencyHint(double seconds) override;
  void SetPreservesPitch(bool preserves_pitch) override;
  void OnRequestPictureInPicture() override;
  void OnTimeUpdate() override;
  void SetSinkId(
      const blink::WebString& sink_id,
      blink::WebSetSinkIdCompleteCallback completion_callback) override;
  void SetPoster(const blink::WebURL& poster) override;
  void SetPreload(blink::WebMediaPlayer::Preload preload) override;
  blink::WebTimeRanges Buffered() const override;
  blink::WebTimeRanges Seekable() const override;

  // paint() the current video frame into |canvas|. This is used to support
  // various APIs and functionalities, including but not limited to: <canvas>,
  // WebGL texImage2D, ImageBitmap, printing and capturing capabilities.
  void Paint(cc::PaintCanvas* canvas,
             const blink::WebRect& rect,
             cc::PaintFlags& flags,
             int already_uploaded_id,
             VideoFrameUploadMetadata* out_metadata) override;

  // True if the loaded media has a playable video/audio track.
  bool HasVideo() const override;
  bool HasAudio() const override;

  void EnabledAudioTracksChanged(
      const blink::WebVector<blink::WebMediaPlayer::TrackId>& enabledTrackIds)
      override;
  void SelectedVideoTrackChanged(
      blink::WebMediaPlayer::TrackId* selectedTrackId) override;

  // Dimensions of the video.
  gfx::Size NaturalSize() const override;

  gfx::Size VisibleSize() const override;

  // Getters of playback state.
  bool Paused() const override;
  bool Seeking() const override;
  double Duration() const override;
  virtual double timelineOffset() const;
  double CurrentTime() const override;
  bool IsEnded() const override;

  bool PausedWhenHidden() const override;

  // Informed when picture-in-picture availability changed.
  void OnPictureInPictureAvailabilityChanged(bool available) override;

  // Internal states of loading and network.
  // TODO(hclam): Ask the pipeline about the state rather than having reading
  // them from members which would cause race conditions.
  blink::WebMediaPlayer::NetworkState GetNetworkState() const override;
  blink::WebMediaPlayer::ReadyState GetReadyState() const override;

  blink::WebMediaPlayer::SurfaceLayerMode GetVideoSurfaceLayerMode()
      const override;

  blink::WebString GetErrorMessage() const override;
  bool DidLoadingProgress() override;
  bool WouldTaintOrigin() const override;

  double MediaTimeForTimeValue(double timeValue) const override;

  unsigned DecodedFrameCount() const override;
  unsigned DroppedFrameCount() const override;
  uint64_t AudioDecodedByteCount() const override;
  uint64_t VideoDecodedByteCount() const override;

  bool HasAvailableVideoFrame() const override;

  bool CopyVideoTextureToPlatformTexture(
      gpu::gles2::GLES2Interface* gl,
      unsigned int target,
      unsigned int texture,
      unsigned internal_format,
      unsigned format,
      unsigned type,
      int level,
      bool premultiply_alpha,
      bool flip_y,
      int already_uploaded_id,
      VideoFrameUploadMetadata* out_metadata) override;

  bool PrepareVideoFrameForWebGL(
      gpu::gles2::GLES2Interface* gl,
      unsigned target,
      unsigned texture,
      int already_uploaded_id,
      WebMediaPlayer::VideoFrameUploadMetadata* out_metadata) override;

  static void ComputeFrameUploadMetadata(
      VideoFrame* frame,
      int already_uploaded_id,
      VideoFrameUploadMetadata* out_metadata);

  scoped_refptr<blink::WebAudioSourceProviderImpl> GetAudioSourceProvider()
      override;

  void SetContentDecryptionModule(
      blink::WebContentDecryptionModule* cdm,
      blink::WebContentDecryptionModuleResult result) override;

  void EnteredFullscreen() override;
  void ExitedFullscreen() override;
  void BecameDominantVisibleContent(bool is_dominant) override;
  void SetIsEffectivelyFullscreen(
      blink::WebFullscreenVideoStatus fullscreen_video_status) override;
  void OnHasNativeControlsChanged(bool) override;
  void OnDisplayTypeChanged(WebMediaPlayer::DisplayType display_type) override;

  // blink::WebMediaPlayerDelegate::Observer implementation.
  void OnFrameHidden() override;
  void OnFrameClosed() override;
  void OnFrameShown() override;
  void OnIdleTimeout() override;
  void OnPlay() override;
  void OnPause() override;
  void OnMuted(bool muted) override;
  void OnSeekForward(double seconds) override;
  void OnSeekBackward(double seconds) override;
  void OnEnterPictureInPicture() override;
  void OnExitPictureInPicture() override;
  void OnSetAudioSink(const std::string& sink_id) override;
  void OnVolumeMultiplierUpdate(double multiplier) override;
  void OnBecamePersistentVideo(bool value) override;
  void OnPowerExperimentState(bool state) override;
  void RequestRemotePlaybackDisabled(bool disabled) override;

#if defined(OS_ANDROID)
  // TODO(https://crbug.com/839651): Rename Flinging[Started/Stopped] to
  // RemotePlayback[Started/Stopped] once the other RemotePlayback methods have
  // been removed
  void FlingingStarted() override;
  void FlingingStopped() override;

  // Called when the play/pause state of media playing on a remote cast device
  // changes, and WMPI wasn't the originator of that change (e.g. a phone on the
  // same network paused the cast device via the casting notification).
  // This is only used by the FlingingRenderer/FlingingRendererClient, when we
  // are flinging media (a.k.a. RemotePlayback).
  // The consistency between the WMPI state and the cast device state is not
  // guaranteed, and it a best effort, which can always be fixed by the user by
  // tapping play/pause once. Attempts to enfore stronger consistency guarantees
  // have lead to unstable states, and a worse user experience.
  void OnRemotePlayStateChange(MediaStatus::State state);
#endif

  // MediaObserverClient implementation.
  void SwitchToRemoteRenderer(
      const std::string& remote_device_friendly_name) override;
  void SwitchToLocalRenderer(
      MediaObserverClient::ReasonToSwitchToLocal reason) override;
  void UpdateRemotePlaybackCompatibility(bool is_compatible) override;

  // Test helper methods for exercising media suspension. Once called, when
  // |target_state| is reached or exceeded the stale flag will be set when
  // computing the play state, which will trigger suspend if the player is
  // paused; see UpdatePlayState_ComputePlayState() for the exact details.
  void ForceStaleStateForTesting(ReadyState target_state) override;
  bool IsSuspendedForTesting() override;
  bool DidLazyLoad() const override;
  void OnBecameVisible() override;
  bool IsOpaque() const override;
  int GetDelegateId() override;
  base::Optional<viz::SurfaceId> GetSurfaceId() override;
  GURL GetSrcAfterRedirects() override;
  void RequestVideoFrameCallback() override;
  std::unique_ptr<blink::WebMediaPlayer::VideoFramePresentationMetadata>
  GetVideoFramePresentationMetadata() override;
  void UpdateFrameIfStale() override;

  base::WeakPtr<blink::WebMediaPlayer> AsWeakPtr() override;

  bool IsBackgroundMediaSuspendEnabled() const {
    return is_background_suspend_enabled_;
  }

  // Distinct states that |delegate_| can be in. (Public for testing.)
  enum class DelegateState {
    GONE,
    PLAYING,
    PAUSED,
  };

  // Playback state variables computed together in UpdatePlayState().
  // (Public for testing.)
  struct PlayState {
    DelegateState delegate_state;
    bool is_idle;
    bool is_memory_reporting_enabled;
    bool is_suspended;
  };

  // Allow background video tracks with ~5 second keyframes (rounding down) to
  // be disabled to save resources.
  enum { kMaxKeyframeDistanceToDisableBackgroundVideoMs = 5500 };

 private:
  friend class WebMediaPlayerImplTest;
  friend class WebMediaPlayerImplBackgroundBehaviorTest;

  void EnableOverlay();
  void DisableOverlay();

  // Do we have overlay information?  For CVV, this is a surface id.  For
  // AndroidOverlay, this is the routing token.
  bool HaveOverlayInfo();

  // Send OverlayInfo to the decoder.
  //
  // If we've requested but not yet received the surface id or routing token, or
  // if there's no decoder-provided callback to send the overlay info, then this
  // call will do nothing.
  void MaybeSendOverlayInfoToDecoder();

  void OnPipelineSuspended();
  void OnBeforePipelineResume();
  void OnPipelineResumed();
  void OnPipelineSeeked(bool time_updated);
  void OnDemuxerOpened();

  // Pipeline::Client overrides.
  void OnError(PipelineStatus status) override;
  void OnEnded() override;
  void OnMetadata(const PipelineMetadata& metadata) override;
  void OnBufferingStateChange(BufferingState state,
                              BufferingStateChangeReason reason) override;
  void OnDurationChange() override;
  void OnAddTextTrack(const TextTrackConfig& config,
                      AddTextTrackDoneCB done_cb) override;
  void OnWaiting(WaitingReason reason) override;
  void OnAudioConfigChange(const AudioDecoderConfig& config) override;
  void OnVideoConfigChange(const VideoDecoderConfig& config) override;
  void OnVideoNaturalSizeChange(const gfx::Size& size) override;
  void OnVideoOpacityChange(bool opaque) override;
  void OnVideoFrameRateChange(base::Optional<int> fps) override;
  void OnVideoAverageKeyframeDistanceUpdate() override;
  void OnAudioDecoderChange(const PipelineDecoderInfo& info) override;
  void OnVideoDecoderChange(const PipelineDecoderInfo& info) override;

  // Simplified watch time reporting.
  void OnSimpleWatchTimerTick();

  // Actually seek. Avoids causing |should_notify_time_changed_| to be set when
  // |time_updated| is false.
  void DoSeek(base::TimeDelta time, bool time_updated);

  // Called after |defer_load_cb_| has decided to allow the load. If
  // |defer_load_cb_| is null this is called immediately.
  void DoLoad(LoadType load_type, const blink::WebURL& url, CorsMode cors_mode);

  // Called after asynchronous initialization of a data source completed.
  void DataSourceInitialized(bool success);

  // Called if the |MultiBufferDataSource| is redirected.
  void OnDataSourceRedirected();

  // Called when the data source is downloading or paused.
  void NotifyDownloading(bool is_downloading);

  // Called by RenderFrameImpl with the overlay routing token, if we request it.
  void OnOverlayRoutingToken(const base::UnguessableToken& token);

  // Called by GpuVideoDecoder on Android to request a surface to render to (if
  // necessary).
  void OnOverlayInfoRequested(bool decoder_requires_restart_for_overlay,
                              ProvideOverlayInfoCB provide_overlay_info_cb);

  // Creates a Renderer via the |renderer_factory_selector_|. If the
  // |factory_type| is base::nullopt, create the base Renderer. Otherwise, set
  // the base type to be |factory_type| and create a Renderer of that type.
  std::unique_ptr<Renderer> CreateRenderer(
      base::Optional<RendererFactoryType> factory_type);

  // Finishes starting the pipeline due to a call to load().
  void StartPipeline();

  // Restart the player/pipeline as soon as possible. This will destroy the
  // current renderer, if any, and create a new one via the RendererFactory; and
  // then seek to resume playback at the current position.
  void ScheduleRestart();

  // Helpers that set the network/ready state and notifies the client if
  // they've changed.
  void SetNetworkState(blink::WebMediaPlayer::NetworkState state);
  void SetReadyState(blink::WebMediaPlayer::ReadyState state);

  // Returns the current video frame from |compositor_|, and asks the compositor
  // to update its frame if it is stale.
  // Can return a nullptr.
  scoped_refptr<VideoFrame> GetCurrentFrameFromCompositor() const;

  // Called when the demuxer encounters encrypted streams.
  void OnEncryptedMediaInitData(EmeInitDataType init_data_type,
                                const std::vector<uint8_t>& init_data);

  // Called when the FFmpegDemuxer encounters new media tracks. This is only
  // invoked when using FFmpegDemuxer, since MSE/ChunkDemuxer handle media
  // tracks separately in WebSourceBufferImpl.
  void OnFFmpegMediaTracksUpdated(std::unique_ptr<MediaTracks> tracks);

  // Sets CdmContext from |cdm| on the pipeline and calls OnCdmAttached()
  // when done.
  void SetCdmInternal(blink::WebContentDecryptionModule* cdm);

  // Called when a CDM has been attached to the |pipeline_|.
  void OnCdmAttached(bool success);

  // Inspects the current playback state and:
  //   - notifies |delegate_|,
  //   - toggles the memory usage reporting timer, and
  //   - toggles suspend/resume as necessary.
  //
  // This method should be called any time its dependent values change. These
  // are:
  //   - is_flinging_,
  //   - hasVideo(),
  //   - delegate_->IsHidden(),
  //   - network_state_, ready_state_,
  //   - is_idle_, must_suspend_,
  //   - paused_, ended_,
  //   - pending_suspend_resume_cycle_,
  //   - enter_pip_callback_,
  void UpdatePlayState();

  // Methods internal to UpdatePlayState().
  PlayState UpdatePlayState_ComputePlayState(bool is_flinging,
                                             bool can_auto_suspend,
                                             bool is_suspended,
                                             bool is_backgrounded);
  void SetDelegateState(DelegateState new_state, bool is_idle);
  void SetMemoryReportingState(bool is_memory_reporting_enabled);
  void SetSuspendState(bool is_suspended);

  void SetDemuxer(std::unique_ptr<Demuxer> demuxer);

  // Called at low frequency to tell external observers how much memory we're
  // using for video playback.  Called by |memory_usage_reporting_timer_|.
  // Memory usage reporting is done in two steps, because |demuxer_| must be
  // accessed on the media thread.
  void ReportMemoryUsage();
  void FinishMemoryUsageReport(int64_t demuxer_memory_usage);

  void OnMainThreadMemoryDump(int32_t id,
                              const base::trace_event::MemoryDumpArgs& args,
                              base::trace_event::ProcessMemoryDump* pmd);
  static void OnMediaThreadMemoryDump(
      int32_t id,
      Demuxer* demuxer,
      const base::trace_event::MemoryDumpArgs& args,
      base::trace_event::ProcessMemoryDump* pmd);

  void OnMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level);

  // Called during OnHidden() when we want a suspended player to enter the
  // paused state after some idle timeout.
  void ScheduleIdlePauseTimer();

  // Returns |true| before HaveFutureData whenever there has been loading
  // progress and we have not been resumed for at least kLoadingToIdleTimeout
  // since then.
  //
  // This is used to delay suspension long enough for preroll to complete, which
  // is necessay because play() will not be called before HaveFutureData (and
  // thus we think we are idle forever).
  bool IsPrerollAttemptNeeded();

  void CreateWatchTimeReporter();
  void UpdateSecondaryProperties();

  void CreateVideoDecodeStatsReporter();

  // Returns true if the player is hidden.
  bool IsHidden() const;

  // Returns true if the player's source is streaming.
  bool IsStreaming() const;

  // Return whether |pipeline_metadata_| is compatible with an overlay. This
  // is intended for android.
  bool DoesOverlaySupportMetadata() const;

  // Whether the playback should be paused when hidden. Uses metadata so has
  // meaning only after the pipeline has started, otherwise returns false.
  // Doesn't check if the playback can actually be paused depending on the
  // pipeline's state.
  bool ShouldPausePlaybackWhenHidden() const;

  // Whether the video track should be disabled when hidden. Uses metadata so
  // has meaning only after the pipeline has started, otherwise returns false.
  // Doesn't check if the video track can actually be disabled depending on the
  // pipeline's state.
  bool ShouldDisableVideoWhenHidden() const;

  // Whether the video is suitable for background playback optimizations (either
  // pausing it or disabling the video track). Uses metadata so has meaning only
  // after the pipeline has started, otherwise returns false.
  // The logical OR between the two methods above that is also used as their
  // common implementation.
  bool IsBackgroundOptimizationCandidate() const;

  // If enabling or disabling background video optimization has been delayed,
  // because of the pipeline not running, seeking or resuming, this method
  // needs to be called to update the optimization state.
  void UpdateBackgroundVideoOptimizationState();

  // Pauses a hidden video only player to save power if possible.
  // Must be called when either of the following happens:
  // - right after the video was hidden,
  // - right ater the pipeline has resumed if the video is hidden.
  void PauseVideoIfNeeded();

  // Disables the video track to save power if possible.
  // Must be called when either of the following happens:
  // - right after the video was hidden,
  // - right after the pipeline has started (|seeking_| is used to detect the
  //   when pipeline started) if the video is hidden,
  // - right ater the pipeline has resumed if the video is hidden.
  void DisableVideoTrackIfNeeded();

  // Enables the video track if it was disabled before to save power.
  // Must be called when either of the following happens:
  // - right after the video was shown,
  // - right before the pipeline is requested to resume
  //   (see https://crbug.com/678374),
  // - right after the pipeline has resumed if the video is not hidden.
  void EnableVideoTrackIfNeeded();

  // Overrides the pipeline statistics returned by GetPiplineStatistics() for
  // tests.
  void SetPipelineStatisticsForTest(const PipelineStatistics& stats);

  // Returns the pipeline statistics or the value overridden by tests.
  PipelineStatistics GetPipelineStatistics() const;

  // Overrides the pipeline media duration returned by
  // GetPipelineMediaDuration() for tests.
  void SetPipelineMediaDurationForTest(base::TimeDelta duration);

  // Return the pipeline media duration or the value overridden by tests.
  base::TimeDelta GetPipelineMediaDuration() const;

  // Records |duration| to the appropriate metric based on whether we're
  // handling a src= or MSE based playback.
  void RecordUnderflowDuration(base::TimeDelta duration);

  // Called by the data source (for src=) or demuxer (for mse) when loading
  // progresses.
  // Can be called quite often.
  void OnProgress();

  // Returns true when we estimate that we can play the rest of the media
  // without buffering.
  bool CanPlayThrough();

  // Internal implementation of Pipeline::Client::OnBufferingStateChange(). When
  // |for_suspended_start| is true, the given state will be set even if the
  // pipeline is not currently stable.
  void OnBufferingStateChangeInternal(BufferingState state,
                                      BufferingStateChangeReason reason,
                                      bool for_suspended_start = false);

  // Records |natural_size| to MediaLog and video height to UMA.
  void RecordVideoNaturalSize(const gfx::Size& natural_size);

  void SetTickClockForTest(const base::TickClock* tick_clock);

  // Returns the current time without clamping to Duration() as required by
  // HTMLMediaElement for handling ended. This method will never return a
  // negative or kInfiniteDuration value. See http://crbug.com/409280,
  // http://crbug.com/645998, and http://crbug.com/751823 for reasons why.
  base::TimeDelta GetCurrentTimeInternal() const;

  // Called by the compositor the very first time a frame is received.
  void OnFirstFrame(base::TimeTicks frame_time);

  // Records timing metrics for three UMA metrics: #key.SRC, #key.MSE, and
  // #key.EME. The SRC and MSE ones are mutually exclusive based on the presence
  // of |chunk_demuxer_|, while the EME one is only recorded if |is_encrypted_|.
  void RecordTimingUMA(const std::string& key, base::TimeDelta elapsed);

  // Records the encryption scheme used by the stream |stream_name|. This is
  // only recorded when metadata is available.
  void RecordEncryptionScheme(const std::string& stream_name,
                              EncryptionScheme encryption_scheme);

  // Returns whether the player is currently displayed in Picture-in-Picture.
  // It will return true even if the player is in AutoPIP mode.
  // The player MUST have a `client_` when this call happen.
  bool IsInPictureInPicture() const;

  // Sets the UKM container name if needed.
  void MaybeSetContainerNameForMetrics();

  // Switch to SurfaceLayer, either initially or from VideoLayer.
  void ActivateSurfaceLayerForVideo();

  // Called by |compositor_| upon presenting a frame, after
  // RequestAnimationFrame() is called.
  void OnNewFramePresentedCallback();

  // Notifies |mb_data_source_| of playback and rate changes which may increase
  // the amount of data the DataSource buffers. Does nothing prior to reaching
  // kReadyStateHaveEnoughData for the first time.
  void MaybeUpdateBufferSizesForPlayback();

  // Create / recreate |smoothness_helper_|, with current features.  Will take
  // no action if we already have a smoothness helper with the same features
  // that we want now.  Will destroy the helper if we shouldn't be measuring
  // smoothness right now.
  void UpdateSmoothnessHelper();

  // Get the LearningTaskController for |task_name|.
  std::unique_ptr<learning::LearningTaskController> GetLearningTaskController(
      const char* task_name);

  // Returns whether the player has an audio track and whether it should be
  // allowed to play it.
  bool HasUnmutedAudio() const;

  blink::WebLocalFrame* const frame_;

  blink::WebMediaPlayer::NetworkState network_state_ =
      WebMediaPlayer::kNetworkStateEmpty;
  blink::WebMediaPlayer::ReadyState ready_state_ =
      WebMediaPlayer::kReadyStateHaveNothing;
  blink::WebMediaPlayer::ReadyState highest_ready_state_ =
      WebMediaPlayer::kReadyStateHaveNothing;

  // Preload state for when |data_source_| is created after setPreload().
  MultibufferDataSource::Preload preload_ = MultibufferDataSource::METADATA;

  // Poster state (for UMA reporting).
  bool has_poster_ = false;

  // Task runner for posting tasks on Chrome's main thread. Also used
  // for DCHECKs so methods calls won't execute in the wrong thread.
  const scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  const scoped_refptr<base::SingleThreadTaskRunner> media_task_runner_;
  const scoped_refptr<base::TaskRunner> worker_task_runner_;
  std::unique_ptr<MediaLog> media_log_;

  // |pipeline_controller_| owns an instance of Pipeline.
  std::unique_ptr<PipelineController> pipeline_controller_;

  // The LoadType passed in the |load_type| parameter of the load() call.
  LoadType load_type_ = kLoadTypeURL;

  // Cache of metadata for answering hasAudio(), hasVideo(), and naturalSize().
  PipelineMetadata pipeline_metadata_;

  // Whether the video is known to be opaque or not.
  bool opaque_ = false;

  // Playback state.
  //
  // TODO(scherkus): we have these because Pipeline favours the simplicity of a
  // single "playback rate" over worrying about paused/stopped etc...  It forces
  // all clients to manage the pause+playback rate externally, but is that
  // really a bad thing?
  //
  // TODO(scherkus): since SetPlaybackRate(0) is asynchronous and we don't want
  // to hang the render thread during pause(), we record the time at the same
  // time we pause and then return that value in currentTime().  Otherwise our
  // clock can creep forward a little bit while the asynchronous
  // SetPlaybackRate(0) is being executed.
  double playback_rate_ = 0.0;

  // Counter that limits spam to |media_log_| of |playback_rate_| changes.
  int num_playback_rate_logs_ = 0;

  // Set while paused. |paused_time_| is only valid when |paused_| is true.
  bool paused_ = true;
  base::TimeDelta paused_time_;

  // Set if paused automatically when hidden and need to resume when visible.
  // Reset if paused for any other reason.
  bool paused_when_hidden_ = false;

  // Set when starting, seeking, and resuming (all of which require a Pipeline
  // seek). |seek_time_| is only valid when |seeking_| is true.
  bool seeking_ = false;
  base::TimeDelta seek_time_;

  // Set when doing a restart (a suspend and resume in sequence) of the pipeline
  // in order to destruct and reinitialize the decoders. This is separate from
  // |pending_resume_| and |pending_suspend_| because they can be elided in
  // certain cases, whereas for a restart they must happen.
  // TODO(sandersd,watk): Create a simpler interface for a pipeline restart.
  bool pending_suspend_resume_cycle_ = false;

  // TODO(scherkus): Replace with an explicit ended signal to HTMLMediaElement,
  // see http://crbug.com/409280
  bool ended_ = false;

  // Tracks whether to issue time changed notifications during buffering state
  // changes.
  bool should_notify_time_changed_ = false;

  bool overlay_enabled_ = false;

  // Whether the current decoder requires a restart on overlay transitions.
  bool decoder_requires_restart_for_overlay_ = false;

  blink::WebMediaPlayerClient* const client_;
  blink::WebMediaPlayerEncryptedMediaClient* const encrypted_client_;

  // WebMediaPlayer notifies the |delegate_| of playback state changes using
  // |delegate_id_|; an id provided after registering with the delegate.  The
  // WebMediaPlayer may also receive directives (play, pause) from the delegate
  // via the WebMediaPlayerDelegate::Observer interface after registration.
  //
  // NOTE: HTMLMediaElement is a Blink::PausableObject, and will receive a
  // call to contextDestroyed() when Blink::Document::shutdown() is called.
  // Document::shutdown() is called before the frame detaches (and before the
  // frame is destroyed). RenderFrameImpl owns |delegate_| and is guaranteed
  // to outlive |this|; thus it is safe to store |delegate_| as a raw pointer.
  blink::WebMediaPlayerDelegate* const delegate_;
  int delegate_id_ = 0;

  // The playback state last reported to |delegate_|, to avoid setting duplicate
  // states.
  // TODO(sandersd): The delegate should be implementing deduplication.
  DelegateState delegate_state_ = DelegateState::GONE;
  bool delegate_has_audio_ = false;

  WebMediaPlayerParams::DeferLoadCB defer_load_cb_;

  // Members for notifying upstream clients about internal memory usage.  The
  // |adjust_allocated_memory_cb_| must only be called on |main_task_runner_|.
  base::RepeatingTimer memory_usage_reporting_timer_;
  WebMediaPlayerParams::AdjustAllocatedMemoryCB adjust_allocated_memory_cb_;
  int64_t last_reported_memory_usage_ = 0;
  std::unique_ptr<MemoryDumpProviderProxy> main_thread_mem_dumper_;
  std::unique_ptr<MemoryDumpProviderProxy> media_thread_mem_dumper_;

  // Routes audio playback to either AudioRendererSink or WebAudio.
  scoped_refptr<blink::WebAudioSourceProviderImpl> audio_source_provider_;

  // These two are mutually exclusive:
  //   |data_source_| is used for regular resource loads.
  //   |chunk_demuxer_| is used for Media Source resource loads.
  //
  // |demuxer_| will contain the appropriate demuxer based on which resource
  // load strategy we're using.
  MultibufferDataSource* mb_data_source_ = nullptr;
  std::unique_ptr<DataSource> data_source_;
  std::unique_ptr<Demuxer> demuxer_;
  ChunkDemuxer* chunk_demuxer_ = nullptr;

  std::unique_ptr<base::MemoryPressureListener> memory_pressure_listener_;

  const base::TickClock* tick_clock_ = nullptr;

  std::unique_ptr<BufferedDataSourceHostImpl> buffered_data_source_host_;
  UrlIndex* const url_index_;
  scoped_refptr<viz::RasterContextProvider> raster_context_provider_;

  // Video rendering members.
  // The |compositor_| runs on the compositor thread, or if
  // kEnableSurfaceLayerForVideo is enabled, the media thread. This task runner
  // posts tasks for the |compositor_| on the correct thread.
  scoped_refptr<base::SingleThreadTaskRunner> vfc_task_runner_;
  std::unique_ptr<VideoFrameCompositor>
      compositor_;  // Deleted on |vfc_task_runner_|.
  PaintCanvasVideoRenderer video_renderer_;

  // The compositor layer for displaying the video content when using composited
  // playback.
  scoped_refptr<cc::VideoLayer> video_layer_;

  std::unique_ptr<blink::WebContentDecryptionModuleResult> set_cdm_result_;

  // If a CdmContext is attached keep a reference to the CdmContextRef, so that
  // it is not destroyed until after the pipeline is done with it.
  std::unique_ptr<CdmContextRef> cdm_context_ref_;

  // Keep track of the CdmContextRef while it is in the process of attaching to
  // the pipeline.
  std::unique_ptr<CdmContextRef> pending_cdm_context_ref_;

  // True when encryption is detected, either by demuxer or by presence of a
  // ContentDecyprtionModule (CDM).
  bool is_encrypted_ = false;

  // Captured once the cdm is provided to SetCdmInternal(). Used in creation of
  // |video_decode_stats_reporter_|.
  base::Optional<CdmConfig> cdm_config_;

  // String identifying the KeySystem described by |cdm_config_|. Empty until a
  // CDM has been attached. Used in creation |video_decode_stats_reporter_|.
  std::string key_system_;

  // Tracks if we are currently flinging a video (e.g. in a RemotePlayback
  // session). Used to prevent videos from being paused when hidden.
  // TODO(https://crbug.com/839651): remove or rename this flag, when removing
  // IsRemote().
  bool is_flinging_ = false;

  // Tracks if we are currently using a remote renderer. See
  // SwitchToRemoteRenderer().
  bool is_remote_rendering_ = false;

  // The last volume received by setVolume() and the last volume multiplier from
  // OnVolumeMultiplierUpdate().  The multiplier is typical 1.0, but may be less
  // if the WebMediaPlayerDelegate has requested a volume reduction (ducking)
  // for a transient sound.  Playout volume is derived by volume * multiplier.
  double volume_ = 1.0;
  double volume_multiplier_ = 1.0;

  std::unique_ptr<RendererFactorySelector> renderer_factory_selector_;

  // For canceling AndroidOverlay routing token requests.
  base::CancelableOnceCallback<void(const base::UnguessableToken&)>
      token_available_cb_;

  // If overlay info is requested before we have it, then the request is saved
  // and satisfied once the overlay info is available. If the decoder does not
  // require restart to change surfaces, this is callback is kept until cleared
  // by the decoder.
  ProvideOverlayInfoCB provide_overlay_info_cb_;

  // On Android an overlay surface means using
  // SurfaceView instead of SurfaceTexture.

  // Allow overlays for all video on android.
  bool always_enable_overlays_ = false;

  // Suppresses calls to OnPipelineError() after destruction / shutdown has been
  // started; prevents us from spuriously logging errors that are transient or
  // unimportant.
  bool suppress_destruction_errors_ = false;

  // TODO(dalecurtis): The following comment is inaccurate as this value is also
  // used for, for example, data URLs.
  // Used for HLS playback and in certain fallback paths (e.g. on older devices
  // that can't support the unified media pipeline).
  GURL loaded_url_;

  // NOTE: |using_media_player_renderer_| is set based on the usage of a
  // MediaResource::Type::URL in StartPipeline(). This works because
  // MediaPlayerRendererClientFactory is the only factory that uses
  // MediaResource::Type::URL for now.
  bool using_media_player_renderer_ = false;

#if defined(OS_ANDROID)
  // Set during the initial DoLoad() call. Used to determine whether to allow
  // credentials or not for MediaPlayerRenderer.
  bool allow_media_player_renderer_credentials_ = false;
#endif

  // Stores the current position state of the media.
  media_session::MediaPosition media_position_state_;

  // Set whenever the demuxer encounters an HLS file.
  // This flag is distinct from |using_media_player_renderer_|, because on older
  // devices we might use MediaPlayerRenderer for non HLS playback.
  bool demuxer_found_hls_ = false;

  // Called sometime after the media is suspended in a playing state in
  // OnFrameHidden(), causing the state to change to paused.
  base::OneShotTimer background_pause_timer_;

  // Monitors the watch time of the played content.
  std::unique_ptr<WatchTimeReporter> watch_time_reporter_;
  std::string audio_decoder_name_;
  std::string video_decoder_name_;

  // The time at which DoLoad() is executed.
  base::TimeTicks load_start_time_;

  // Time elapsed time from |load_start_time_| to OnMetadata(). Used to later
  // adjust |load_start_time_| if a suspended startup occurred.
  base::TimeDelta time_to_metadata_;
  bool skip_metrics_due_to_startup_suspend_ = false;

  bool have_reported_time_to_play_ready_ = false;

  // Records pipeline statistics for describing media capabilities.
  std::unique_ptr<VideoDecodeStatsReporter> video_decode_stats_reporter_;

  // Elapsed time since we've last reached BUFFERING_HAVE_NOTHING.
  std::unique_ptr<base::ElapsedTimer> underflow_timer_;

  // Used to track loading progress, used by IsPrerollAttemptNeeded().
  // |preroll_attempt_pending_| indicates that the clock has been reset
  // (awaiting a resume to start), while |preroll_attempt_start_time_| tracks
  // when a preroll attempt began.
  bool preroll_attempt_pending_ = false;
  base::TimeTicks preroll_attempt_start_time_;

  // Monitors the player events.
  base::WeakPtr<MediaObserver> observer_;

  // Owns the weblayer and obtains/maintains SurfaceIds for
  // kUseSurfaceLayerForVideo feature.
  std::unique_ptr<blink::WebSurfaceLayerBridge> bridge_;

  // The maximum video keyframe distance that allows triggering background
  // playback optimizations (non-MSE).
  base::TimeDelta max_keyframe_distance_to_disable_background_video_;

  // The maximum video keyframe distance that allows triggering background
  // playback optimizations (MSE).
  base::TimeDelta max_keyframe_distance_to_disable_background_video_mse_;

  // When MSE memory pressure based garbage collection is enabled, the
  // |enable_instant_source_buffer_gc| controls whether the GC is done
  // immediately on memory pressure notification or during the next SourceBuffer
  // append (slower, but MSE spec compliant).
  bool enable_instant_source_buffer_gc_ = false;

  // Whether disabled the video track as an optimization.
  bool video_track_disabled_ = false;

  // Whether the pipeline is being resumed at the moment.
  bool is_pipeline_resuming_ = false;

  // When this is true, pipeline will not be auto suspended.
  bool disable_pipeline_auto_suspend_ = false;

  // Pipeline statistics overridden by tests.
  base::Optional<PipelineStatistics> pipeline_statistics_for_test_;

  // Pipeline media duration overridden by tests.
  base::Optional<base::TimeDelta> pipeline_media_duration_for_test_;

  // Whether the video requires a user gesture to resume after it was paused in
  // the background. Affects the value of ShouldPausePlaybackWhenHidden().
  bool video_locked_when_paused_when_hidden_ = false;

  // Whether embedded media experience is currently enabled.
  bool embedded_media_experience_enabled_ = false;

  // When should we use SurfaceLayer for video?
  blink::WebMediaPlayer::SurfaceLayerMode surface_layer_mode_ =
      blink::WebMediaPlayer::SurfaceLayerMode::kNever;

  // Whether surface layer is currently in use to display frames.
  bool surface_layer_for_video_enabled_ = false;

  CreateSurfaceLayerBridgeCB create_bridge_callback_;

  bool initial_video_height_recorded_ = false;

  enum class OverlayMode {
    // All overlays are turned off.
    kNoOverlays,

    // Use AndroidOverlay for overlays.
    kUseAndroidOverlay,
  };

  OverlayMode overlay_mode_ = OverlayMode::kNoOverlays;

  // Optional callback to request the routing token for AndroidOverlay.
  RequestRoutingTokenCallback request_routing_token_cb_;

  // If |overlay_routing_token_is_pending_| is false, then
  // |overlay_routing_token_| contains the routing token we should send, if any.
  // Otherwise, |overlay_routing_token_| is undefined.  We set the flag while
  // we have a request for the token that hasn't been answered yet; i.e., it
  // means that we don't know what, if any, token we should be using.
  bool overlay_routing_token_is_pending_ = false;
  OverlayInfo::RoutingToken overlay_routing_token_;

  OverlayInfo overlay_info_;

  base::CancelableClosure update_background_status_cb_;

  mojo::Remote<mojom::MediaMetricsProvider> media_metrics_provider_;
  mojo::Remote<mojom::PlaybackEventsRecorder> playback_events_recorder_;

  base::Optional<ReadyState> stale_state_override_for_testing_;

  // True if we attempt to start the media pipeline in a suspended state for
  // preload=metadata. Cleared upon pipeline startup.
  bool attempting_suspended_start_ = false;

  // True if a frame has ever been rendered.
  bool has_first_frame_ = false;

  // True if we have not yet rendered a first frame, but one is needed. Set back
  // to false as soon as |has_first_frame_| is set to true.
  bool needs_first_frame_ = false;

  // True if StartPipeline() completed a lazy load startup.
  bool did_lazy_load_ = false;

  // Whether the renderer should automatically suspend media playback in
  // background tabs.
  bool is_background_suspend_enabled_ = false;

  // If disabled, video will be auto paused when in background. Affects the
  // value of ShouldPausePlaybackWhenHidden().
  bool is_background_video_playback_enabled_ = true;

  // Whether background video optimization is supported on current platform.
  bool is_background_video_track_optimization_supported_ = true;

  base::CancelableOnceClosure have_enough_after_lazy_load_cb_;

  // State for simplified watch time reporting.
  RendererFactoryType reported_renderer_type_ = RendererFactoryType::kDefault;
  SimpleWatchTimer simple_watch_timer_;

  LearningExperimentHelper will_play_helper_;

  // Stores the optional override Demuxer until it is used in DoLoad().
  std::unique_ptr<Demuxer> demuxer_override_;

  std::unique_ptr<PowerStatusHelper> power_status_helper_;

  // Created while playing, deleted otherwise.
  std::unique_ptr<SmoothnessHelper> smoothness_helper_;
  base::Optional<int> last_reported_fps_;

  base::WeakPtr<WebMediaPlayerImpl> weak_this_;
  base::WeakPtrFactory<WebMediaPlayerImpl> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WebMediaPlayerImpl);
};

}  // namespace media

#endif  // MEDIA_BLINK_WEBMEDIAPLAYER_IMPL_H_
