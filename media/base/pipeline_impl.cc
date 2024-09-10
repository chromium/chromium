// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/pipeline_impl.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "media/base/callback_timeout_helpers.h"
#include "media/base/cdm_context.h"
#include "media/base/decoder.h"
#include "media/base/demuxer.h"
#include "media/base/media_log.h"
#include "media/base/media_switches.h"
#include "media/base/renderer.h"
#include "media/base/renderer_client.h"
#include "media/base/serial_runner.h"
#include "media/base/timestamp_constants.h"
#include "media/base/video_decoder_config.h"

#if BUILDFLAG(IS_WIN)
#include "media/base/win/mf_feature_checks.h"
#endif  // BUILDFLAG(IS_WIN)

static const double kDefaultPlaybackRate = 0.0;
static const float kDefaultVolume = 1.0f;

namespace media {

namespace {

gfx::Size GetRotatedVideoSize(VideoRotation rotation, gfx::Size natural_size) {
  if (rotation == VIDEO_ROTATION_90 || rotation == VIDEO_ROTATION_270)
    return gfx::Size(natural_size.height(), natural_size.width());
  return natural_size;
}

void OnCallbackTimeout(const std::string& uma_name,
                       bool called_on_destruction) {
  DVLOG(1) << "Callback Timeout: " << uma_name
           << ", called_on_destruction=" << called_on_destruction;
  base::UmaHistogramEnumeration(
      uma_name, called_on_destruction
                    ? CallbackTimeoutStatus::kDestructedBeforeTimeout
                    : CallbackTimeoutStatus::kTimeout);
}

}  // namespace

// A wrapper of Renderer that runs on the |media_task_runner|.
// |default_renderer| in Start() and Resume() helps avoid a round trip to the
// render main task runner for Renderer creation in most cases which could add
// latency to start-to-play time.
class PipelineImpl::RendererWrapper final : public DemuxerHost,
                                            public RendererClient {
 public:
  RendererWrapper(scoped_refptr<base::SequencedTaskRunner> media_task_runner,
                  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
                  MediaLog* media_log);

  RendererWrapper(const RendererWrapper&) = delete;
  RendererWrapper& operator=(const RendererWrapper&) = delete;

  ~RendererWrapper() final;

  void Start(StartType start_type,
             Demuxer* demuxer,
             std::unique_ptr<Renderer> default_renderer,
             base::WeakPtr<PipelineImpl> weak_pipeline);
  void Stop();
  void Seek(base::TimeDelta time);
  void Suspend();
  void Resume(std::unique_ptr<Renderer> default_renderer, base::TimeDelta time);
  void SetPlaybackRate(double playback_rate);
  void SetVolume(float volume);
  void SetLatencyHint(std::optional<base::TimeDelta> latency_hint);
  void SetPreservesPitch(bool preserves_pitch);
  void SetWasPlayedWithUserActivationAndHighMediaEngagement(
      bool was_played_with_user_activation_and_high_media_engagement);
  base::TimeDelta GetMediaTime() const;
  Ranges<base::TimeDelta> GetBufferedTimeRanges() const;
  bool DidLoadingProgress();
  PipelineStatistics GetStatistics() const;
  void SetCdm(CdmContext* cdm_context, CdmAttachedCB cdm_attached_cb);

  // |enabled_track_ids| contains track ids of enabled audio tracks.
  void OnEnabledAudioTracksChanged(
      const std::vector<MediaTrack::Id>& enabled_track_ids,
      base::OnceClosure change_completed_cb);

  // |selected_track_id| is either empty, which means no video track is
  // selected, or contains the selected video track id.
  void OnSelectedVideoTrackChanged(
      std::optional<MediaTrack::Id> selected_track_id,
      base::OnceClosure change_completed_cb);

  void OnExternalVideoFrameRequest();

 private:
  enum class State {
    kCreated,
    kStarting,
    kSeeking,
    kPlaying,
    kStopping,
    kStopped,
    kSuspending,
    kSuspended,
    kResuming,
  };

  // Contains state shared between main and media thread. On the media thread
  // each member can be read without locking, but writing requires locking. On
  // the main thread reading requires a lock and writing is prohibited.
  //
  // This struct should only contain state that is not immediately needed by
  // PipelineClient and can be cached on the media thread until queried.
  // Alternatively we could cache it on the main thread by posting the
  // notification to the main thread. But some of the state change notifications
  // (OnStatisticsUpdate and OnBufferedTimeRangesChanged) arrive much more
  // frequently than needed. Posting all those notifications to the main thread
  // causes performance issues: crbug.com/619975.
  struct SharedState {
    // TODO(scherkus): Enforce that Renderer is only called on a single thread,
    // even for accessing media time http://crbug.com/370634
    //
    // Note: Renderer implementations must support GetMediaTime() being called
    // on both the main and media threads. RendererWrapper::GetMediaTime() calls
    // it from the main thread (locked).
    std::unique_ptr<Renderer> renderer;

    // True when OnBufferedTimeRangesChanged() has been called more recently
    // than DidLoadingProgress().
    bool did_loading_progress = false;

    // Amount of available buffered data as reported by Demuxer.
    Ranges<base::TimeDelta> buffered_time_ranges;

    // Accumulated statistics reported by the renderer.
    PipelineStatistics statistics;

    // The media timestamp to return while the pipeline is suspended.
    // Otherwise set to kNoTimestamp.
    base::TimeDelta suspend_timestamp = kNoTimestamp;
  };

  static const char* GetStateString(State state);

  base::TimeDelta GetCurrentTimestamp();

  void OnDemuxerCompletedTrackChange(
      DemuxerStream::Type stream_type,
      base::OnceClosure change_completed_cb,
      const std::vector<DemuxerStream*>& streams);

  // DemuxerHost implementation.
  void OnBufferedTimeRangesChanged(const Ranges<base::TimeDelta>& ranges) final;
  void SetDuration(base::TimeDelta duration) final;
  void OnDemuxerError(PipelineStatus error) final;

  // RendererClient implementation.
  void OnError(PipelineStatus error) final;
  void OnFallback(PipelineStatus status) final;
  void OnEnded() final;
  void OnStatisticsUpdate(const PipelineStatistics& stats) final;
  void OnBufferingStateChange(BufferingState state,
                              BufferingStateChangeReason reason) final;
  void OnWaiting(WaitingReason reason) final;
  void OnAudioConfigChange(const AudioDecoderConfig& config) final;
  void OnVideoConfigChange(const VideoDecoderConfig& config) final;
  void OnVideoNaturalSizeChange(const gfx::Size& size) final;
  void OnVideoOpacityChange(bool opaque) final;
  void OnVideoFrameRateChange(std::optional<int> fps) final;

  // Common handlers for notifications from renderers and demuxer.
  void OnPipelineError(PipelineStatus error);
  void OnCdmAttached(CdmAttachedCB cdm_attached_cb,
                     CdmContext* cdm_context,
                     bool success);
  void CheckPlaybackEnded();

  // State transition tasks.
  void SetState(State next_state);
  void CompleteSeek(base::TimeDelta seek_time, PipelineStatus status);
  void CompleteSuspend(PipelineStatus status);
  void InitializeDemuxer(PipelineStatusCallback done_cb);
  void CreateRenderer(PipelineStatusCallback done_cb);
  void OnRendererCreated(PipelineStatusCallback done_cb,
                         std::unique_ptr<Renderer> renderer);
  void InitializeRenderer(PipelineStatusCallback done_cb);
  void DestroyRenderer();
  void ReportMetadata(StartType start_type);

  // Returns whether there's any encrypted stream in the demuxer.
  bool HasEncryptedStream();

  // Uses |default_renderer_| as the Renderer or asynchronously creates a new
  // one by calling back to PipelineImpl. Fires |done_cb| with the result.
  void CreateRendererInternal(PipelineStatusCallback done_cb);

  const scoped_refptr<base::SequencedTaskRunner> media_task_runner_;
  const scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  const raw_ptr<MediaLog, AcrossTasksDanglingUntriaged> media_log_;

  // A weak pointer to PipelineImpl. Must only use on the main task runner.
  base::WeakPtr<PipelineImpl> weak_pipeline_;

  raw_ptr<Demuxer> demuxer_;

  // Optional default renderer to be used during Start() and Resume(). If not
  // available, or if a different Renderer is needed,
  // PipelineImpl::AsyncCreateRenderer() will be called to create a new one.
  std::unique_ptr<Renderer> default_renderer_;

  double playback_rate_;
  float volume_;
  std::optional<base::TimeDelta> latency_hint_;
  raw_ptr<CdmContext, DanglingUntriaged> cdm_context_ = nullptr;

  // By default, apply pitch adjustments.
  bool preserves_pitch_ = true;

  bool was_played_with_user_activation_and_high_media_engagement_ = false;

  // Lock used to serialize |shared_state_|.
  // TODO(crbug.com/41419817): Add GUARDED_BY annotations.
  mutable base::Lock shared_state_lock_;

  // State shared between main and media thread.
  SharedState shared_state_;

  // Current state of the pipeline.
  State state_;

  // Status of the pipeline.  Initialized to PIPELINE_OK which indicates that
  // the pipeline is operating correctly. Any other value indicates that the
  // pipeline is stopped or is stopping.  Clients can call the Stop() method to
  // reset the pipeline state, and restore this to PIPELINE_OK.
  PipelineStatus status_;

  // Whether we've received the audio/video ended events.
  bool renderer_ended_;

  // Series of tasks to Start(), Seek(), and Resume().
  std::unique_ptr<SerialRunner> pending_callbacks_;

  // Callback to store the |done_cb| when CreateRenderer() needs to wait for a
  // CDM to be set. Should only be set in kStarting or kResuming states.
  PipelineStatusCallback create_renderer_done_cb_;

  // Called from non-media threads when an error occurs.
  PipelineStatusCB error_cb_;

  base::WeakPtrFactory<RendererWrapper> weak_factory_{this};
};

PipelineImpl::RendererWrapper::RendererWrapper(
    scoped_refptr<base::SequencedTaskRunner> media_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    MediaLog* media_log)
    : media_task_runner_(std::move(media_task_runner)),
      main_task_runner_(std::move(main_task_runner)),
      media_log_(media_log),
      demuxer_(nullptr),
      playback_rate_(kDefaultPlaybackRate),
      volume_(kDefaultVolume),
      state_(State::kCreated),
      status_(PIPELINE_OK),
      renderer_ended_(false) {}

PipelineImpl::RendererWrapper::~RendererWrapper() {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(state_ == State::kCreated || state_ == State::kStopped);
}

// Note that the usage of base::Unretained() with the renderers is considered
// safe as they are owned by |pending_callbacks_| and share the same lifetime.
//
// That being said, deleting the renderers while keeping |pending_callbacks_|
// running on the media thread would result in crashes.

void PipelineImpl::RendererWrapper::Start(
    StartType start_type,
    Demuxer* demuxer,
    std::unique_ptr<Renderer> default_renderer,
    base::WeakPtr<PipelineImpl> weak_pipeline) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(state_ == State::kCreated || state_ == State::kStopped)
      << "Received start in unexpected state: " << GetStateString(state_);
  DCHECK(!demuxer_);
  DCHECK(!renderer_ended_);

