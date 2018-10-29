// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/renderers/renderer_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/trace_event/trace_event.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/audio_renderer.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/media_log.h"
#include "media/base/media_resource.h"
#include "media/base/media_switches.h"
#include "media/base/renderer_client.h"
#include "media/base/time_source.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_renderer.h"
#include "media/base/wall_clock_time_source.h"

namespace media {

// See |video_underflow_threshold_|.
static const int kDefaultVideoUnderflowThresholdMs = 3000;

class RendererImpl::RendererClientInternal final : public RendererClient {
 public:
  RendererClientInternal(DemuxerStream::Type type, RendererImpl* renderer)
      : type_(type), renderer_(renderer) {
    DCHECK((type_ == DemuxerStream::AUDIO) || (type_ == DemuxerStream::VIDEO));
  }

  void OnError(PipelineStatus error) override { renderer_->OnError(error); }
  void OnEnded() override { renderer_->OnRendererEnded(type_); }
  void OnStatisticsUpdate(const PipelineStatistics& stats) override {
    renderer_->OnStatisticsUpdate(stats);
  }
  void OnBufferingStateChange(BufferingState state) override {
    renderer_->OnBufferingStateChange(type_, state);
  }
  void OnWaitingForDecryptionKey() override {
    renderer_->OnWaitingForDecryptionKey();
  }
  void OnAudioConfigChange(const AudioDecoderConfig& config) override {
    renderer_->OnAudioConfigChange(config);
  }
  void OnVideoConfigChange(const VideoDecoderConfig& config) override {
    renderer_->OnVideoConfigChange(config);
  }
  void OnVideoNaturalSizeChange(const gfx::Size& size) override {
    DCHECK(type_ == DemuxerStream::VIDEO);
    renderer_->OnVideoNaturalSizeChange(size);
  }
  void OnVideoOpacityChange(bool opaque) override {
    DCHECK(type_ == DemuxerStream::VIDEO);
    renderer_->OnVideoOpacityChange(opaque);
  }
  void OnDurationChange(base::TimeDelta duration) override {
    // RendererClients should only be notified of duration changes in certain
    // scenarios, none of which should arise for RendererClientInternal.
    // Duration changes should be sent to the pipeline by the DemuxerStream, via
    // the DemuxerHost interface.
    NOTREACHED();
  }

 private:
  DemuxerStream::Type type_;
  RendererImpl* renderer_;
};

RendererImpl::RendererImpl(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    std::unique_ptr<AudioRenderer> audio_renderer,
    std::unique_ptr<VideoRenderer> video_renderer)
    : state_(STATE_UNINITIALIZED),
      task_runner_(task_runner),
      audio_renderer_(std::move(audio_renderer)),
      video_renderer_(std::move(video_renderer)),
      current_audio_stream_(nullptr),
      current_video_stream_(nullptr),
      time_source_(NULL),
      time_ticking_(false),
      playback_rate_(0.0),
      audio_buffering_state_(BUFFERING_HAVE_NOTHING),
      video_buffering_state_(BUFFERING_HAVE_NOTHING),
      audio_ended_(false),
      video_ended_(false),
      audio_playing_(false),
      video_playing_(false),
      cdm_context_(nullptr),
      underflow_disabled_for_testing_(false),
      clockless_video_playback_enabled_for_testing_(false),
      video_underflow_threshold_(
          base::TimeDelta::FromMilliseconds(kDefaultVideoUnderflowThresholdMs)),
      pending_audio_track_change_(false),
      pending_video_track_change_(false),
      weak_factory_(this) {
  weak_this_ = weak_factory_.GetWeakPtr();
  DVLOG(1) << __func__;

  // TODO(dalecurtis): Remove once experiments for http://crbug.com/470940 are
  // complete.
  int threshold_ms = 0;
  std::string threshold_ms_str(
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kVideoUnderflowThresholdMs));
  if (base::StringToInt(threshold_ms_str, &threshold_ms) && threshold_ms > 0) {
    video_underflow_threshold_ =
        base::TimeDelta::FromMilliseconds(threshold_ms);
  }
}

