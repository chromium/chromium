// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/pipeline_impl.h"

#include <algorithm>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_task_runner_handle.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/demuxer.h"
#include "media/base/media_log.h"
#include "media/base/media_switches.h"
#include "media/base/renderer.h"
#include "media/base/renderer_client.h"
#include "media/base/serial_runner.h"
#include "media/base/text_renderer.h"
#include "media/base/text_track_config.h"
#include "media/base/timestamp_constants.h"
#include "media/base/video_decoder_config.h"

static const double kDefaultPlaybackRate = 0.0;
static const float kDefaultVolume = 1.0f;

namespace media {

namespace {

gfx::Size GetRotatedVideoSize(VideoRotation rotation, gfx::Size natural_size) {
  if (rotation == VIDEO_ROTATION_90 || rotation == VIDEO_ROTATION_270)
    return gfx::Size(natural_size.height(), natural_size.width());
  return natural_size;
}

}  // namespace

class PipelineImpl::RendererWrapper : public DemuxerHost,
                                      public RendererClient {
 public:
  RendererWrapper(scoped_refptr<base::SingleThreadTaskRunner> media_task_runner,
                  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
                  MediaLog* media_log);
  ~RendererWrapper() final;

  void Start(StartType start_type,
             Demuxer* demuxer,
             std::unique_ptr<Renderer> renderer,
             base::WeakPtr<PipelineImpl> weak_pipeline);
  void Stop(const base::Closure& stop_cb);
  void Seek(base::TimeDelta time);
  void Suspend();
  void Resume(std::unique_ptr<Renderer> renderer, base::TimeDelta time);
  void SetPlaybackRate(double playback_rate);
  void SetVolume(float volume);
  base::TimeDelta GetMediaTime() const;
  Ranges<base::TimeDelta> GetBufferedTimeRanges() const;
  bool DidLoadingProgress();
  PipelineStatistics GetStatistics() const;
  void SetCdm(CdmContext* cdm_context, const CdmAttachedCB& cdm_attached_cb);

  // |enabled_track_ids| contains track ids of enabled audio tracks.
  void OnEnabledAudioTracksChanged(
      const std::vector<MediaTrack::Id>& enabled_track_ids,
      base::OnceClosure change_completed_cb);

  // |selected_track_id| is either empty, which means no video track is
  // selected, or contains the selected video track id.
  void OnSelectedVideoTrackChanged(
      base::Optional<MediaTrack::Id> selected_track_id,
      base::OnceClosure change_completed_cb);

 private:
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

  base::TimeDelta GetCurrentTimestamp();

  void OnDemuxerCompletedTrackChange(
      base::OnceClosure change_completed_cb,
      DemuxerStream::Type stream_type,
      const std::vector<DemuxerStream*>& streams);

  // DemuxerHost implementaion.
  void OnBufferedTimeRangesChanged(const Ranges<base::TimeDelta>& ranges) final;
  void SetDuration(base::TimeDelta duration) final;
  void OnDemuxerError(PipelineStatus error) final;

  // RendererClient implementation.
  void OnError(PipelineStatus error) final;
  void OnEnded() final;
  void OnStatisticsUpdate(const PipelineStatistics& stats) final;
  void OnBufferingStateChange(BufferingState state) final;
  void OnWaitingForDecryptionKey() final;
  void OnAudioConfigChange(const AudioDecoderConfig& config) final;
  void OnVideoConfigChange(const VideoDecoderConfig& config) final;
  void OnVideoNaturalSizeChange(const gfx::Size& size) final;
  void OnVideoOpacityChange(bool opaque) final;
  void OnDurationChange(base::TimeDelta duration) final;

  // Common handlers for notifications from renderers and demuxer.
  void OnPipelineError(PipelineStatus error);
  void OnCdmAttached(const CdmAttachedCB& cdm_attached_cb,
                     CdmContext* cdm_context,
                     bool success);
  void CheckPlaybackEnded();

  // State transition tasks.
  void SetState(State next_state);
  void CompleteSeek(base::TimeDelta seek_time, PipelineStatus status);
  void CompleteSuspend(PipelineStatus status);
  void InitializeDemuxer(const PipelineStatusCB& done_cb);
  void InitializeRenderer(const PipelineStatusCB& done_cb);
  void DestroyRenderer();
  void ReportMetadata(StartType start_type);

  const scoped_refptr<base::SingleThreadTaskRunner> media_task_runner_;
  const scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  MediaLog* const media_log_;

  base::WeakPtr<PipelineImpl> weak_pipeline_;
  Demuxer* demuxer_;
  double playback_rate_;
  float volume_;
  CdmContext* cdm_context_;

  // Lock used to serialize |shared_state_|.
  // TODO(crbug.com/893739): Add GUARDED_BY annotations.
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

  // Whether we've received the audio/video/text ended events.
  bool renderer_ended_;
  bool text_renderer_ended_;

  // Series of tasks to Start(), Seek(), and Resume().
  std::unique_ptr<SerialRunner> pending_callbacks_;

  base::WeakPtr<RendererWrapper> weak_this_;
  base::WeakPtrFactory<RendererWrapper> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(RendererWrapper);
};

PipelineImpl::RendererWrapper::RendererWrapper(
    scoped_refptr<base::SingleThreadTaskRunner> media_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    MediaLog* media_log)
    : media_task_runner_(std::move(media_task_runner)),
      main_task_runner_(std::move(main_task_runner)),
      media_log_(media_log),
      demuxer_(nullptr),
      playback_rate_(kDefaultPlaybackRate),
      volume_(kDefaultVolume),
      cdm_context_(nullptr),
      state_(kCreated),
      status_(PIPELINE_OK),
      renderer_ended_(false),
      text_renderer_ended_(false),
      weak_factory_(this) {
  weak_this_ = weak_factory_.GetWeakPtr();
}

PipelineImpl::RendererWrapper::~RendererWrapper() {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DCHECK(state_ == kCreated || state_ == kStopped);
}

// Note that the usage of base::Unretained() with the renderers is considered
// safe as they are owned by |pending_callbacks_| and share the same lifetime.
//
// That being said, deleting the renderers while keeping |pending_callbacks_|
// running on the media thread would result in crashes.

void PipelineImpl::RendererWrapper::Start(
    StartType start_type,
    Demuxer* demuxer,
    std::unique_ptr<Renderer> renderer,
    base::WeakPtr<PipelineImpl> weak_pipeline) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DCHECK(state_ == kCreated || state_ == kStopped)
      << "Received start in unexpected state: " << state_;