  SetState(State::kStarting);
  demuxer_ = demuxer;
  default_renderer_ = std::move(default_renderer);
  weak_pipeline_ = weak_pipeline;

  // Setup |error_cb_| on the media thread.
  error_cb_ = base::BindRepeating(&RendererWrapper::OnPipelineError,
                                  weak_factory_.GetWeakPtr());

  // Queue asynchronous actions required to start.
  DCHECK(!pending_callbacks_);
  SerialRunner::Queue fns;

  // Initialize demuxer.
  fns.Push(base::BindOnce(&RendererWrapper::InitializeDemuxer,
                          weak_factory_.GetWeakPtr()));

  // Once the demuxer is initialized successfully, media metadata must be
  // available - report the metadata to client. If starting without a renderer
  // we'll complete initialization at this point.
  fns.Push(base::BindOnce(&RendererWrapper::ReportMetadata,
                          weak_factory_.GetWeakPtr(), start_type));

  // Create renderer.
  fns.Push(base::BindOnce(&RendererWrapper::CreateRenderer,
                          weak_factory_.GetWeakPtr()));

  // Initialize renderer.
  fns.Push(base::BindOnce(&RendererWrapper::InitializeRenderer,
                          weak_factory_.GetWeakPtr()));

  // Run tasks.
  pending_callbacks_ = SerialRunner::Run(
      std::move(fns),
      base::BindOnce(&RendererWrapper::CompleteSeek, weak_factory_.GetWeakPtr(),
                     base::TimeDelta()));
}

void PipelineImpl::RendererWrapper::Stop() {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(state_ != State::kStopping && state_ != State::kStopped);

  SetState(State::kStopping);

  if (shared_state_.statistics.video_frames_decoded > 0) {
    UMA_HISTOGRAM_COUNTS_1M("Media.DroppedFrameCount",
                            shared_state_.statistics.video_frames_dropped);
  }

  // If we stop during starting/seeking/suspending/resuming we don't want to
  // leave outstanding callbacks around. The callbacks also do not get run if
  // the pipeline is stopped before it had a chance to complete outstanding
  // tasks.
  pending_callbacks_.reset();
  weak_factory_.InvalidateWeakPtrs();

  DestroyRenderer();

  if (demuxer_) {
    demuxer_->Stop();
    demuxer_ = nullptr;
  }

  SetState(State::kStopped);

  // Reset the status. Otherwise, if we encountered an error, new errors will
  // never be propagated. See https://crbug.com/812465.
  status_ = PIPELINE_OK;
}

void PipelineImpl::RendererWrapper::Seek(base::TimeDelta time) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());

  // Suppress seeking if we're not fully started.
  if (state_ != State::kPlaying) {
    DCHECK(state_ == State::kStopping || state_ == State::kStopped)
        << "Receive seek in unexpected state: " << GetStateString(state_);
    OnPipelineError(PIPELINE_ERROR_INVALID_STATE);
    return;
  }

  base::TimeDelta seek_timestamp = std::max(time, demuxer_->GetStartTime());

  SetState(State::kSeeking);
  renderer_ended_ = false;

  // Queue asynchronous actions required to start.
  DCHECK(!pending_callbacks_);
  SerialRunner::Queue bound_fns;

  // Abort any reads the renderer may be blocked on.
  demuxer_->AbortPendingReads();


  // Flush.
  DCHECK(shared_state_.renderer);
  bound_fns.Push(base::BindOnce(
      &Renderer::Flush, base::Unretained(shared_state_.renderer.get())));

  // Seek demuxer.
  bound_fns.Push(base::BindOnce(&Demuxer::Seek, base::Unretained(demuxer_),
                                seek_timestamp));

  // Run tasks.
  pending_callbacks_ = SerialRunner::Run(
      std::move(bound_fns),
      base::BindOnce(&RendererWrapper::CompleteSeek, weak_factory_.GetWeakPtr(),
                     seek_timestamp));
}