RendererImpl::~RendererImpl() {
  DVLOG(1) << __func__;
  DCHECK(task_runner_->BelongsToCurrentThread());

  // RendererImpl is being destroyed, so invalidate weak pointers right away to
  // avoid getting callbacks which might try to access fields that has been
  // destroyed, e.g. audio_renderer_/video_renderer_ below (crbug.com/668963).
  weak_factory_.InvalidateWeakPtrs();

  // Tear down in opposite order of construction as |video_renderer_| can still
  // need |time_source_| (which can be |audio_renderer_|) to be alive.
  video_renderer_.reset();
  audio_renderer_.reset();

  if (init_cb_)
    FinishInitialization(PIPELINE_ERROR_ABORT);
  else if (flush_cb_)
    FinishFlush();
}

void RendererImpl::Initialize(MediaResource* media_resource,
                              RendererClient* client,
                              const PipelineStatusCB& init_cb) {
  DVLOG(1) << __func__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(state_, STATE_UNINITIALIZED);
  DCHECK(init_cb);
  DCHECK(client);
  TRACE_EVENT_ASYNC_BEGIN0("media", "RendererImpl::Initialize", this);

  client_ = client;
  media_resource_ = media_resource;
  init_cb_ = init_cb;

  if (HasEncryptedStream() && !cdm_context_) {
    DVLOG(1) << __func__ << ": Has encrypted stream but CDM is not set.";
    state_ = STATE_INIT_PENDING_CDM;
    return;
  }

  state_ = STATE_INITIALIZING;
  InitializeAudioRenderer();
}

void RendererImpl::SetCdm(CdmContext* cdm_context,
                          const CdmAttachedCB& cdm_attached_cb) {
  DVLOG(1) << __func__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(cdm_context);
  TRACE_EVENT0("media", "RendererImpl::SetCdm");

  if (cdm_context_) {
    DVLOG(1) << "Switching CDM not supported.";
    cdm_attached_cb.Run(false);
    return;
  }

  cdm_context_ = cdm_context;
  cdm_attached_cb.Run(true);

  if (state_ != STATE_INIT_PENDING_CDM)
    return;

  DCHECK(init_cb_);
  state_ = STATE_INITIALIZING;
  InitializeAudioRenderer();
}

void RendererImpl::Flush(const base::Closure& flush_cb) {
  DVLOG(1) << __func__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(!flush_cb_);
  DCHECK(!(pending_audio_track_change_ || pending_video_track_change_));
  TRACE_EVENT_ASYNC_BEGIN0("media", "RendererImpl::Flush", this);

  if (state_ == STATE_FLUSHED) {
    flush_cb_ = BindToCurrentLoop(flush_cb);
    FinishFlush();
    return;
  }

  if (state_ != STATE_PLAYING) {
    DCHECK_EQ(state_, STATE_ERROR);
    return;
  }

  flush_cb_ = flush_cb;
  state_ = STATE_FLUSHING;

  // If a stream restart is pending, this Flush() will complete it. Upon flush
  // completion any pending actions will be executed as well.
  FlushInternal();
}

void RendererImpl::StartPlayingFrom(base::TimeDelta time) {
  DVLOG(1) << __func__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  TRACE_EVENT1("media", "RendererImpl::StartPlayingFrom", "time_us",
               time.InMicroseconds());

  if (state_ != STATE_FLUSHED) {
    DCHECK_EQ(state_, STATE_ERROR);
    return;
  }

  time_source_->SetMediaTime(time);

  state_ = STATE_PLAYING;
  if (audio_renderer_) {
    audio_playing_ = true;
    audio_renderer_->StartPlaying();
  }
  if (video_renderer_) {
    video_playing_ = true;
    video_renderer_->StartPlayingFrom(time);
  }
}

void RendererImpl::SetPlaybackRate(double playback_rate) {
  DVLOG(1) << __func__ << "(" << playback_rate << ")";
  DCHECK(task_runner_->BelongsToCurrentThread());
  TRACE_EVENT1("media", "RendererImpl::SetPlaybackRate", "rate", playback_rate);

  // Playback rate changes are only carried out while playing.
  if (state_ != STATE_PLAYING && state_ != STATE_FLUSHED)
    return;

  time_source_->SetPlaybackRate(playback_rate);

  const double old_rate = playback_rate_;
  playback_rate_ = playback_rate;
  if (!time_ticking_ || !video_renderer_)
    return;

  if (old_rate == 0 && playback_rate > 0)
    video_renderer_->OnTimeProgressing();
  else if (old_rate > 0 && playback_rate == 0)
    video_renderer_->OnTimeStopped();
}

void RendererImpl::SetVolume(float volume) {
  DVLOG(1) << __func__;
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (audio_renderer_)
    audio_renderer_->SetVolume(volume);
}