  // Tracking down http://crbug.com/827990
  CHECK(renderer);

  SetState(kStarting);

  DCHECK(!demuxer_);
  DCHECK(!renderer_ended_);
  DCHECK(!text_renderer_ended_);
  demuxer_ = demuxer;
  {
    base::AutoLock auto_lock(shared_state_lock_);
    DCHECK(!shared_state_.renderer);
    shared_state_.renderer = std::move(renderer);
  }
  weak_pipeline_ = weak_pipeline;

  // Queue asynchronous actions required to start.
  DCHECK(!pending_callbacks_);
  SerialRunner::Queue fns;

  // Initialize demuxer.
  fns.Push(base::Bind(&RendererWrapper::InitializeDemuxer, weak_this_));

  // Once the demuxer is initialized successfully, media metadata must be
  // available - report the metadata to client. If starting without a renderer
  // we'll complete initialization at this point.
  fns.Push(
      base::Bind(&RendererWrapper::ReportMetadata, weak_this_, start_type));

  // Initialize renderer.
  fns.Push(base::Bind(&RendererWrapper::InitializeRenderer, weak_this_));

  // Run tasks.
  pending_callbacks_ =
      SerialRunner::Run(fns, base::Bind(&RendererWrapper::CompleteSeek,
                                        weak_this_, base::TimeDelta()));
}

void PipelineImpl::RendererWrapper::Stop(const base::Closure& stop_cb) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DCHECK(state_ != kStopping && state_ != kStopped);

  SetState(kStopping);

  if (shared_state_.statistics.video_frames_decoded > 0) {
    UMA_HISTOGRAM_COUNTS_1M("Media.DroppedFrameCount",
                            shared_state_.statistics.video_frames_dropped);
  }

  // If we stop during starting/seeking/suspending/resuming we don't want to
  // leave outstanding callbacks around. The callbacks also do not get run if
  // the pipeline is stopped before it had a chance to complete outstanding
  // tasks.
  pending_callbacks_.reset();

  DestroyRenderer();

  if (demuxer_) {
    demuxer_->Stop();
    demuxer_ = NULL;
  }

  SetState(kStopped);

  // Reset the status. Otherwise, if we encountered an error, new erros will
  // never be propagated. See https://crbug.com/812465.
  status_ = PIPELINE_OK;

  // Post the stop callback to enqueue it after the tasks that may have been
  // posted by Demuxer and Renderer during stopping. Note that in theory the
  // tasks posted by Demuxer/Renderer may post even more tasks that will get
  // enqueued after |stop_cb|. This may be problematic because Demuxer may
  // get destroyed as soon as |stop_cb| is run. In practice this is not a
  // problem, but ideally Demuxer should be destroyed on the media thread.
  media_task_runner_->PostTask(FROM_HERE, stop_cb);
}