void PipelineImpl::RendererWrapper::Suspend() {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());

  // Suppress suspending if we're not playing.
  if (state_ != State::kPlaying) {
    DCHECK(state_ == State::kStopping || state_ == State::kStopped)
        << "Receive suspend in unexpected state: " << GetStateString(state_);
    OnPipelineError(PIPELINE_ERROR_INVALID_STATE);
    return;
  }
  DCHECK(!pending_callbacks_.get());

  SetState(State::kSuspending);

  // Freeze playback and record the media time before destroying the renderer.
  shared_state_.renderer->SetPlaybackRate(0.0);
  {
    base::AutoLock auto_lock(shared_state_lock_);
    DCHECK(shared_state_.renderer);
    shared_state_.suspend_timestamp = shared_state_.renderer->GetMediaTime();
    DCHECK(shared_state_.suspend_timestamp != kNoTimestamp);
  }

  // Queue the asynchronous actions required to stop playback.
  SerialRunner::Queue fns;

  // No need to flush the renderer since it's going to be destroyed.
  pending_callbacks_ = SerialRunner::Run(
      std::move(fns), base::BindOnce(&RendererWrapper::CompleteSuspend,
                                     weak_factory_.GetWeakPtr()));
}

void PipelineImpl::RendererWrapper::Resume(
    std::unique_ptr<Renderer> default_renderer,
    base::TimeDelta timestamp) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());

  // Suppress resuming if we're not suspended.
  if (state_ != State::kSuspended) {
    DCHECK(state_ == State::kStopping || state_ == State::kStopped)
        << "Receive resume in unexpected state: " << GetStateString(state_);
    OnPipelineError(PIPELINE_ERROR_INVALID_STATE);
    return;
  }
  DCHECK(!pending_callbacks_.get());

  if (!default_renderer) {
    OnPipelineError({PIPELINE_ERROR_INITIALIZATION_FAILED,
                     "Media Renderer creation failed during resume!"});
    return;
  }

  SetState(State::kResuming);

  {
    base::AutoLock auto_lock(shared_state_lock_);
    DCHECK(!shared_state_.renderer);
  }

  default_renderer_ = std::move(default_renderer);
  renderer_ended_ = false;
  base::TimeDelta start_timestamp =
      std::max(timestamp, demuxer_->GetStartTime());

  // Queue the asynchronous actions required to start playback.
  SerialRunner::Queue fns;

  fns.Push(base::BindOnce(&Demuxer::Seek, base::Unretained(demuxer_),
                          start_timestamp));

  fns.Push(base::BindOnce(&RendererWrapper::CreateRenderer,
                          weak_factory_.GetWeakPtr()));

  fns.Push(base::BindOnce(&RendererWrapper::InitializeRenderer,
                          weak_factory_.GetWeakPtr()));

  pending_callbacks_ = SerialRunner::Run(
      std::move(fns),
      base::BindOnce(&RendererWrapper::CompleteSeek, weak_factory_.GetWeakPtr(),
                     start_timestamp));
}

void PipelineImpl::RendererWrapper::SetPlaybackRate(double playback_rate) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());

  playback_rate_ = playback_rate;
  if (state_ == State::kPlaying) {
    shared_state_.renderer->SetPlaybackRate(playback_rate_);
  }

  if (state_ != State::kCreated && state_ != State::kStopping &&
      state_ != State::kStopped) {
    DCHECK(demuxer_);
    demuxer_->SetPlaybackRate(playback_rate);
  }
}

void PipelineImpl::RendererWrapper::SetVolume(float volume) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());

  volume_ = volume;
  if (shared_state_.renderer)
    shared_state_.renderer->SetVolume(volume_);
}

void PipelineImpl::RendererWrapper::SetLatencyHint(
    std::optional<base::TimeDelta> latency_hint) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());

  if (latency_hint_ == latency_hint)
    return;

  latency_hint_ = latency_hint;
  if (shared_state_.renderer)
    shared_state_.renderer->SetLatencyHint(latency_hint_);
}

void PipelineImpl::RendererWrapper::SetPreservesPitch(bool preserves_pitch) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());

  if (preserves_pitch_ == preserves_pitch)
    return;

  preserves_pitch_ = preserves_pitch;
  if (shared_state_.renderer)
    shared_state_.renderer->SetPreservesPitch(preserves_pitch_);
}

void PipelineImpl::RendererWrapper::
    SetWasPlayedWithUserActivationAndHighMediaEngagement(
        bool was_played_with_user_activation_and_high_media_engagement) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());

  was_played_with_user_activation_and_high_media_engagement_ =
      was_played_with_user_activation_and_high_media_engagement;
  if (shared_state_.renderer) {
    shared_state_.renderer
        ->SetWasPlayedWithUserActivationAndHighMediaEngagement(
            was_played_with_user_activation_and_high_media_engagement_);
  }
}

base::TimeDelta PipelineImpl::RendererWrapper::GetMediaTime() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  base::AutoLock auto_lock(shared_state_lock_);
  if (shared_state_.suspend_timestamp != kNoTimestamp)
    return shared_state_.suspend_timestamp;
  return shared_state_.renderer ? shared_state_.renderer->GetMediaTime()
                                : base::TimeDelta();
}

Ranges<base::TimeDelta> PipelineImpl::RendererWrapper::GetBufferedTimeRanges()
    const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  base::AutoLock auto_lock(shared_state_lock_);
  return shared_state_.buffered_time_ranges;
}

bool PipelineImpl::RendererWrapper::DidLoadingProgress() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  base::AutoLock auto_lock(shared_state_lock_);
  bool did_progress = shared_state_.did_loading_progress;
  shared_state_.did_loading_progress = false;
  return did_progress;
}

PipelineStatistics PipelineImpl::RendererWrapper::GetStatistics() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  base::AutoLock auto_lock(shared_state_lock_);
  return shared_state_.statistics;
}

void PipelineImpl::RendererWrapper::SetCdm(CdmContext* cdm_context,
                                           CdmAttachedCB cdm_attached_cb) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(cdm_context);

  // If there's already a renderer, set the CDM on the renderer directly.
  if (shared_state_.renderer) {
    shared_state_.renderer->SetCdm(
        cdm_context, base::BindOnce(&RendererWrapper::OnCdmAttached,
                                    weak_factory_.GetWeakPtr(),
                                    std::move(cdm_attached_cb), cdm_context));
    return;
  }

  // Otherwise, wait for the Renderer to be created and the CDM will be set
  // in InitializeRenderer().
  cdm_context_ = cdm_context;
  std::move(cdm_attached_cb).Run(true);

  // Continue Renderer creation if it's waiting for the CDM to be set.
  if (create_renderer_done_cb_)
    CreateRendererInternal(std::move(create_renderer_done_cb_));
}