base::TimeDelta RendererImpl::GetMediaTime() {
  // No BelongsToCurrentThread() checking because this can be called from other
  // threads.
  {
    base::AutoLock lock(restarting_audio_lock_);
    if (pending_audio_track_change_) {
      DCHECK_NE(kNoTimestamp, restarting_audio_time_);
      return restarting_audio_time_;
    }
  }

  return time_source_->CurrentMediaTime();
}

void RendererImpl::DisableUnderflowForTesting() {
  DVLOG(1) << __func__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(state_, STATE_UNINITIALIZED);

  underflow_disabled_for_testing_ = true;
}

void RendererImpl::EnableClocklessVideoPlaybackForTesting() {
  DVLOG(1) << __func__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(state_, STATE_UNINITIALIZED);
  DCHECK(underflow_disabled_for_testing_)
      << "Underflow must be disabled for clockless video playback";

  clockless_video_playback_enabled_for_testing_ = true;
}

bool RendererImpl::GetWallClockTimes(
    const std::vector<base::TimeDelta>& media_timestamps,
    std::vector<base::TimeTicks>* wall_clock_times) {
  // No BelongsToCurrentThread() checking because this can be called from other
  // threads.
  //
  // TODO(scherkus): Currently called from VideoRendererImpl's internal thread,
  // which should go away at some point http://crbug.com/110814
  if (clockless_video_playback_enabled_for_testing_) {
    if (media_timestamps.empty()) {
      *wall_clock_times = std::vector<base::TimeTicks>(1,
                                                       base::TimeTicks::Now());
    } else {
      *wall_clock_times = std::vector<base::TimeTicks>();
      for (auto const &media_time : media_timestamps) {
        wall_clock_times->push_back(base::TimeTicks() + media_time);
      }
    }
    return true;
  }

  return time_source_->GetWallClockTimes(media_timestamps, wall_clock_times);
}

bool RendererImpl::HasEncryptedStream() {
  std::vector<DemuxerStream*> demuxer_streams =
      media_resource_->GetAllStreams();

  for (auto* stream : demuxer_streams) {
    if (stream->type() == DemuxerStream::AUDIO &&
        stream->audio_decoder_config().is_encrypted())
      return true;
    if (stream->type() == DemuxerStream::VIDEO &&
        stream->video_decoder_config().is_encrypted())
      return true;
  }

  return false;
}

void RendererImpl::FinishInitialization(PipelineStatus status) {
  DCHECK(init_cb_);
  TRACE_EVENT_ASYNC_END1("media", "RendererImpl::Initialize", this, "status",
                         MediaLog::PipelineStatusToString(status));
  std::move(init_cb_).Run(status);
}

void RendererImpl::FinishFlush() {
  DCHECK(flush_cb_);
  TRACE_EVENT_ASYNC_END0("media", "RendererImpl::Flush", this);
  std::move(flush_cb_).Run();
}

void RendererImpl::InitializeAudioRenderer() {
  DVLOG(1) << __func__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(state_, STATE_INITIALIZING);
  DCHECK(init_cb_);

  PipelineStatusCB done_cb =
      base::Bind(&RendererImpl::OnAudioRendererInitializeDone, weak_this_);

  // TODO(servolk): Implement proper support for multiple streams. But for now
  // pick the first enabled stream to preserve the existing behavior.
  DemuxerStream* audio_stream =
      media_resource_->GetFirstStream(DemuxerStream::AUDIO);

  if (!audio_stream) {
    audio_renderer_.reset();
    task_runner_->PostTask(FROM_HERE, base::BindOnce(done_cb, PIPELINE_OK));
    return;
  }

  current_audio_stream_ = audio_stream;

  audio_renderer_client_.reset(
      new RendererClientInternal(DemuxerStream::AUDIO, this));
  // Note: After the initialization of a renderer, error events from it may
  // happen at any time and all future calls must guard against STATE_ERROR.
  audio_renderer_->Initialize(audio_stream, cdm_context_,
                              audio_renderer_client_.get(), done_cb);
}