void PipelineImpl::RendererWrapper::Seek(base::TimeDelta time) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  // Suppress seeking if we're not fully started.
  if (state_ != kPlaying) {
    DCHECK(state_ == kStopping || state_ == kStopped)
        << "Receive seek in unexpected state: " << state_;
    OnPipelineError(PIPELINE_ERROR_INVALID_STATE);
    return;
  }

  base::TimeDelta seek_timestamp = std::max(time, demuxer_->GetStartTime());

  SetState(kSeeking);
  renderer_ended_ = false;
  text_renderer_ended_ = false;

  // Queue asynchronous actions required to start.
  DCHECK(!pending_callbacks_);
  SerialRunner::Queue bound_fns;

  // Abort any reads the renderer may be blocked on.
  demuxer_->AbortPendingReads();


  // Flush.
  DCHECK(shared_state_.renderer);
  bound_fns.Push(base::Bind(&Renderer::Flush,
                            base::Unretained(shared_state_.renderer.get())));

  // Seek demuxer.
  bound_fns.Push(
      base::Bind(&Demuxer::Seek, base::Unretained(demuxer_), seek_timestamp));

  // Run tasks.
  pending_callbacks_ = SerialRunner::Run(
      bound_fns,
      base::Bind(&RendererWrapper::CompleteSeek, weak_this_, seek_timestamp));
}

void PipelineImpl::RendererWrapper::Suspend() {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  // Suppress suspending if we're not playing.
  if (state_ != kPlaying) {
    DCHECK(state_ == kStopping || state_ == kStopped)
        << "Receive suspend in unexpected state: " << state_;
    OnPipelineError(PIPELINE_ERROR_INVALID_STATE);
    return;
  }
  DCHECK(!pending_callbacks_.get());

  SetState(kSuspending);

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
      fns, base::Bind(&RendererWrapper::CompleteSuspend, weak_this_));
}

void PipelineImpl::RendererWrapper::Resume(std::unique_ptr<Renderer> renderer,
                                           base::TimeDelta timestamp) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  // Tracking down http://crbug.com/827990
  CHECK(renderer);

  // Suppress resuming if we're not suspended.
  if (state_ != kSuspended) {
    DCHECK(state_ == kStopping || state_ == kStopped)
        << "Receive resume in unexpected state: " << state_;
    OnPipelineError(PIPELINE_ERROR_INVALID_STATE);
    return;
  }
  DCHECK(!pending_callbacks_.get());

  SetState(kResuming);

  {
    base::AutoLock auto_lock(shared_state_lock_);
    DCHECK(!shared_state_.renderer);
    shared_state_.renderer = std::move(renderer);
  }

  renderer_ended_ = false;
  text_renderer_ended_ = false;
  base::TimeDelta start_timestamp =
      std::max(timestamp, demuxer_->GetStartTime());

  // Queue the asynchronous actions required to start playback.
  SerialRunner::Queue fns;

  fns.Push(
      base::Bind(&Demuxer::Seek, base::Unretained(demuxer_), start_timestamp));

  fns.Push(base::Bind(&RendererWrapper::InitializeRenderer, weak_this_));

  pending_callbacks_ = SerialRunner::Run(
      fns,
      base::Bind(&RendererWrapper::CompleteSeek, weak_this_, start_timestamp));
}

void PipelineImpl::RendererWrapper::SetPlaybackRate(double playback_rate) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  playback_rate_ = playback_rate;
  if (state_ == kPlaying)
    shared_state_.renderer->SetPlaybackRate(playback_rate_);
}

void PipelineImpl::RendererWrapper::SetVolume(float volume) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  volume_ = volume;
  if (state_ == kPlaying)
    shared_state_.renderer->SetVolume(volume_);
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