void PipelineImpl::RendererWrapper::CreateRendererInternal(
    PipelineStatusCallback done_cb) {
  DVLOG(1) << __func__;

  DCHECK(state_ == State::kStarting || state_ == State::kResuming);
  DCHECK(cdm_context_ || !HasEncryptedStream())
      << "CDM should be available now if has encrypted stream";

  std::optional<RendererType> renderer_type;

#if BUILDFLAG(IS_WIN)
  if (cdm_context_) {
    if (cdm_context_->RequiresMediaFoundationRenderer()) {
      renderer_type = RendererType::kMediaFoundation;
    } else if (media::SupportMediaFoundationClearPlayback()) {
      // When MediaFoundation for Clear is enabled, the base renderer
      // type is set to MediaFoundation. In order to ensure DRM systems
      // built on non-Media Foundation pipelines continue to work we
      // explicitly set renderer_type to Default.
      renderer_type = RendererType::kRendererImpl;
    }
  }
#endif  // BUILDFLAG(IS_WIN)

  // TODO(xhwang): During Resume(), the |default_renderer_| might already match
  // the |renderer_type|, in which case we shouldn't need to create a new one.
  if (!default_renderer_ || renderer_type) {
    // Create the Renderer asynchronously on the main task runner. Use
    // base::BindPostTaskToCurrentDefault to call OnRendererCreated() on the
    // media task runner.
    auto renderer_created_cb = base::BindPostTaskToCurrentDefault(
        base::BindOnce(&RendererWrapper::OnRendererCreated,
                       weak_factory_.GetWeakPtr(), std::move(done_cb)));
    main_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&PipelineImpl::AsyncCreateRenderer, weak_pipeline_,
                       renderer_type, std::move(renderer_created_cb)));
    return;
  }

  // Just use the default one.
  OnRendererCreated(std::move(done_cb), std::move(default_renderer_));
}

void PipelineImpl::RendererWrapper::OnBufferedTimeRangesChanged(
    const Ranges<base::TimeDelta>& ranges) {
  // TODO(alokp): Add thread DCHECK after ensuring that all Demuxer
  // implementations call DemuxerHost on the media thread.
  base::AutoLock auto_lock(shared_state_lock_);
  shared_state_.did_loading_progress = true;
  shared_state_.buffered_time_ranges = ranges;
}

void PipelineImpl::RendererWrapper::SetDuration(base::TimeDelta duration) {
  // TODO(alokp): Add thread DCHECK after ensuring that all Demuxer
  // implementations call DemuxerHost on the media thread.
  media_log_->AddEvent<MediaLogEvent::kDurationChanged>(duration);
  main_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&PipelineImpl::OnDurationChange, weak_pipeline_,
                                duration));
}

const char* PipelineImpl::RendererWrapper::GetStateString(State state) {
  switch (state) {
    case State::kCreated:
      return "kCreated";
    case State::kStarting:
      return "kStarting";
    case State::kSeeking:
      return "kSeeking";
    case State::kPlaying:
      return "kPlaying";
    case State::kStopping:
      return "kStopping";
    case State::kStopped:
      return "kStopped";
    case State::kSuspending:
      return "kSuspending";
    case State::kSuspended:
      return "kSuspended";
    case State::kResuming:
      return "kResuming";
  }
  NOTREACHED();
}

void PipelineImpl::RendererWrapper::OnDemuxerError(PipelineStatus error) {
  // TODO(alokp): Add thread DCHECK after ensuring that all Demuxer
  // implementations call DemuxerHost on the media thread.
  DCHECK(error_cb_);
  media_task_runner_->PostTask(FROM_HERE, base::BindOnce(error_cb_, error));
}

void PipelineImpl::RendererWrapper::OnError(PipelineStatus error) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(error_cb_);
  media_task_runner_->PostTask(FROM_HERE, base::BindOnce(error_cb_, error));
}

void PipelineImpl::RendererWrapper::OnFallback(PipelineStatus fallback) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  main_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&PipelineImpl::OnFallback, weak_pipeline_,
                                std::move(fallback).AddHere()));
}

void PipelineImpl::RendererWrapper::OnEnded() {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  media_log_->AddEvent<MediaLogEvent::kEnded>();

  if (state_ != State::kPlaying) {
    return;
  }

  DCHECK(!renderer_ended_);
  renderer_ended_ = true;
  CheckPlaybackEnded();
}

// TODO(crbug.com/40564930): Combine this functionality into
// renderer->GetMediaTime().
base::TimeDelta PipelineImpl::RendererWrapper::GetCurrentTimestamp() {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(demuxer_);
  DCHECK(shared_state_.renderer || state_ != State::kPlaying);

  return state_ == State::kPlaying ? shared_state_.renderer->GetMediaTime()
                                   : demuxer_->GetStartTime();
}

void PipelineImpl::OnEnabledAudioTracksChanged(
    const std::vector<MediaTrack::Id>& enabled_track_ids,
    base::OnceClosure change_completed_cb) {
  DCHECK(thread_checker_.CalledOnValidThread());
  media_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &RendererWrapper::OnEnabledAudioTracksChanged,
          base::Unretained(renderer_wrapper_.get()), enabled_track_ids,
          base::BindPostTaskToCurrentDefault(std::move(change_completed_cb))));
}

void PipelineImpl::RendererWrapper::OnEnabledAudioTracksChanged(
    const std::vector<MediaTrack::Id>& enabled_track_ids,
    base::OnceClosure change_completed_cb) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());

  // If the pipeline has been created, but not started yet, we may still receive
  // track notifications from blink level (e.g. when video track gets deselected
  // due to player/pipeline belonging to a background tab). We can safely ignore
  // these, since WebMediaPlayerImpl will ensure that demuxer stream / track
  // status is in sync with blink after pipeline is started.
  if (state_ == State::kCreated) {
    DCHECK(!demuxer_);
    std::move(change_completed_cb).Run();
    return;
  }

  // Track status notifications might be delivered asynchronously. If we receive
  // a notification when pipeline is stopped/shut down, it's safe to ignore it.
  if (state_ == State::kStopping || state_ == State::kStopped) {
    std::move(change_completed_cb).Run();
    return;
  }
  demuxer_->OnEnabledAudioTracksChanged(
      enabled_track_ids, GetCurrentTimestamp(),
      base::BindOnce(&RendererWrapper::OnDemuxerCompletedTrackChange,
                     weak_factory_.GetWeakPtr(), DemuxerStream::AUDIO,
                     std::move(change_completed_cb)));
}

void PipelineImpl::OnSelectedVideoTrackChanged(
    std::optional<MediaTrack::Id> selected_track_id,
    base::OnceClosure change_completed_cb) {
  DCHECK(thread_checker_.CalledOnValidThread());
  media_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &RendererWrapper::OnSelectedVideoTrackChanged,
          base::Unretained(renderer_wrapper_.get()), selected_track_id,
          base::BindPostTaskToCurrentDefault(std::move(change_completed_cb))));
}

void PipelineImpl::RendererWrapper::OnSelectedVideoTrackChanged(
    std::optional<MediaTrack::Id> selected_track_id,
    base::OnceClosure change_completed_cb) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());

  // See RenderWrapper::OnEnabledAudioTracksChanged.
  if (state_ == State::kCreated) {
    DCHECK(!demuxer_);
    std::move(change_completed_cb).Run();
    return;
  }

  if (state_ == State::kStopping || state_ == State::kStopped) {
    std::move(change_completed_cb).Run();
    return;
  }

  std::vector<MediaTrack::Id> tracks;
  if (selected_track_id)
    tracks.push_back(*selected_track_id);

  demuxer_->OnSelectedVideoTrackChanged(
      tracks, GetCurrentTimestamp(),
      base::BindOnce(&RendererWrapper::OnDemuxerCompletedTrackChange,
                     weak_factory_.GetWeakPtr(), DemuxerStream::VIDEO,
                     std::move(change_completed_cb)));
}

void PipelineImpl::OnExternalVideoFrameRequest() {
  // This function is currently a no-op unless we're on a Windows build with
  // Media Foundation for Clear running.
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!external_video_frame_request_signaled_) {
    external_video_frame_request_signaled_ = true;
    media_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&RendererWrapper::OnExternalVideoFrameRequest,
                                  base::Unretained(renderer_wrapper_.get())));
  }
}