void RendererImpl::OnAudioRendererInitializeDone(PipelineStatus status) {
  DVLOG(1) << __func__ << ": " << status;
  DCHECK(task_runner_->BelongsToCurrentThread());

  // OnError() may be fired at any time by the renderers, even if they thought
  // they initialized successfully (due to delayed output device setup).
  if (state_ != STATE_INITIALIZING) {
    DCHECK(!init_cb_);
    audio_renderer_.reset();
    return;
  }

  if (status != PIPELINE_OK) {
    FinishInitialization(status);
    return;
  }

  DCHECK(init_cb_);
  InitializeVideoRenderer();
}

void RendererImpl::InitializeVideoRenderer() {
  DVLOG(1) << __func__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(state_, STATE_INITIALIZING);
  DCHECK(init_cb_);

  PipelineStatusCB done_cb =
      base::Bind(&RendererImpl::OnVideoRendererInitializeDone, weak_this_);

  // TODO(servolk): Implement proper support for multiple streams. But for now
  // pick the first enabled stream to preserve the existing behavior.
  DemuxerStream* video_stream =
      media_resource_->GetFirstStream(DemuxerStream::VIDEO);

  if (!video_stream) {
    video_renderer_.reset();
    task_runner_->PostTask(FROM_HERE, base::BindOnce(done_cb, PIPELINE_OK));
    return;
  }

  current_video_stream_ = video_stream;

  video_renderer_client_.reset(
      new RendererClientInternal(DemuxerStream::VIDEO, this));
  video_renderer_->Initialize(
      video_stream, cdm_context_, video_renderer_client_.get(),
      base::Bind(&RendererImpl::GetWallClockTimes, base::Unretained(this)),
      done_cb);
}

void RendererImpl::OnVideoRendererInitializeDone(PipelineStatus status) {
  DVLOG(1) << __func__ << ": " << status;
  DCHECK(task_runner_->BelongsToCurrentThread());

  // OnError() may be fired at any time by the renderers, even if they thought
  // they initialized successfully (due to delayed output device setup).
  if (state_ != STATE_INITIALIZING) {
    DCHECK(!init_cb_);
    audio_renderer_.reset();
    video_renderer_.reset();
    return;
  }

  DCHECK(init_cb_);

  if (status != PIPELINE_OK) {
    FinishInitialization(status);
    return;
  }

  if (audio_renderer_) {
    time_source_ = audio_renderer_->GetTimeSource();
  } else if (!time_source_) {
    wall_clock_time_source_.reset(new WallClockTimeSource());
    time_source_ = wall_clock_time_source_.get();
  }

  state_ = STATE_FLUSHED;
  DCHECK(time_source_);
  DCHECK(audio_renderer_ || video_renderer_);

  FinishInitialization(PIPELINE_OK);
}

void RendererImpl::FlushInternal() {
  DVLOG(1) << __func__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(state_, STATE_FLUSHING);
  DCHECK(flush_cb_);

  if (time_ticking_)
    PausePlayback();

  FlushAudioRenderer();
}

// TODO(tmathmeyer) Combine this functionality with track switching flushing.
void RendererImpl::FlushAudioRenderer() {
  DVLOG(1) << __func__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(state_, STATE_FLUSHING);
  DCHECK(flush_cb_);

  if (!audio_renderer_ || !audio_playing_) {
    OnAudioRendererFlushDone();
  } else {
    audio_renderer_->Flush(base::BindRepeating(
        &RendererImpl::OnAudioRendererFlushDone, weak_this_));
  }
}

void RendererImpl::OnAudioRendererFlushDone() {
  DVLOG(1) << __func__;
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (state_ == STATE_ERROR) {
    DCHECK(!flush_cb_);
    return;
  }

  DCHECK_EQ(state_, STATE_FLUSHING);
  DCHECK(flush_cb_);

  // If we had a deferred video renderer underflow prior to the flush, it should
  // have been cleared by the audio renderer changing to BUFFERING_HAVE_NOTHING.
  DCHECK(deferred_video_underflow_cb_.IsCancelled());
  DCHECK_EQ(audio_buffering_state_, BUFFERING_HAVE_NOTHING);
  audio_ended_ = false;
  audio_playing_ = false;

  FlushVideoRenderer();
}

void RendererImpl::FlushVideoRenderer() {
  DVLOG(1) << __func__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(state_, STATE_FLUSHING);
  DCHECK(flush_cb_);

  if (!video_renderer_ || !video_playing_) {
    OnVideoRendererFlushDone();
  } else {
    video_renderer_->Flush(base::BindRepeating(
        &RendererImpl::OnVideoRendererFlushDone, weak_this_));
  }
}