void PipelineImpl::RendererWrapper::SetCdm(
    CdmContext* cdm_context,
    const CdmAttachedCB& cdm_attached_cb) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  if (!shared_state_.renderer) {
    cdm_context_ = cdm_context;
    cdm_attached_cb.Run(true);
    return;
  }

  shared_state_.renderer->SetCdm(
      cdm_context, base::Bind(&RendererWrapper::OnCdmAttached, weak_this_,
                              cdm_attached_cb, cdm_context));
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
  media_log_->AddEvent(media_log_->CreateTimeEvent(MediaLogEvent::DURATION_SET,
                                                   "duration", duration));
  UMA_HISTOGRAM_CUSTOM_TIMES(
      "Media.Duration2", duration, base::TimeDelta::FromMilliseconds(1),
      base::TimeDelta::FromDays(1), 50 /* bucket_count */);

  main_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&PipelineImpl::OnDurationChange, weak_pipeline_, duration));
}

void PipelineImpl::RendererWrapper::OnDemuxerError(PipelineStatus error) {
  // TODO(alokp): Add thread DCHECK after ensuring that all Demuxer
  // implementations call DemuxerHost on the media thread.
  media_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&RendererWrapper::OnPipelineError, weak_this_, error));
}

void PipelineImpl::RendererWrapper::OnError(PipelineStatus error) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  media_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&RendererWrapper::OnPipelineError, weak_this_, error));
}

void PipelineImpl::RendererWrapper::OnEnded() {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  media_log_->AddEvent(media_log_->CreateEvent(MediaLogEvent::ENDED));

  if (state_ != kPlaying)
    return;

  DCHECK(!renderer_ended_);
  renderer_ended_ = true;
  CheckPlaybackEnded();
}

// TODO(crbug/817089): Combine this functionality into renderer->GetMediaTime().
base::TimeDelta PipelineImpl::RendererWrapper::GetCurrentTimestamp() {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DCHECK(demuxer_);
  DCHECK(shared_state_.renderer || state_ != kPlaying);

  return state_ == kPlaying ? shared_state_.renderer->GetMediaTime()
                            : demuxer_->GetStartTime();
}

void PipelineImpl::OnEnabledAudioTracksChanged(
    const std::vector<MediaTrack::Id>& enabled_track_ids,
    base::OnceClosure change_completed_cb) {
  DCHECK(thread_checker_.CalledOnValidThread());
  media_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&RendererWrapper::OnEnabledAudioTracksChanged,
                     base::Unretained(renderer_wrapper_.get()),
                     enabled_track_ids,
                     BindToCurrentLoop(std::move(change_completed_cb))));
}

void PipelineImpl::RendererWrapper::OnEnabledAudioTracksChanged(
    const std::vector<MediaTrack::Id>& enabled_track_ids,
    base::OnceClosure change_completed_cb) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  // If the pipeline has been created, but not started yet, we may still receive
  // track notifications from blink level (e.g. when video track gets deselected
  // due to player/pipeline belonging to a background tab). We can safely ignore
  // these, since WebMediaPlayerImpl will ensure that demuxer stream / track
  // status is in sync with blink after pipeline is started.
  if (state_ == kCreated) {
    DCHECK(!demuxer_);
    std::move(change_completed_cb).Run();
    return;
  }

  // Track status notifications might be delivered asynchronously. If we receive
  // a notification when pipeline is stopped/shut down, it's safe to ignore it.
  if (state_ == kStopping || state_ == kStopped) {
    std::move(change_completed_cb).Run();
    return;
  }
  demuxer_->OnEnabledAudioTracksChanged(
      enabled_track_ids, GetCurrentTimestamp(),
      base::BindOnce(&RendererWrapper::OnDemuxerCompletedTrackChange,
                     weak_this_, base::Passed(&change_completed_cb)));
}

void PipelineImpl::OnSelectedVideoTrackChanged(
    base::Optional<MediaTrack::Id> selected_track_id,
    base::OnceClosure change_completed_cb) {
  DCHECK(thread_checker_.CalledOnValidThread());
  media_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&RendererWrapper::OnSelectedVideoTrackChanged,
                     base::Unretained(renderer_wrapper_.get()),
                     selected_track_id,
                     BindToCurrentLoop(std::move(change_completed_cb))));
}

void PipelineImpl::RendererWrapper::OnSelectedVideoTrackChanged(
    base::Optional<MediaTrack::Id> selected_track_id,
    base::OnceClosure change_completed_cb) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  // See RenderWrapper::OnEnabledAudioTracksChanged.
  if (state_ == kCreated) {
    DCHECK(!demuxer_);
    std::move(change_completed_cb).Run();
    return;
  }

  if (state_ == kStopping || state_ == kStopped) {
    std::move(change_completed_cb).Run();
    return;
  }

  std::vector<MediaTrack::Id> tracks;
  if (selected_track_id)
    tracks.push_back(*selected_track_id);

  demuxer_->OnSelectedVideoTrackChanged(
      tracks, GetCurrentTimestamp(),
      base::BindOnce(&RendererWrapper::OnDemuxerCompletedTrackChange,
                     weak_this_, base::Passed(&change_completed_cb)));
}