void PipelineImpl::RendererWrapper::OnExternalVideoFrameRequest() {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  if (!shared_state_.renderer) {
    return;
  }

  shared_state_.renderer->OnExternalVideoFrameRequest();
}

void PipelineImpl::RendererWrapper::OnDemuxerCompletedTrackChange(
    DemuxerStream::Type stream_type,
    base::OnceClosure change_completed_cb,
    const std::vector<DemuxerStream*>& streams) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  if (!shared_state_.renderer) {
    // This can happen if the pipeline has been suspended.
    std::move(change_completed_cb).Run();
    return;
  }

  switch (stream_type) {
    case DemuxerStream::AUDIO:
      shared_state_.renderer->OnEnabledAudioTracksChanged(
          streams, std::move(change_completed_cb));
      break;
    case DemuxerStream::VIDEO:
      shared_state_.renderer->OnSelectedVideoTracksChanged(
          streams, std::move(change_completed_cb));
      break;
    case DemuxerStream::UNKNOWN:  // Fail on unknown type.
      NOTREACHED();
  }
}

void PipelineImpl::RendererWrapper::OnStatisticsUpdate(
    const PipelineStatistics& stats) {
  DVLOG(3) << __func__;
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());

  base::AutoLock auto_lock(shared_state_lock_);
  shared_state_.statistics.audio_bytes_decoded += stats.audio_bytes_decoded;
  shared_state_.statistics.video_bytes_decoded += stats.video_bytes_decoded;
  shared_state_.statistics.video_frames_decoded += stats.video_frames_decoded;
  shared_state_.statistics.video_frames_decoded_power_efficient +=
      stats.video_frames_decoded_power_efficient;
  shared_state_.statistics.video_frames_dropped += stats.video_frames_dropped;
  shared_state_.statistics.audio_memory_usage += stats.audio_memory_usage;
  shared_state_.statistics.video_memory_usage += stats.video_memory_usage;

  if (stats.audio_pipeline_info.decoder_type != AudioDecoderType::kUnknown &&
      stats.audio_pipeline_info !=
          shared_state_.statistics.audio_pipeline_info) {
    shared_state_.statistics.audio_pipeline_info = stats.audio_pipeline_info;
    main_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&PipelineImpl::OnAudioPipelineInfoChange,
                                  weak_pipeline_, stats.audio_pipeline_info));
  }

  if (stats.video_pipeline_info.decoder_type != VideoDecoderType::kUnknown &&
      stats.video_pipeline_info !=
          shared_state_.statistics.video_pipeline_info) {
    shared_state_.statistics.video_pipeline_info = stats.video_pipeline_info;
    main_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&PipelineImpl::OnVideoPipelineInfoChange,
                                  weak_pipeline_, stats.video_pipeline_info));
  }

  if (stats.video_frame_duration_average != kNoTimestamp) {
    shared_state_.statistics.video_frame_duration_average =
        stats.video_frame_duration_average;
  }

  base::TimeDelta old_key_frame_distance_average =
      shared_state_.statistics.video_keyframe_distance_average;
  if (stats.video_keyframe_distance_average != kNoTimestamp) {
    shared_state_.statistics.video_keyframe_distance_average =
        stats.video_keyframe_distance_average;
  }

  if (shared_state_.statistics.video_keyframe_distance_average !=
      old_key_frame_distance_average) {
    main_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&PipelineImpl::OnVideoAverageKeyframeDistanceUpdate,
                       weak_pipeline_));
  }
}

void PipelineImpl::RendererWrapper::OnBufferingStateChange(
    BufferingState state,
    BufferingStateChangeReason reason) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  DVLOG(2) << __func__ << "(" << state << ", " << reason << ") ";

  main_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&PipelineImpl::OnBufferingStateChange,
                                weak_pipeline_, state, reason));
}

void PipelineImpl::RendererWrapper::OnWaiting(WaitingReason reason) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());

  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&PipelineImpl::OnWaiting, weak_pipeline_, reason));
}

void PipelineImpl::RendererWrapper::OnVideoNaturalSizeChange(
    const gfx::Size& size) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());

  main_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&PipelineImpl::OnVideoNaturalSizeChange,
                                weak_pipeline_, size));
}

void PipelineImpl::RendererWrapper::OnVideoOpacityChange(bool opaque) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());

  main_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&PipelineImpl::OnVideoOpacityChange,
                                weak_pipeline_, opaque));
}

void PipelineImpl::RendererWrapper::OnVideoFrameRateChange(
    std::optional<int> fps) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());

  main_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&PipelineImpl::OnVideoFrameRateChange,
                                weak_pipeline_, fps));
}

void PipelineImpl::RendererWrapper::OnAudioConfigChange(
    const AudioDecoderConfig& config) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());

  main_task_runner_->PostTask(FROM_HERE,
                              base::BindOnce(&PipelineImpl::OnAudioConfigChange,
                                             weak_pipeline_, config));
}

void PipelineImpl::RendererWrapper::OnVideoConfigChange(
    const VideoDecoderConfig& config) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());

  main_task_runner_->PostTask(FROM_HERE,
                              base::BindOnce(&PipelineImpl::OnVideoConfigChange,
                                             weak_pipeline_, config));
}

void PipelineImpl::RendererWrapper::OnPipelineError(PipelineStatus error) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(!error.is_ok()) << "PIPELINE_OK isn't an error!";

  // Preserve existing abnormal status.
  if (status_ != PIPELINE_OK)
    return;

  // If the pipeline is already stopping or stopped we don't need to report an
  // error. Similarly if the pipeline is suspending or suspended, the error may
  // be recoverable, so don't propagate it now, instead let the subsequent seek
  // during resume propagate it if it's unrecoverable.
  if (state_ == State::kStopping || state_ == State::kStopped ||
      state_ == State::kSuspending || state_ == State::kSuspended) {
    return;
  }

  // PIPELINE_ERROR_HARDWARE_CONTEXT_RESET and DEMUXER_ERROR_DETECTED_HLS are
  // not fatal errors. They are just signals to restart or reconfig the
  // pipeline.
  if (error != PIPELINE_ERROR_HARDWARE_CONTEXT_RESET &&
      error != DEMUXER_ERROR_DETECTED_HLS) {
    status_ = error;
  }

  main_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&PipelineImpl::OnError, weak_pipeline_, error));
}

void PipelineImpl::RendererWrapper::OnCdmAttached(CdmAttachedCB cdm_attached_cb,
                                                  CdmContext* cdm_context,
                                                  bool success) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());

  if (success)
    cdm_context_ = cdm_context;
  std::move(cdm_attached_cb).Run(success);
}

void PipelineImpl::RendererWrapper::CheckPlaybackEnded() {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());

  if (shared_state_.renderer && !renderer_ended_)
    return;

  // Don't fire an ended event if we're already in an error state.
  if (status_ != PIPELINE_OK)
    return;

  main_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&PipelineImpl::OnEnded, weak_pipeline_));
}

void PipelineImpl::RendererWrapper::SetState(State next_state) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  DVLOG(1) << GetStateString(state_) << " -> " << GetStateString(next_state);

  state_ = next_state;

  // TODO(tmathmeyer) Make State serializable so GetStateString won't need
  // to be called here.
  media_log_->AddEvent<MediaLogEvent::kPipelineStateChange>(
      std::string(GetStateString(next_state)));
}