void RendererImpl::OnVideoRendererFlushDone() {
  DVLOG(1) << __func__;
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (state_ == STATE_ERROR) {
    DCHECK(!flush_cb_);
    return;
  }

  DCHECK_EQ(state_, STATE_FLUSHING);
  DCHECK(flush_cb_);

  DCHECK_EQ(video_buffering_state_, BUFFERING_HAVE_NOTHING);
  video_ended_ = false;
  video_playing_ = false;
  state_ = STATE_FLUSHED;
  FinishFlush();
}

void RendererImpl::ReinitializeAudioRenderer(
    DemuxerStream* stream,
    base::TimeDelta time,
    base::OnceClosure reinitialize_completed_cb) {
  DVLOG(2) << __func__ << " stream=" << stream << " time=" << time.InSecondsF();
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_NE(stream, current_audio_stream_);

  current_audio_stream_ = stream;
  audio_renderer_->Initialize(
      stream, cdm_context_, audio_renderer_client_.get(),
      base::BindRepeating(&RendererImpl::OnAudioRendererReinitialized,
                          weak_this_, stream, time,
                          base::Passed(&reinitialize_completed_cb)));
}

void RendererImpl::OnAudioRendererReinitialized(
    DemuxerStream* stream,
    base::TimeDelta time,
    base::OnceClosure reinitialize_completed_cb,
    PipelineStatus status) {
  DVLOG(2) << __func__ << ": status=" << status;
  DCHECK_EQ(stream, current_audio_stream_);

  if (status != PIPELINE_OK) {
    std::move(reinitialize_completed_cb).Run();
    OnError(status);
    return;
  }
  RestartAudioRenderer(stream, time, std::move(reinitialize_completed_cb));
}

void RendererImpl::ReinitializeVideoRenderer(
    DemuxerStream* stream,
    base::TimeDelta time,
    base::OnceClosure reinitialize_completed_cb) {
  DVLOG(2) << __func__ << " stream=" << stream << " time=" << time.InSecondsF();
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_NE(stream, current_video_stream_);

  current_video_stream_ = stream;
  video_renderer_->OnTimeStopped();
  video_renderer_->Initialize(
      stream, cdm_context_, video_renderer_client_.get(),
      base::BindRepeating(&RendererImpl::GetWallClockTimes,
                          base::Unretained(this)),
      base::BindRepeating(&RendererImpl::OnVideoRendererReinitialized,
                          weak_this_, stream, time,
                          base::Passed(&reinitialize_completed_cb)));
}

void RendererImpl::OnVideoRendererReinitialized(
    DemuxerStream* stream,
    base::TimeDelta time,
    base::OnceClosure reinitialize_completed_cb,
    PipelineStatus status) {
  DVLOG(2) << __func__ << ": status=" << status;
  DCHECK_EQ(stream, current_video_stream_);

  if (status != PIPELINE_OK) {
    std::move(reinitialize_completed_cb).Run();
    OnError(status);
    return;
  }
  RestartVideoRenderer(stream, time, std::move(reinitialize_completed_cb));
}

void RendererImpl::RestartAudioRenderer(
    DemuxerStream* stream,
    base::TimeDelta time,
    base::OnceClosure restart_completed_cb) {
  DVLOG(2) << __func__ << " stream=" << stream << " time=" << time.InSecondsF();
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(audio_renderer_);
  DCHECK_EQ(stream, current_audio_stream_);
  DCHECK(state_ == STATE_PLAYING || state_ == STATE_FLUSHED ||
         state_ == STATE_FLUSHING);

  if (state_ == STATE_FLUSHED) {
    // If we are in the FLUSHED state, then we are done. The audio renderer will
    // be restarted by a subsequent RendererImpl::StartPlayingFrom call.
    std::move(restart_completed_cb).Run();
    return;
  }

  {
    base::AutoLock lock(restarting_audio_lock_);
    audio_playing_ = true;
    pending_audio_track_change_ = false;
  }
  audio_renderer_->StartPlaying();
  std::move(restart_completed_cb).Run();
}