void PipelineImpl::RendererWrapper::OnDemuxerCompletedTrackChange(
    base::OnceClosure change_completed_cb,
    DemuxerStream::Type stream_type,
    const std::vector<DemuxerStream*>& streams) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
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
    // TODO(tmathmeyer): Look into text track switching.
    case DemuxerStream::TEXT:
    case DemuxerStream::UNKNOWN:  // Fail on unknown type.
      NOTREACHED();
  }
}

void PipelineImpl::RendererWrapper::OnStatisticsUpdate(
    const PipelineStatistics& stats) {
  DVLOG(3) << __func__;
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  base::AutoLock auto_lock(shared_state_lock_);
  shared_state_.statistics.audio_bytes_decoded += stats.audio_bytes_decoded;
  shared_state_.statistics.video_bytes_decoded += stats.video_bytes_decoded;
  shared_state_.statistics.video_frames_decoded += stats.video_frames_decoded;
  shared_state_.statistics.video_frames_decoded_power_efficient +=
      stats.video_frames_decoded_power_efficient;
  shared_state_.statistics.video_frames_dropped += stats.video_frames_dropped;
  shared_state_.statistics.audio_memory_usage += stats.audio_memory_usage;
  shared_state_.statistics.video_memory_usage += stats.video_memory_usage;

  if (!stats.audio_decoder_name.empty() &&
      shared_state_.statistics.audio_decoder_name != stats.audio_decoder_name) {
    shared_state_.statistics.audio_decoder_name = stats.audio_decoder_name;
    main_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&PipelineImpl::OnAudioDecoderChange,
                                  weak_pipeline_, stats.audio_decoder_name));
  }
  if (!stats.video_decoder_name.empty() &&
      shared_state_.statistics.video_decoder_name != stats.video_decoder_name) {
    shared_state_.statistics.video_decoder_name = stats.video_decoder_name;
    main_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&PipelineImpl::OnVideoDecoderChange,
                                  weak_pipeline_, stats.video_decoder_name));
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
        base::Bind(&PipelineImpl::OnVideoAverageKeyframeDistanceUpdate,
                   weak_pipeline_));
  }
}

void PipelineImpl::RendererWrapper::OnBufferingStateChange(
    BufferingState state) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DVLOG(2) << __func__ << "(" << state << ") ";

  main_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&PipelineImpl::OnBufferingStateChange, weak_pipeline_, state));
}

void PipelineImpl::RendererWrapper::OnWaitingForDecryptionKey() {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  main_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&PipelineImpl::OnWaitingForDecryptionKey, weak_pipeline_));
}

void PipelineImpl::RendererWrapper::OnVideoNaturalSizeChange(
    const gfx::Size& size) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  main_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&PipelineImpl::OnVideoNaturalSizeChange,
                                weak_pipeline_, size));
}

void PipelineImpl::RendererWrapper::OnVideoOpacityChange(bool opaque) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  main_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&PipelineImpl::OnVideoOpacityChange, weak_pipeline_, opaque));
}

void PipelineImpl::RendererWrapper::OnAudioConfigChange(
    const AudioDecoderConfig& config) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  main_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&PipelineImpl::OnAudioConfigChange, weak_pipeline_, config));
}

void PipelineImpl::RendererWrapper::OnVideoConfigChange(
    const VideoDecoderConfig& config) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  main_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&PipelineImpl::OnVideoConfigChange, weak_pipeline_, config));
}

void PipelineImpl::RendererWrapper::OnDurationChange(base::TimeDelta duration) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  SetDuration(duration);
}

void PipelineImpl::RendererWrapper::OnPipelineError(PipelineStatus error) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DCHECK_NE(PIPELINE_OK, error) << "PIPELINE_OK isn't an error!";

  // Preserve existing abnormal status.
  if (status_ != PIPELINE_OK)
    return;

  // Don't report pipeline error events to the media log here. The embedder
  // will log this when Client::OnError is called. If the pipeline is already
  // stopped or stopping we also don't want to log any event. In case we are
  // suspending or suspended, the error may be recoverable, so don't propagate
  // it now, instead let the subsequent seek during resume propagate it if
  // it's unrecoverable.
  if (state_ == kStopping || state_ == kStopped || state_ == kSuspending ||
      state_ == kSuspended) {
    return;
  }

  status_ = error;
  main_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&PipelineImpl::OnError, weak_pipeline_, error));
}