void PipelineImpl::RendererWrapper::CompleteSeek(base::TimeDelta seek_time,
                                                 PipelineStatus status) {
  DVLOG(1) << __func__ << ": seek_time=" << seek_time << ", status=" << status;
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(state_ == State::kStarting || state_ == State::kSeeking ||
         state_ == State::kResuming);

  DCHECK(pending_callbacks_);
  pending_callbacks_.reset();

  if (status != PIPELINE_OK) {
    OnPipelineError(status);
    return;
  }

  shared_state_.renderer->StartPlayingFrom(
      std::max(seek_time, demuxer_->GetStartTime()));
  {
    base::AutoLock auto_lock(shared_state_lock_);
    shared_state_.suspend_timestamp = kNoTimestamp;
  }

  shared_state_.renderer->SetPlaybackRate(playback_rate_);

  SetState(State::kPlaying);
  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&PipelineImpl::OnSeekDone, weak_pipeline_, false));
}

void PipelineImpl::RendererWrapper::CompleteSuspend(PipelineStatus status) {
  DVLOG(1) << __func__ << ": status=" << status;
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  DCHECK_EQ(State::kSuspending, state_);

  DCHECK(pending_callbacks_);
  pending_callbacks_.reset();

  // In case we are suspending or suspended, the error may be recoverable,
  // so don't propagate it now, instead let the subsequent seek during resume
  // propagate it if it's unrecoverable.
  LOG_IF(WARNING, status != PIPELINE_OK)
      << "Encountered pipeline error while suspending: " << status;

  DestroyRenderer();
  {
    base::AutoLock auto_lock(shared_state_lock_);
    shared_state_.statistics.audio_memory_usage = 0;
    shared_state_.statistics.video_memory_usage = 0;
  }

  // Abort any reads the renderer may have kicked off.
  demuxer_->AbortPendingReads();

  SetState(State::kSuspended);
  main_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&PipelineImpl::OnSuspendDone, weak_pipeline_));
}

void PipelineImpl::RendererWrapper::InitializeDemuxer(
    PipelineStatusCallback done_cb) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());

  demuxer_->Initialize(this, std::move(done_cb));
}

void PipelineImpl::RendererWrapper::CreateRenderer(
    PipelineStatusCallback done_cb) {
  DVLOG(1) << __func__;
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(state_ == State::kStarting || state_ == State::kResuming);

  if (HasEncryptedStream() && !cdm_context_) {
    DVLOG(1) << __func__ << ": Has encrypted stream but CDM is not set.";
    create_renderer_done_cb_ = std::move(done_cb);
    OnWaiting(WaitingReason::kNoCdm);
    return;
  }

  CreateRendererInternal(std::move(done_cb));
}

void PipelineImpl::RendererWrapper::OnRendererCreated(
    PipelineStatusCallback done_cb,
    std::unique_ptr<Renderer> renderer) {
  DVLOG(1) << __func__ << ": renderer=" << renderer.get();
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());

  if (!renderer) {
    std::move(done_cb).Run({PIPELINE_ERROR_INITIALIZATION_FAILED,
                            "Media Renderer creation failed!"});
    return;
  }

  {
    base::AutoLock auto_lock(shared_state_lock_);
    DCHECK(!shared_state_.renderer);
    shared_state_.renderer = std::move(renderer);
  }
  std::move(done_cb).Run(PIPELINE_OK);
}

void PipelineImpl::RendererWrapper::InitializeRenderer(
    PipelineStatusCallback done_cb) {
  DVLOG(1) << __func__;
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());

  switch (demuxer_->GetType()) {
    case MediaResource::Type::kStream:
      if (demuxer_->GetAllStreams().empty()) {
        DVLOG(1) << "Error: demuxer does not have an audio or a video stream.";
        std::move(done_cb).Run(PIPELINE_ERROR_COULD_NOT_RENDER);
        return;
      }
      break;

    case MediaResource::Type::KUrl:
      // NOTE: Empty GURL are not valid.
      if (!demuxer_->GetMediaUrlParams().media_url.is_valid()) {
        DVLOG(1) << "Error: demuxer does not have a valid URL.";
        std::move(done_cb).Run(PIPELINE_ERROR_COULD_NOT_RENDER);
        return;
      }
      break;
  }

  if (cdm_context_)
    shared_state_.renderer->SetCdm(cdm_context_, base::DoNothing());

  if (latency_hint_)
    shared_state_.renderer->SetLatencyHint(latency_hint_);

  shared_state_.renderer->SetPreservesPitch(preserves_pitch_);

  // Calling SetVolume() before Initialize() allows renderers to optimize for
  // power by avoiding initialization of audio output until necessary.
  shared_state_.renderer->SetVolume(volume_);

  shared_state_.renderer->SetWasPlayedWithUserActivationAndHighMediaEngagement(
      was_played_with_user_activation_and_high_media_engagement_);

  // Initialize Renderer and report timeout UMA.
  std::string uma_name = "Media.InitializeRendererTimeout";
  base::UmaHistogramEnumeration(uma_name, CallbackTimeoutStatus::kCreate);
  shared_state_.renderer->Initialize(
      demuxer_, this,
      WrapCallbackWithTimeoutHandler(
          std::move(done_cb), /*timeout_delay=*/base::Seconds(10),
          base::BindOnce(&OnCallbackTimeout, uma_name)));
}

void PipelineImpl::RendererWrapper::DestroyRenderer() {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());

  // Destroy the renderer outside the lock scope to avoid holding the lock
  // while renderer is being destroyed (in case Renderer destructor is costly).
  std::unique_ptr<Renderer> renderer;
  {
    base::AutoLock auto_lock(shared_state_lock_);
    renderer.swap(shared_state_.renderer);
  }
}

void PipelineImpl::RendererWrapper::ReportMetadata(StartType start_type) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());

  PipelineMetadata metadata;
  std::vector<DemuxerStream*> streams;

  switch (demuxer_->GetType()) {
    case MediaResource::Type::kStream:
      metadata.timeline_offset = demuxer_->GetTimelineOffset();
      // TODO(servolk): What should we do about metadata for multiple streams?
      streams = demuxer_->GetAllStreams();
      for (media::DemuxerStream* stream : streams) {
        if (stream->type() == DemuxerStream::VIDEO && !metadata.has_video) {
          metadata.has_video = true;
          metadata.natural_size = GetRotatedVideoSize(
              stream->video_decoder_config().video_transformation().rotation,
              stream->video_decoder_config().natural_size());
          metadata.video_decoder_config = stream->video_decoder_config();
        }
        if (stream->type() == DemuxerStream::AUDIO && !metadata.has_audio) {
          metadata.has_audio = true;
          metadata.audio_decoder_config = stream->audio_decoder_config();
        }
      }
      break;

    case MediaResource::Type::KUrl:
      // We don't know if the MediaPlayerRender has Audio/Video until we start
      // playing. Conservatively assume that they do.
      metadata.has_video = true;
      metadata.has_audio = true;
      break;
  }

  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&PipelineImpl::OnMetadata, weak_pipeline_, metadata));

  // If suspended start has not been requested, or is not allowed given the
  // metadata, continue the normal renderer initialization path.
  if (start_type == StartType::kNormal ||
      (start_type == StartType::kSuspendAfterMetadataForAudioOnly &&
       metadata.has_video)) {
    return;
  }

  // Abort pending render initialization tasks and suspend the pipeline.
  pending_callbacks_.reset();
  DestroyRenderer();
  shared_state_.suspend_timestamp =
      std::max(base::TimeDelta(), demuxer_->GetStartTime());
  SetState(State::kSuspended);
  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&PipelineImpl::OnSeekDone, weak_pipeline_, true));
}