void RendererImpl::RestartVideoRenderer(
    DemuxerStream* stream,
    base::TimeDelta time,
    base::OnceClosure restart_completed_cb) {
  DVLOG(2) << __func__ << " stream=" << stream << " time=" << time.InSecondsF();
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(video_renderer_);
  DCHECK_EQ(stream, current_video_stream_);
  DCHECK(state_ == STATE_PLAYING || state_ == STATE_FLUSHED ||
         state_ == STATE_FLUSHING);

  if (state_ == STATE_FLUSHED) {
    // If we are in the FLUSHED state, then we are done. The video renderer will
    // be restarted by a subsequent RendererImpl::StartPlayingFrom call.
    std::move(restart_completed_cb).Run();
    return;
  }

  video_playing_ = true;
  pending_video_track_change_ = false;
  video_renderer_->StartPlayingFrom(time);
  std::move(restart_completed_cb).Run();
}

void RendererImpl::OnStatisticsUpdate(const PipelineStatistics& stats) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  client_->OnStatisticsUpdate(stats);
}

void RendererImpl::OnBufferingStateChange(DemuxerStream::Type type,
                                          BufferingState new_buffering_state) {
  DCHECK((type == DemuxerStream::AUDIO) || (type == DemuxerStream::VIDEO));
  BufferingState* buffering_state = type == DemuxerStream::AUDIO
                                        ? &audio_buffering_state_
                                        : &video_buffering_state_;

  const auto* type_string = DemuxerStream::GetTypeName(type);
  DVLOG(1) << __func__ << " " << type_string << " "
           << MediaLog::BufferingStateToString(*buffering_state) << " -> "
           << MediaLog::BufferingStateToString(new_buffering_state);
  DCHECK(task_runner_->BelongsToCurrentThread());
  TRACE_EVENT2("media", "RendererImpl::OnBufferingStateChange", "type",
               type_string, "state",
               MediaLog::BufferingStateToString(new_buffering_state));

  bool was_waiting_for_enough_data = WaitingForEnoughData();

  if (new_buffering_state == BUFFERING_HAVE_NOTHING) {
    if ((pending_audio_track_change_ && type == DemuxerStream::AUDIO) ||
        (pending_video_track_change_ && type == DemuxerStream::VIDEO)) {
      // Don't pass up a nothing event if it was triggered by a track change.
      // This would cause the renderer to effectively lie about underflow state.
      // Even though this might cause an immediate video underflow due to
      // changing an audio track, all playing is paused when audio is disabled.
      *buffering_state = new_buffering_state;
      return;
    }
  }

  // When audio is present and has enough data, defer video underflow callbacks
  // for some time to avoid unnecessary glitches in audio; see
  // http://crbug.com/144683#c53.
  if (audio_renderer_ && type == DemuxerStream::VIDEO &&
      state_ == STATE_PLAYING) {
    if (video_buffering_state_ == BUFFERING_HAVE_ENOUGH &&
        audio_buffering_state_ == BUFFERING_HAVE_ENOUGH &&
        new_buffering_state == BUFFERING_HAVE_NOTHING &&
        deferred_video_underflow_cb_.IsCancelled()) {
      DVLOG(4) << __func__ << " Deferring HAVE_NOTHING for video stream.";
      deferred_video_underflow_cb_.Reset(
          base::Bind(&RendererImpl::OnBufferingStateChange, weak_this_, type,
                     new_buffering_state));
      task_runner_->PostDelayedTask(FROM_HERE,
                                    deferred_video_underflow_cb_.callback(),
                                    video_underflow_threshold_);
      return;
    }

    DVLOG(4) << "deferred_video_underflow_cb_.Cancel()";
    deferred_video_underflow_cb_.Cancel();
  } else if (!deferred_video_underflow_cb_.IsCancelled() &&
             type == DemuxerStream::AUDIO &&
             new_buffering_state == BUFFERING_HAVE_NOTHING) {
    // If audio underflows while we have a deferred video underflow in progress
    // we want to mark video as underflowed immediately and cancel the deferral.
    deferred_video_underflow_cb_.Cancel();
    video_buffering_state_ = BUFFERING_HAVE_NOTHING;
  }

  *buffering_state = new_buffering_state;

  // Disable underflow by ignoring updates that renderers have ran out of data.
  if (state_ == STATE_PLAYING && underflow_disabled_for_testing_ &&
      time_ticking_) {
    DVLOG(1) << "Update ignored because underflow is disabled for testing.";
    return;
  }

  // Renderer underflowed.
  if (!was_waiting_for_enough_data && WaitingForEnoughData()) {
    PausePlayback();
    client_->OnBufferingStateChange(BUFFERING_HAVE_NOTHING);
    return;
  }

  // Renderer prerolled.
  if (was_waiting_for_enough_data && !WaitingForEnoughData()) {
    // Prevent condition where audio or video is sputtering and flipping back
    // and forth between NOTHING and ENOUGH mixing with a track change, causing
    // a StartPlayback to be called while the audio renderer is being flushed.
    if (!pending_audio_track_change_ && !pending_video_track_change_) {
      StartPlayback();
      client_->OnBufferingStateChange(BUFFERING_HAVE_ENOUGH);
      return;
    }
  }
}