void PipelineImpl::RendererWrapper::OnCdmAttached(
    const CdmAttachedCB& cdm_attached_cb,
    CdmContext* cdm_context,
    bool success) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  if (success)
    cdm_context_ = cdm_context;
  cdm_attached_cb.Run(success);
}

void PipelineImpl::RendererWrapper::CheckPlaybackEnded() {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  if (shared_state_.renderer && !renderer_ended_)
    return;

  DCHECK_EQ(status_, PIPELINE_OK);
  main_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&PipelineImpl::OnEnded, weak_pipeline_));
}

void PipelineImpl::RendererWrapper::SetState(State next_state) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DVLOG(1) << PipelineImpl::GetStateString(state_) << " -> "
           << PipelineImpl::GetStateString(next_state);

  state_ = next_state;
  media_log_->AddEvent(media_log_->CreatePipelineStateChangedEvent(next_state));
}

void PipelineImpl::RendererWrapper::CompleteSeek(base::TimeDelta seek_time,
                                                 PipelineStatus status) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DCHECK(state_ == kStarting || state_ == kSeeking || state_ == kResuming);

  if (state_ == kStarting) {
    UMA_HISTOGRAM_ENUMERATION("Media.PipelineStatus.Start", status,
                              PIPELINE_STATUS_MAX + 1);
  }

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
  shared_state_.renderer->SetVolume(volume_);

  SetState(kPlaying);
  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&PipelineImpl::OnSeekDone, weak_pipeline_, false));
}

void PipelineImpl::RendererWrapper::CompleteSuspend(PipelineStatus status) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(kSuspending, state_);

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

  SetState(kSuspended);
  main_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&PipelineImpl::OnSuspendDone, weak_pipeline_));
}

void PipelineImpl::RendererWrapper::InitializeDemuxer(
    const PipelineStatusCB& done_cb) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  demuxer_->Initialize(this, done_cb);
}

void PipelineImpl::RendererWrapper::InitializeRenderer(
    const PipelineStatusCB& done_cb) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  switch (demuxer_->GetType()) {
    case MediaResource::Type::STREAM:
      if (demuxer_->GetAllStreams().empty()) {
        DVLOG(1) << "Error: demuxer does not have an audio or a video stream.";
        done_cb.Run(PIPELINE_ERROR_COULD_NOT_RENDER);
        return;
      }
      break;

    case MediaResource::Type::URL:
      // NOTE: Empty GURL are not valid.
      if (!demuxer_->GetMediaUrlParams().media_url.is_valid()) {
        DVLOG(1) << "Error: demuxer does not have a valid URL.";
        done_cb.Run(PIPELINE_ERROR_COULD_NOT_RENDER);
        return;
      }
      break;
  }

  if (cdm_context_) {
    shared_state_.renderer->SetCdm(cdm_context_,
                                   base::Bind(&IgnoreCdmAttached));
  }

  shared_state_.renderer->Initialize(demuxer_, this, done_cb);
}

void PipelineImpl::RendererWrapper::DestroyRenderer() {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  // Destroy the renderer outside the lock scope to avoid holding the lock
  // while renderer is being destroyed (in case Renderer destructor is costly).
  std::unique_ptr<Renderer> renderer;
  {
    base::AutoLock auto_lock(shared_state_lock_);
    renderer.swap(shared_state_.renderer);
  }
}

void PipelineImpl::RendererWrapper::ReportMetadata(StartType start_type) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  PipelineMetadata metadata;
  std::vector<DemuxerStream*> streams;

  switch (demuxer_->GetType()) {
    case MediaResource::Type::STREAM:
      metadata.timeline_offset = demuxer_->GetTimelineOffset();
      // TODO(servolk): What should we do about metadata for multiple streams?
      streams = demuxer_->GetAllStreams();
      for (auto* stream : streams) {
        if (stream->type() == DemuxerStream::VIDEO && !metadata.has_video) {
          metadata.has_video = true;
          metadata.natural_size = GetRotatedVideoSize(
              stream->video_decoder_config().video_rotation(),
              stream->video_decoder_config().natural_size());
          metadata.video_decoder_config = stream->video_decoder_config();
        }
        if (stream->type() == DemuxerStream::AUDIO && !metadata.has_audio) {
          metadata.has_audio = true;
          metadata.audio_decoder_config = stream->audio_decoder_config();
        }
      }
      break;

    case MediaResource::Type::URL:
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
  SetState(kSuspended);
  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&PipelineImpl::OnSeekDone, weak_pipeline_, true));
}