bool PipelineImpl::RendererWrapper::HasEncryptedStream() {
  // Encrypted streams are only handled explicitly for STREAM type.
  if (demuxer_->GetType() != MediaResource::Type::kStream)
    return false;

  auto streams = demuxer_->GetAllStreams();

  for (media::DemuxerStream* stream : streams) {
    if (stream->type() == DemuxerStream::AUDIO &&
        stream->audio_decoder_config().is_encrypted())
      return true;
    if (stream->type() == DemuxerStream::VIDEO &&
        stream->video_decoder_config().is_encrypted())
      return true;
  }

  return false;
}

PipelineImpl::PipelineImpl(
    scoped_refptr<base::SequencedTaskRunner> media_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    CreateRendererCB create_renderer_cb,
    MediaLog* media_log)
    : media_task_runner_(media_task_runner),
      create_renderer_cb_(create_renderer_cb),
      media_log_(media_log),
      client_(nullptr),
      playback_rate_(kDefaultPlaybackRate),
      volume_(kDefaultVolume),
      is_suspended_(false) {
  DVLOG(2) << __func__;
  DCHECK(create_renderer_cb_);

  renderer_wrapper_ = std::make_unique<RendererWrapper>(
      media_task_runner_, std::move(main_task_runner), media_log_);
}

PipelineImpl::~PipelineImpl() {
  DVLOG(2) << __func__;
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!client_) << "Stop() must complete before destroying object";
  DCHECK(!seek_cb_);
  DCHECK(!suspend_cb_);
  DCHECK(!weak_factory_.HasWeakPtrs())
      << "Stop() should have invalidated all weak pointers";

  // RendererWrapper is deleted on the media thread.
  media_task_runner_->DeleteSoon(FROM_HERE, renderer_wrapper_.release());
}

void PipelineImpl::Start(StartType start_type,
                         Demuxer* demuxer,
                         Client* client,
                         PipelineStatusCallback seek_cb) {
  DVLOG(2) << __func__ << ": start_type=" << static_cast<int>(start_type);
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(demuxer);
  DCHECK(client);
  DCHECK(seek_cb);

  DCHECK(!client_);
  DCHECK(!seek_cb_);
  client_ = client;
  seek_cb_ = std::move(seek_cb);
  last_media_time_ = base::TimeDelta();
  seek_time_ = kNoTimestamp;
  external_video_frame_request_signaled_ = false;

  // By default, create a default renderer to avoid additional start-to-play
  // latency caused by asynchronous Renderer creation. When |start_type| is
  // kSuspendAfterMetadata, latency is not important and the video may never
  // play. In this case, not creating a default renderer to reduce memory usage.
  std::unique_ptr<Renderer> default_renderer;
  if (start_type != StartType::kSuspendAfterMetadata)
    default_renderer = create_renderer_cb_.Run(std::nullopt);

  media_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&RendererWrapper::Start,
                     base::Unretained(renderer_wrapper_.get()), start_type,
                     demuxer, std::move(default_renderer),
                     weak_factory_.GetWeakPtr()));
}

void PipelineImpl::Stop() {
  DVLOG(2) << __func__;
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!IsRunning()) {
    DVLOG(2) << "Media pipeline isn't running. Ignoring Stop()";
    return;
  }

  if (media_task_runner_->RunsTasksInCurrentSequence()) {
    // This path is executed by unittests that share media and main threads.
    renderer_wrapper_->Stop();
  } else {
    // This path is executed by production code where the two task runners -
    // main and media - live on different threads.
    media_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&RendererWrapper::Stop,
                                  base::Unretained(renderer_wrapper_.get())));
  }

  // Once the pipeline is stopped, nothing is reported back to the client.
  // Reset all callbacks and client handle.
  seek_cb_.Reset();
  suspend_cb_.Reset();
  client_ = nullptr;

  // Invalidate self weak pointers effectively canceling all pending
  // notifications in the message queue.
  weak_factory_.InvalidateWeakPtrs();
}

void PipelineImpl::Seek(base::TimeDelta time, PipelineStatusCallback seek_cb) {
  DVLOG(2) << __func__ << " to " << time;
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(seek_cb);

  if (!IsRunning()) {
    DLOG(ERROR) << "Media pipeline isn't running. Ignoring Seek().";
    std::move(seek_cb).Run(PIPELINE_ERROR_INVALID_STATE);
    return;
  }

  DCHECK(!seek_cb_);
  seek_cb_ = std::move(seek_cb);
  seek_time_ = time;
  last_media_time_ = base::TimeDelta();
  media_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&RendererWrapper::Seek,
                     base::Unretained(renderer_wrapper_.get()), time));
}

void PipelineImpl::Suspend(PipelineStatusCallback suspend_cb) {
  DVLOG(2) << __func__;
  DCHECK(suspend_cb);

  DCHECK(IsRunning());
  DCHECK(!suspend_cb_);
  suspend_cb_ = std::move(suspend_cb);

  media_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&RendererWrapper::Suspend,
                                base::Unretained(renderer_wrapper_.get())));
}

void PipelineImpl::Resume(base::TimeDelta time,
                          PipelineStatusCallback seek_cb) {
  DVLOG(2) << __func__;
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(seek_cb);

  DCHECK(IsRunning());
  DCHECK(!seek_cb_);
  seek_cb_ = std::move(seek_cb);
  seek_time_ = time;
  last_media_time_ = base::TimeDelta();
  external_video_frame_request_signaled_ = false;

  // Always create a default renderer for Resume(). Creation error is handled in
  // `RendererWrapper::Resume()`.
  auto default_renderer = create_renderer_cb_.Run(std::nullopt);

  media_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&RendererWrapper::Resume,
                                base::Unretained(renderer_wrapper_.get()),
                                std::move(default_renderer), time));
}

bool PipelineImpl::IsRunning() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return !!client_;
}

bool PipelineImpl::IsSuspended() const {
  DVLOG(2) << __func__ << "(" << is_suspended_ << ")";
  DCHECK(thread_checker_.CalledOnValidThread());
  return is_suspended_;
}

double PipelineImpl::GetPlaybackRate() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return playback_rate_;
}

void PipelineImpl::SetPlaybackRate(double playback_rate) {
  DVLOG(2) << __func__ << "(" << playback_rate << ")";
  DCHECK(thread_checker_.CalledOnValidThread());

  // Not checking IsRunning() so we can set the playback rate before Start().

  if (playback_rate < 0.0) {
    DVLOG(1) << __func__ << ": Invalid playback rate " << playback_rate;
    return;
  }

  playback_rate_ = playback_rate;
  media_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&RendererWrapper::SetPlaybackRate,
                                base::Unretained(renderer_wrapper_.get()),
                                playback_rate_));
}

float PipelineImpl::GetVolume() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return volume_;
}

void PipelineImpl::SetVolume(float volume) {
  DVLOG(2) << __func__ << "(" << volume << ")";
  DCHECK(thread_checker_.CalledOnValidThread());

  // Not checking IsRunning() so we can set the volume before Start().

  if (volume < 0.0f) {
    DVLOG(1) << __func__ << ": Invalid volume " << volume;
    return;
  }

  volume_ = volume;
  media_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&RendererWrapper::SetVolume,
                     base::Unretained(renderer_wrapper_.get()), volume_));
}

void PipelineImpl::SetLatencyHint(std::optional<base::TimeDelta> latency_hint) {
  DVLOG(1) << __func__ << "("
           << (latency_hint
                   ? base::NumberToString(latency_hint->InMilliseconds()) + "ms"
                   : "null_opt")
           << ")";
  DCHECK(!latency_hint || (*latency_hint >= base::TimeDelta()));
  DCHECK(thread_checker_.CalledOnValidThread());

  // Not checking IsRunning() so we can set the latency hint before Start().
  media_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&RendererWrapper::SetLatencyHint,
                     base::Unretained(renderer_wrapper_.get()), latency_hint));
}