bool RendererImpl::WaitingForEnoughData() const {
  DCHECK(task_runner_->BelongsToCurrentThread());
  if (state_ != STATE_PLAYING)
    return false;
  if (audio_renderer_ && audio_buffering_state_ != BUFFERING_HAVE_ENOUGH)
    return true;
  if (video_renderer_ && video_buffering_state_ != BUFFERING_HAVE_ENOUGH)
    return true;
  return false;
}

void RendererImpl::PausePlayback() {
  DVLOG(1) << __func__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  TRACE_EVENT0("media", "RendererImpl::PausePlayback");

  switch (state_) {
    case STATE_PLAYING:
      DCHECK(PlaybackHasEnded() || WaitingForEnoughData() ||
             pending_audio_track_change_)
          << "Playback should only pause due to ending or underflowing or"
             " when restarting audio stream";

      break;

    case STATE_FLUSHING:
    case STATE_FLUSHED:
      // It's OK to pause playback when flushing.
      break;

    case STATE_UNINITIALIZED:
    case STATE_INIT_PENDING_CDM:
    case STATE_INITIALIZING:
      NOTREACHED() << "Invalid state: " << state_;
      break;

    case STATE_ERROR:
      // An error state may occur at any time.
      break;
  }
  if (time_ticking_) {
    time_ticking_ = false;
    time_source_->StopTicking();
  }

  if (playback_rate_ > 0 && video_renderer_)
    video_renderer_->OnTimeStopped();
}

void RendererImpl::StartPlayback() {
  DVLOG(1) << __func__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(state_, STATE_PLAYING);
  DCHECK(!WaitingForEnoughData());
  TRACE_EVENT0("media", "RendererImpl::StartPlayback");

  if (!time_ticking_) {
    time_ticking_ = true;
    audio_playing_ = true;
    time_source_->StartTicking();
  }
  if (playback_rate_ > 0 && video_renderer_) {
    video_playing_ = true;
    video_renderer_->OnTimeProgressing();
  }
}

void RendererImpl::OnRendererEnded(DemuxerStream::Type type) {
  const auto* type_string = DemuxerStream::GetTypeName(type);
  DVLOG(1) << __func__ << ": " << type_string;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK((type == DemuxerStream::AUDIO) || (type == DemuxerStream::VIDEO));
  TRACE_EVENT1("media", "RendererImpl::OnRendererEnded", "type", type_string);

  if (state_ != STATE_PLAYING)
    return;

  if (type == DemuxerStream::AUDIO) {
    // If all streams are ended, do not propagate a redundant ended event.
    if (audio_ended_ && PlaybackHasEnded())
      return;
    audio_ended_ = true;
  } else {
    DCHECK(video_renderer_);
    // If all streams are ended, do not propagate a redundant ended event.
    if (audio_ended_ && PlaybackHasEnded())
      return;
    video_ended_ = true;
    video_renderer_->OnTimeStopped();
  }

  RunEndedCallbackIfNeeded();
}

bool RendererImpl::PlaybackHasEnded() const {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (audio_renderer_ && !audio_ended_)
    return false;

  if (video_renderer_ && !video_ended_)
    return false;

  return true;
}

void RendererImpl::RunEndedCallbackIfNeeded() {
  DVLOG(1) << __func__;
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (!PlaybackHasEnded())
    return;

  if (time_ticking_)
    PausePlayback();

  client_->OnEnded();
}

void RendererImpl::OnError(PipelineStatus error) {
  DVLOG(1) << __func__ << "(" << error << ")";
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_NE(PIPELINE_OK, error) << "PIPELINE_OK isn't an error!";
  TRACE_EVENT1("media", "RendererImpl::OnError", "error",
               MediaLog::PipelineStatusToString(error));

  // An error has already been delivered.
  if (state_ == STATE_ERROR)
    return;

  const State old_state = state_;
  state_ = STATE_ERROR;

  if (init_cb_) {
    DCHECK(old_state == STATE_INITIALIZING ||
           old_state == STATE_INIT_PENDING_CDM);
    FinishInitialization(error);
    return;
  }

  // After OnError() returns, the pipeline may destroy |this|.
  client_->OnError(error);

  if (flush_cb_)
    FinishFlush();
}