PipelineImpl::PipelineImpl(
    scoped_refptr<base::SingleThreadTaskRunner> media_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    MediaLog* media_log)
    : media_task_runner_(media_task_runner),
      media_log_(media_log),
      client_(nullptr),
      playback_rate_(kDefaultPlaybackRate),
      volume_(kDefaultVolume),
      is_suspended_(false),
      weak_factory_(this) {
  DVLOG(2) << __func__;
  renderer_wrapper_.reset(new RendererWrapper(
      media_task_runner_, std::move(main_task_runner), media_log_));
}

PipelineImpl::~PipelineImpl() {
  DVLOG(2) << __func__;
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!client_) << "Stop() must complete before destroying object";
  DCHECK(!seek_cb_);
  DCHECK(!suspend_cb_);
  DCHECK(!weak_factory_.HasWeakPtrs());

  // RendererWrapper is deleted on the media thread.
  media_task_runner_->DeleteSoon(FROM_HERE, renderer_wrapper_.release());
}

void PipelineImpl::Start(StartType start_type,
                         Demuxer* demuxer,
                         std::unique_ptr<Renderer> renderer,
                         Client* client,
                         const PipelineStatusCB& seek_cb) {
  DVLOG(2) << __func__;
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(demuxer);
  DCHECK(renderer);
  DCHECK(client);
  DCHECK(seek_cb);

  DCHECK(!client_);
  DCHECK(!seek_cb_);
  client_ = client;
  seek_cb_ = seek_cb;
  last_media_time_ = base::TimeDelta();
  seek_time_ = kNoTimestamp;

  media_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&RendererWrapper::Start,
                 base::Unretained(renderer_wrapper_.get()), start_type, demuxer,
                 base::Passed(&renderer), weak_factory_.GetWeakPtr()));
}

void PipelineImpl::Stop() {
  DVLOG(2) << __func__;
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!IsRunning()) {
    DVLOG(2) << "Media pipeline isn't running. Ignoring Stop()";
    return;
  }

  if (media_task_runner_->BelongsToCurrentThread()) {
    // This path is executed by unittests that share media and main threads.
    renderer_wrapper_->Stop(base::DoNothing());
  } else {
    // This path is executed by production code where the two task runners -
    // main and media - live on different threads.
    //
    // TODO(alokp): We should not have to wait for the RendererWrapper::Stop.
    // RendererWrapper holds a raw reference to Demuxer, which in turn holds a
    // raw reference to DataSource. Both Demuxer and DataSource need to live
    // until RendererWrapper is stopped. If RendererWrapper owned Demuxer and
    // Demuxer owned DataSource, we could simply let RendererWrapper get lazily
    // destroyed on the media thread.
    base::WaitableEvent waiter(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                               base::WaitableEvent::InitialState::NOT_SIGNALED);
    base::Closure stop_cb =
        base::Bind(&base::WaitableEvent::Signal, base::Unretained(&waiter));
    // If posting the task fails or the posted task fails to run,
    // we will wait here forever. So add a CHECK to make sure we do not run
    // into those situations.
    CHECK(media_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&RendererWrapper::Stop,
                   base::Unretained(renderer_wrapper_.get()), stop_cb)));
    waiter.Wait();
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

void PipelineImpl::Seek(base::TimeDelta time, const PipelineStatusCB& seek_cb) {
  DVLOG(2) << __func__ << " to " << time.InMicroseconds();
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(seek_cb);

  if (!IsRunning()) {
    DLOG(ERROR) << "Media pipeline isn't running. Ignoring Seek().";
    return;
  }

  DCHECK(!seek_cb_);
  seek_cb_ = seek_cb;
  seek_time_ = time;
  last_media_time_ = base::TimeDelta();
  media_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&RendererWrapper::Seek,
                     base::Unretained(renderer_wrapper_.get()), time));
}

void PipelineImpl::Suspend(const PipelineStatusCB& suspend_cb) {
  DVLOG(2) << __func__;
  DCHECK(suspend_cb);

  DCHECK(IsRunning());
  DCHECK(!suspend_cb_);
  suspend_cb_ = suspend_cb;

  media_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&RendererWrapper::Suspend,
                                base::Unretained(renderer_wrapper_.get())));
}