void PipelineImpl::SetPreservesPitch(bool preserves_pitch) {
  DCHECK(thread_checker_.CalledOnValidThread());

  media_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&RendererWrapper::SetPreservesPitch,
                                base::Unretained(renderer_wrapper_.get()),
                                preserves_pitch));
}

void PipelineImpl::SetWasPlayedWithUserActivationAndHighMediaEngagement(
    bool was_played_with_user_activation_and_high_media_engagement) {
  DCHECK(thread_checker_.CalledOnValidThread());

  media_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &RendererWrapper::
              SetWasPlayedWithUserActivationAndHighMediaEngagement,
          base::Unretained(renderer_wrapper_.get()),
          was_played_with_user_activation_and_high_media_engagement));
}

base::TimeDelta PipelineImpl::GetMediaTime() const {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Don't trust renderer time during a pending seek. Renderer may return
  // pre-seek time which may corrupt |last_media_time_| used for clamping.
  if (seek_time_ != kNoTimestamp) {
    DVLOG(3) << __func__ << ": (seeking) " << seek_time_.InMilliseconds()
             << " ms";
    return seek_time_;
  }

  base::TimeDelta media_time = renderer_wrapper_->GetMediaTime();

  // Clamp current media time to the last reported value, this prevents higher
  // level clients from seeing time go backwards based on inaccurate or spurious
  // delay values reported to the AudioClock.
  //
  // It is expected that such events are transient and will be recovered as
  // rendering continues over time.
  if (media_time < last_media_time_) {
    DVLOG(2) << __func__ << ": actual=" << media_time
             << " clamped=" << last_media_time_;
    return last_media_time_;
  }

  DVLOG(3) << __func__ << ": " << media_time.InMilliseconds() << " ms";
  last_media_time_ = media_time;
  return last_media_time_;
}

Ranges<base::TimeDelta> PipelineImpl::GetBufferedTimeRanges() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return renderer_wrapper_->GetBufferedTimeRanges();
}

base::TimeDelta PipelineImpl::GetMediaDuration() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return duration_;
}

bool PipelineImpl::DidLoadingProgress() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return renderer_wrapper_->DidLoadingProgress();
}

PipelineStatistics PipelineImpl::GetStatistics() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return renderer_wrapper_->GetStatistics();
}

void PipelineImpl::SetCdm(CdmContext* cdm_context,
                          CdmAttachedCB cdm_attached_cb) {
  DVLOG(2) << __func__;
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(cdm_context);
  DCHECK(cdm_attached_cb);

  // Not checking IsRunning() so we can set the CDM before Start().

  media_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &RendererWrapper::SetCdm, base::Unretained(renderer_wrapper_.get()),
          cdm_context,
          base::BindPostTaskToCurrentDefault(std::move(cdm_attached_cb))));
}

void PipelineImpl::AsyncCreateRenderer(
    std::optional<RendererType> renderer_type,
    RendererCreatedCB renderer_created_cb) {
  DVLOG(2) << __func__;
  DCHECK(thread_checker_.CalledOnValidThread());

  std::move(renderer_created_cb).Run(create_renderer_cb_.Run(renderer_type));
}

void PipelineImpl::OnError(PipelineStatus error) {
  DVLOG(2) << __func__;
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!error.is_ok()) << "PIPELINE_OK isn't an error!";
  DCHECK(IsRunning());

  // If the error happens during starting/seeking/suspending/resuming,
  // report the error via the completion callback for those tasks.
  // Else report error via the client interface.
  if (seek_cb_) {
    std::move(seek_cb_).Run(error);
    return;
  }

  if (suspend_cb_) {
    std::move(suspend_cb_).Run(error);
    return;
  }

  DCHECK(client_);
  client_->OnError(error);
}

void PipelineImpl::OnFallback(PipelineStatus status) {
  client_->OnFallback(std::move(status).AddHere());
}

void PipelineImpl::OnEnded() {
  DVLOG(2) << __func__;
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(IsRunning());

  DCHECK(client_);
  client_->OnEnded();
}

void PipelineImpl::OnMetadata(const PipelineMetadata& metadata) {
  DVLOG(2) << __func__;
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(IsRunning());

  DCHECK(client_);
  client_->OnMetadata(metadata);
}

void PipelineImpl::OnBufferingStateChange(BufferingState state,
                                          BufferingStateChangeReason reason) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(IsRunning());

  DCHECK(client_);
  client_->OnBufferingStateChange(state, reason);
}

void PipelineImpl::OnDurationChange(base::TimeDelta duration) {
  DVLOG(2) << __func__;
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(IsRunning());

  duration_ = duration;

  DCHECK(client_);
  client_->OnDurationChange();
}

void PipelineImpl::OnWaiting(WaitingReason reason) {
  DVLOG(2) << __func__;
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(IsRunning());

  DCHECK(client_);
  client_->OnWaiting(reason);
}

void PipelineImpl::OnVideoNaturalSizeChange(const gfx::Size& size) {
  DVLOG(2) << __func__;
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(IsRunning());

  DCHECK(client_);
  client_->OnVideoNaturalSizeChange(size);
}

void PipelineImpl::OnVideoOpacityChange(bool opaque) {
  DVLOG(2) << __func__;
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(IsRunning());

  DCHECK(client_);
  client_->OnVideoOpacityChange(opaque);
}

void PipelineImpl::OnVideoFrameRateChange(std::optional<int> fps) {
  DVLOG(2) << __func__;
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(IsRunning());

  DCHECK(client_);
  client_->OnVideoFrameRateChange(fps);
}

void PipelineImpl::OnAudioConfigChange(const AudioDecoderConfig& config) {
  DVLOG(2) << __func__;
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(IsRunning());

  DCHECK(client_);
  client_->OnAudioConfigChange(config);
}

void PipelineImpl::OnVideoConfigChange(const VideoDecoderConfig& config) {
  DVLOG(2) << __func__;
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(IsRunning());

  DCHECK(client_);
  client_->OnVideoConfigChange(config);
}

void PipelineImpl::OnVideoAverageKeyframeDistanceUpdate() {
  DVLOG(2) << __func__;
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(IsRunning());

  DCHECK(client_);
  client_->OnVideoAverageKeyframeDistanceUpdate();
}

void PipelineImpl::OnAudioPipelineInfoChange(const AudioPipelineInfo& info) {
  DVLOG(2) << __func__ << ": info=" << info;
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(IsRunning());

  DCHECK(client_);
  client_->OnAudioPipelineInfoChange(info);
}

void PipelineImpl::OnVideoPipelineInfoChange(const VideoPipelineInfo& info) {
  DVLOG(2) << __func__ << ": info=" << info;
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(IsRunning());

  DCHECK(client_);
  client_->OnVideoPipelineInfoChange(info);
}

void PipelineImpl::OnSeekDone(bool is_suspended) {
  DVLOG(3) << __func__ << ": is_suspended=" << is_suspended;
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(IsRunning());

  seek_time_ = kNoTimestamp;
  is_suspended_ = is_suspended;

  // `seek_cb_` could have been reset in OnError().
  if (seek_cb_)
    std::move(seek_cb_).Run(PIPELINE_OK);
}

void PipelineImpl::OnSuspendDone() {
  DVLOG(3) << __func__;
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(IsRunning());

  is_suspended_ = true;

  // `suspend_cb_` could have been reset in OnError().
  if (suspend_cb_)
    std::move(suspend_cb_).Run(PIPELINE_OK);
}

}  // namespace media