void RendererImpl::OnWaitingForDecryptionKey() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  client_->OnWaitingForDecryptionKey();
}

void RendererImpl::OnAudioConfigChange(const AudioDecoderConfig& config) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  client_->OnAudioConfigChange(config);
}

void RendererImpl::OnVideoConfigChange(const VideoDecoderConfig& config) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  client_->OnVideoConfigChange(config);
}

void RendererImpl::OnVideoNaturalSizeChange(const gfx::Size& size) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  client_->OnVideoNaturalSizeChange(size);
}

void RendererImpl::OnVideoOpacityChange(bool opaque) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  client_->OnVideoOpacityChange(opaque);
}

void RendererImpl::CleanUpTrackChange(base::RepeatingClosure on_finished,
                                      bool* ended,
                                      bool* playing) {
  *playing = false;
  // If either stream is alive (i.e. hasn't reached ended state), ended can be
  // set to false. If both streams are dead, keep ended=true.
  if ((audio_renderer_ && !audio_ended_) || (video_renderer_ && !video_ended_))
    *ended = false;
  std::move(on_finished).Run();
}

void RendererImpl::OnSelectedVideoTracksChanged(
    const std::vector<DemuxerStream*>& enabled_tracks,
    base::OnceClosure change_completed_cb) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  TRACE_EVENT0("media", "RendererImpl::OnSelectedVideoTracksChanged");

  DCHECK_LT(enabled_tracks.size(), 2u);
  DemuxerStream* stream = enabled_tracks.empty() ? nullptr : enabled_tracks[0];

  if (!stream && !video_playing_) {
    std::move(change_completed_cb).Run();
    return;
  }

  // 'fixing' the stream -> restarting if its the same stream,
  //                        reinitializing if it is different.
  base::RepeatingClosure fix_stream_cb;
  if (stream && stream != current_video_stream_) {
    fix_stream_cb = base::BindRepeating(
        &RendererImpl::ReinitializeVideoRenderer, weak_this_, stream,
        GetMediaTime(), base::Passed(&change_completed_cb));
  } else {
    fix_stream_cb = base::BindRepeating(
        &RendererImpl::RestartVideoRenderer, weak_this_, current_video_stream_,
        GetMediaTime(), base::Passed(&change_completed_cb));
  }

  pending_video_track_change_ = true;
  video_renderer_->Flush(base::BindRepeating(
      &RendererImpl::CleanUpTrackChange, weak_this_,
      base::Passed(&fix_stream_cb), &video_ended_, &video_playing_));
}

void RendererImpl::OnEnabledAudioTracksChanged(
    const std::vector<DemuxerStream*>& enabled_tracks,
    base::OnceClosure change_completed_cb) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  TRACE_EVENT0("media", "RendererImpl::OnEnabledAudioTracksChanged");

  DCHECK_LT(enabled_tracks.size(), 2u);
  DemuxerStream* stream = enabled_tracks.empty() ? nullptr : enabled_tracks[0];

  if (!stream && !audio_playing_) {
    std::move(change_completed_cb).Run();
    return;
  }

  // 'fixing' the stream -> restarting if its the same stream,
  //                        reinitializing if it is different.
  base::RepeatingClosure fix_stream_cb;

  if (stream && stream != current_audio_stream_) {
    fix_stream_cb = base::BindRepeating(
        &RendererImpl::ReinitializeAudioRenderer, weak_this_, stream,
        GetMediaTime(), base::Passed(&change_completed_cb));
  } else {
    fix_stream_cb = base::BindRepeating(
        &RendererImpl::RestartAudioRenderer, weak_this_, current_audio_stream_,
        GetMediaTime(), base::Passed(&change_completed_cb));
  }

  {
    base::AutoLock lock(restarting_audio_lock_);
    pending_audio_track_change_ = true;
    restarting_audio_time_ = time_source_->CurrentMediaTime();
  }

  if (audio_playing_)
    PausePlayback();

  audio_renderer_->Flush(base::BindRepeating(
      &RendererImpl::CleanUpTrackChange, weak_this_,
      base::Passed(&fix_stream_cb), &audio_ended_, &audio_playing_));
}

}  // namespace media