void PipelineImpl::Resume(std::unique_ptr<Renderer> renderer,
                          base::TimeDelta time,
                          const PipelineStatusCB& seek_cb) {
  DVLOG(2) << __func__;
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(renderer);
  DCHECK(seek_cb);

  DCHECK(IsRunning());
  DCHECK(!seek_cb_);
  seek_cb_ = seek_cb;
  seek_time_ = time;
  last_media_time_ = base::TimeDelta();

  media_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&RendererWrapper::Resume,
                                base::Unretained(renderer_wrapper_.get()),
                                base::Passed(&renderer), time));
}

bool PipelineImpl::IsRunning() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return !!client_;
}

bool PipelineImpl::IsSuspended() const {
  DVLOG(2) << __func__;
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

  if (playback_rate < 0.0)
    return;

  playback_rate_ = playback_rate;
  media_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&RendererWrapper::SetPlaybackRate,
                 base::Unretained(renderer_wrapper_.get()), playback_rate_));
}

float PipelineImpl::GetVolume() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return volume_;
}

void PipelineImpl::SetVolume(float volume) {
  DVLOG(2) << __func__ << "(" << volume << ")";
  DCHECK(thread_checker_.CalledOnValidThread());

  if (volume < 0.0f || volume > 1.0f)
    return;

  volume_ = volume;
  media_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&RendererWrapper::SetVolume,
                 base::Unretained(renderer_wrapper_.get()), volume_));
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
                          const CdmAttachedCB& cdm_attached_cb) {
  DVLOG(2) << __func__;
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(cdm_context);
  DCHECK(cdm_attached_cb);

  media_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&RendererWrapper::SetCdm,
                 base::Unretained(renderer_wrapper_.get()), cdm_context,
                 media::BindToCurrentLoop(cdm_attached_cb)));
}

#define RETURN_STRING(state) \
  case state:                \
    return #state;

// static
const char* PipelineImpl::GetStateString(State state) {
  switch (state) {
    RETURN_STRING(kCreated);
    RETURN_STRING(kStarting);
    RETURN_STRING(kSeeking);
    RETURN_STRING(kPlaying);
    RETURN_STRING(kStopping);
    RETURN_STRING(kStopped);
    RETURN_STRING(kSuspending);
    RETURN_STRING(kSuspended);
    RETURN_STRING(kResuming);
  }
  NOTREACHED();
  return "INVALID";
}

#undef RETURN_STRING

void PipelineImpl::OnError(PipelineStatus error) {
  DVLOG(2) << __func__;
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_NE(PIPELINE_OK, error) << "PIPELINE_OK isn't an error!";
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

void PipelineImpl::OnEnded() {
  DVLOG(2) << __func__;
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(IsRunning());

  DCHECK(client_);
  client_->OnEnded();
}

void PipelineImpl::OnMetadata(PipelineMetadata metadata) {
  DVLOG(2) << __func__;
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(IsRunning());

  DCHECK(client_);
  client_->OnMetadata(metadata);
}

void PipelineImpl::OnBufferingStateChange(BufferingState state) {
  DVLOG(2) << __func__ << "(" << state << ")";
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(IsRunning());

  DCHECK(client_);
  client_->OnBufferingStateChange(state);
}

void PipelineImpl::OnDurationChange(base::TimeDelta duration) {
  DVLOG(2) << __func__;
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(IsRunning());

  duration_ = duration;

  DCHECK(client_);
  client_->OnDurationChange();
}

void PipelineImpl::OnWaitingForDecryptionKey() {
  DVLOG(2) << __func__;
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(IsRunning());

  DCHECK(client_);
  client_->OnWaitingForDecryptionKey();
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

void PipelineImpl::OnAudioDecoderChange(const std::string& name) {
  DVLOG(2) << __func__;
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(IsRunning());

  DCHECK(client_);
  client_->OnAudioDecoderChange(name);
}

void PipelineImpl::OnVideoDecoderChange(const std::string& name) {
  DVLOG(2) << __func__;
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(IsRunning());

  DCHECK(client_);
  client_->OnVideoDecoderChange(name);
}

void PipelineImpl::OnSeekDone(bool is_suspended) {
  DVLOG(3) << __func__;
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(IsRunning());

  seek_time_ = kNoTimestamp;
  is_suspended_ = is_suspended;

  DCHECK(seek_cb_);
  std::move(seek_cb_).Run(PIPELINE_OK);
}

void PipelineImpl::OnSuspendDone() {
  DVLOG(3) << __func__;
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(IsRunning());

  is_suspended_ = true;
  DCHECK(suspend_cb_);
  std::move(suspend_cb_).Run(PIPELINE_OK);
}

}  // namespace media
