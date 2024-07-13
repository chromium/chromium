// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/renderers/video_renderer_impl.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/default_tick_clock.h"
#include "base/trace_event/trace_event.h"
#include "media/base/media_log.h"
#include "media/base/media_switches.h"
#include "media/base/pipeline_status.h"
#include "media/base/renderer_client.h"
#include "media/base/video_frame.h"

namespace media {

namespace {

// Maximum number of frames we will buffer, regardless of their "effectiveness".
// See HaveReachedBufferingCap(). The value was historically described in terms
// of |min_buffered_frames_| as follows:
// = 3 * high_water_mark(min_buffered_frames_),
// = 3 * (2 * limits::kMaxVideoFrames)
// = 3 * 2 * 4
// Today, |min_buffered_frames_| can go down (as low as 1) and up in response to
// SetLatencyHint(), so we needed to peg this with a constant.
constexpr int kAbsoluteMaxFrames = 24;

bool ShouldUseLowDelayMode(DemuxerStream* stream) {
  return base::FeatureList::IsEnabled(kLowDelayVideoRenderingOnLiveStream) &&
         stream->liveness() == StreamLiveness::kLive;
}

}  // namespace

VideoRendererImpl::VideoRendererImpl(
    const scoped_refptr<base::SequencedTaskRunner>& media_task_runner,
    VideoRendererSink* sink,
    const CreateVideoDecodersCB& create_video_decoders_cb,
    bool drop_frames,
    MediaLog* media_log,
    std::unique_ptr<GpuMemoryBufferVideoFramePool> gmb_pool,
    MediaPlayerLoggingID media_player_id)
    : task_runner_(media_task_runner),
      sink_(sink),
      sink_started_(false),
      client_(nullptr),
      gpu_memory_buffer_pool_(std::move(gmb_pool)),
      media_log_(media_log),
      player_id_(media_player_id),
      low_delay_(false),
      received_end_of_stream_(false),
      rendered_end_of_stream_(false),
      state_(kUninitialized),
      create_video_decoders_cb_(create_video_decoders_cb),
      pending_read_(false),
      drop_frames_(drop_frames),
      buffering_state_(BUFFERING_HAVE_NOTHING),
      tick_clock_(base::DefaultTickClock::GetInstance()),
      was_background_rendering_(false),
      time_progressing_(false),
      have_renderered_frames_(false),
      last_frame_opaque_(false),
      painted_first_frame_(false),
      min_buffered_frames_(initial_buffering_size_.value()),
      max_buffered_frames_(initial_buffering_size_.value()) {
  DCHECK(create_video_decoders_cb_);
}

VideoRendererImpl::~VideoRendererImpl() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  if (init_cb_)
    FinishInitialization(PIPELINE_ERROR_ABORT);

  if (flush_cb_)
    FinishFlush();

  if (sink_started_)
    StopSink();
}

void VideoRendererImpl::Flush(base::OnceClosure callback) {
  DVLOG(1) << __func__;
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  if (sink_started_)
    StopSink();

  base::AutoLock auto_lock(lock_);

  DCHECK_EQ(state_, kPlaying);
  flush_cb_ = std::move(callback);
  state_ = kFlushing;

  if (buffering_state_ != BUFFERING_HAVE_NOTHING) {
    buffering_state_ = BUFFERING_HAVE_NOTHING;
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&VideoRendererImpl::OnBufferingStateChange,
                       weak_factory_.GetWeakPtr(), buffering_state_));
  }
  received_end_of_stream_ = false;
  rendered_end_of_stream_ = false;

  // Reset |video_decoder_stream_| and drop any pending read callbacks from it.
  pending_read_ = false;
  if (gpu_memory_buffer_pool_)
    gpu_memory_buffer_pool_->Abort();
  cancel_on_flush_weak_factory_.InvalidateWeakPtrs();
  paint_first_frame_cb_.Cancel();
  video_decoder_stream_->Reset(
      base::BindOnce(&VideoRendererImpl::OnVideoDecoderStreamResetDone,
                     weak_factory_.GetWeakPtr()));

  // To avoid unnecessary work by VDAs, only delete queued frames after
  // resetting |video_decoder_stream_|. If this is done in the opposite order
  // VDAs will get a bunch of ReusePictureBuffer() calls before the Reset(),
  // which they may use to output more frames that won't be used.
  algorithm_->Reset();
  painted_first_frame_ = false;

  // Reset preroll capacity so seek time is not penalized. |latency_hint_|
  // and |low_delay_| mode disable automatic preroll adjustments.
  if (!latency_hint_.has_value() && !low_delay_) {
    min_buffered_frames_ = max_buffered_frames_ =
        initial_buffering_size_.value();
  }
}

void VideoRendererImpl::StartPlayingFrom(base::TimeDelta timestamp) {
  DVLOG(1) << __func__ << "(" << timestamp.InMicroseconds() << ")";
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  base::AutoLock auto_lock(lock_);
  DCHECK_EQ(state_, kFlushed);
  DCHECK(!pending_read_);
  DCHECK_EQ(buffering_state_, BUFFERING_HAVE_NOTHING);

  state_ = kPlaying;
  start_timestamp_ = timestamp;
  painted_first_frame_ = false;
  last_render_time_ = last_frame_ready_time_ = base::TimeTicks();
  video_decoder_stream_->SkipPrepareUntil(start_timestamp_);
  AttemptRead_Locked();
}

void VideoRendererImpl::Initialize(
    DemuxerStream* stream,
    CdmContext* cdm_context,
    RendererClient* client,
    const TimeSource::WallClockTimeCB& wall_clock_time_cb,
    PipelineStatusCallback init_cb) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("media", "VideoRendererImpl::Initialize",
                                    TRACE_ID_LOCAL(this));

  base::AutoLock auto_lock(lock_);
  DCHECK(stream);
  DCHECK_EQ(stream->type(), DemuxerStream::VIDEO);
  DCHECK(init_cb);
  DCHECK(wall_clock_time_cb);
  DCHECK(kUninitialized == state_ || kFlushed == state_);
  DCHECK(!was_background_rendering_);
  DCHECK(!time_progressing_);

  demuxer_stream_ = stream;

  video_decoder_stream_ = std::make_unique<VideoDecoderStream>(
      std::make_unique<VideoDecoderStream::StreamTraits>(media_log_),
      task_runner_, create_video_decoders_cb_, media_log_);
  video_decoder_stream_->set_config_change_observer(base::BindRepeating(
      &VideoRendererImpl::OnConfigChange, weak_factory_.GetWeakPtr()));
  video_decoder_stream_->set_fallback_observer(base::BindRepeating(
      &VideoRendererImpl::OnFallback, weak_factory_.GetWeakPtr()));
  if (gpu_memory_buffer_pool_) {
    video_decoder_stream_->SetPrepareCB(base::BindRepeating(
        &GpuMemoryBufferVideoFramePool::MaybeCreateHardwareFrame,
        // Safe since VideoDecoderStream won't issue calls after destruction.
        base::Unretained(gpu_memory_buffer_pool_.get())));
  }

  low_delay_ = ShouldUseLowDelayMode(demuxer_stream_);
  if (low_delay_) {
    MEDIA_LOG(DEBUG, media_log_) << "Video rendering in low delay mode.";

    // "Low delay mode" means only one frame must be buffered to transition to
    // BUFFERING_HAVE_ENOUGH.
    min_buffered_frames_ = 1;
  }

  // Always post |init_cb_| because |this| could be destroyed if initialization
  // failed.
  init_cb_ = base::BindPostTaskToCurrentDefault(std::move(init_cb));

  client_ = client;
  wall_clock_time_cb_ = wall_clock_time_cb;
  state_ = kInitializing;

  current_decoder_config_ = demuxer_stream_->video_decoder_config();
  DCHECK(current_decoder_config_.IsValidConfig());

  video_decoder_stream_->Initialize(
      demuxer_stream_,
      base::BindOnce(&VideoRendererImpl::OnVideoDecoderStreamInitialized,
                     weak_factory_.GetWeakPtr()),
      cdm_context,
      base::BindRepeating(&VideoRendererImpl::OnStatisticsUpdate,
                          weak_factory_.GetWeakPtr()),
      base::BindRepeating(&VideoRendererImpl::OnWaiting,
                          weak_factory_.GetWeakPtr()));
}

scoped_refptr<VideoFrame> VideoRendererImpl::Render(
    base::TimeTicks deadline_min,
    base::TimeTicks deadline_max,
    RenderingMode rendering_mode) {
  TRACE_EVENT_BEGIN1("media", "VideoRendererImpl::Render", "id", player_id_);
  base::AutoLock auto_lock(lock_);
  DCHECK_EQ(state_, kPlaying);
  last_render_time_ = tick_clock_->NowTicks();

  size_t frames_dropped = 0;
  scoped_refptr<VideoFrame> result =
      algorithm_->Render(deadline_min, deadline_max, &frames_dropped);

  // Due to how the |algorithm_| holds frames, this should never be null if
  // we've had a proper startup sequence.
  DCHECK(result);

  const bool background_rendering =
      rendering_mode == RenderingMode::kBackground;

  // Declare HAVE_NOTHING if we reach a state where we can't progress playback
  // any further.  We don't want to do this if we've already done so, reached
  // end of stream, or have frames available.  We also don't want to do this in
  // background rendering mode, as the frames aren't visible anyways.
  MaybeFireEndedCallback_Locked(true);
  if (buffering_state_ == BUFFERING_HAVE_ENOUGH && !received_end_of_stream_ &&
      !algorithm_->effective_frames_queued() && !background_rendering &&
      !was_background_rendering_) {
    // Do not set |buffering_state_| here as the lock in FrameReady() may be
    // held already and it fire the state changes in the wrong order.
    DVLOG(3) << __func__ << " posted TransitionToHaveNothing.";
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&VideoRendererImpl::TransitionToHaveNothing,
                                  weak_factory_.GetWeakPtr()));
  }

  // We don't count dropped frames in the background to avoid skewing the count
  // and impacting JavaScript visible metrics used by web developers.
  //
  // Just after resuming from background rendering, we also don't count the
  // dropped frames since they are likely just dropped due to being too old.
  if (!background_rendering && !was_background_rendering_)
    stats_.video_frames_dropped += frames_dropped;
  was_background_rendering_ = background_rendering;

  // Always post this task, it will acquire new frames if necessary and since it
  // happens on another thread, even if we don't have room in the queue now, by
  // the time it runs (may be delayed up to 50ms for complex decodes!) we might.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VideoRendererImpl::AttemptReadAndCheckForMetadataChanges,
                     weak_factory_.GetWeakPtr(), result->format(),
                     result->natural_size()));

  TRACE_EVENT_END1("media", "VideoRendererImpl::Render", "frame",
                   result->AsHumanReadableString());
  return result;
}

void VideoRendererImpl::OnFrameDropped() {
  base::AutoLock auto_lock(lock_);
  algorithm_->OnLastFrameDropped();
}

base::TimeDelta VideoRendererImpl::GetPreferredRenderInterval() {
  base::AutoLock auto_lock(lock_);
  return algorithm_->average_frame_duration();
}

void VideoRendererImpl::OnVideoDecoderStreamInitialized(bool success) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  base::AutoLock auto_lock(lock_);
  DCHECK_EQ(state_, kInitializing);

  if (!success) {
    state_ = kUninitialized;
    FinishInitialization(DECODER_ERROR_NOT_SUPPORTED);
    return;
  }

  // We're all good! Consider ourselves flushed because we have not read any
  // frames yet.
  state_ = kFlushed;

  algorithm_ =
      std::make_unique<VideoRendererAlgorithm>(wall_clock_time_cb_, media_log_);
  if (!drop_frames_)
    algorithm_->disable_frame_dropping();

  FinishInitialization(PIPELINE_OK);
}

void VideoRendererImpl::FinishInitialization(PipelineStatus status) {
  DCHECK(init_cb_);
  TRACE_EVENT_NESTABLE_ASYNC_END1("media", "VideoRendererImpl::Initialize",
                                  TRACE_ID_LOCAL(this), "status",
                                  PipelineStatusToString(status));
  std::move(init_cb_).Run(status);
}

void VideoRendererImpl::FinishFlush() {
  DCHECK(flush_cb_);
  TRACE_EVENT_NESTABLE_ASYNC_END0("media", "VideoRendererImpl::Flush",
                                  TRACE_ID_LOCAL(this));
  std::move(flush_cb_).Run();
}

void VideoRendererImpl::OnPlaybackError(PipelineStatus error) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  client_->OnError(error);
}

void VideoRendererImpl::OnPlaybackEnded() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  {
    // Send one last stats update so things like memory usage are correct.
    base::AutoLock auto_lock(lock_);
    UpdateStats_Locked(true);
  }

  client_->OnEnded();
}

void VideoRendererImpl::OnStatisticsUpdate(const PipelineStatistics& stats) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  client_->OnStatisticsUpdate(stats);
}

void VideoRendererImpl::OnBufferingStateChange(BufferingState buffering_state) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  // "Underflow" is only possible when playing. This avoids noise like blaming
  // the decoder for an "underflow" that is really just a seek.
  BufferingStateChangeReason reason = BUFFERING_CHANGE_REASON_UNKNOWN;
  if (state_ == kPlaying && buffering_state == BUFFERING_HAVE_NOTHING) {
    reason = video_decoder_stream_->is_demuxer_read_pending()
                 ? DEMUXER_UNDERFLOW
                 : DECODER_UNDERFLOW;
  }

  media_log_->AddEvent<MediaLogEvent::kBufferingStateChanged>(
      SerializableBufferingState<SerializableBufferingStateType::kVideo>{
          buffering_state, reason});

  client_->OnBufferingStateChange(buffering_state, reason);
}

void VideoRendererImpl::OnWaiting(WaitingReason reason) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  client_->OnWaiting(reason);
}

void VideoRendererImpl::OnConfigChange(const VideoDecoderConfig& config) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(config.IsValidConfig());

  // RendererClient only cares to know about config changes that differ from
  // previous configs.
  if (!current_decoder_config_.Matches(config)) {
    current_decoder_config_ = config;
    client_->OnVideoConfigChange(config);
  }
}

void VideoRendererImpl::OnFallback(PipelineStatus status) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  client_->OnFallback(std::move(status).AddHere());
}

void VideoRendererImpl::SetTickClockForTesting(
    const base::TickClock* tick_clock) {
  tick_clock_ = tick_clock;
}

void VideoRendererImpl::OnTimeProgressing() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  // WARNING: Do not attempt to use |lock_| here as StartSink() may cause a
  // reentrant call.

  time_progressing_ = true;

  if (sink_started_)
    return;

  // If only an EOS frame came in after a seek, the renderer may not have
  // received the ended event yet though we've posted it.
  if (rendered_end_of_stream_)
    return;

  // If we have no frames queued, there is a pending buffering state change in
  // flight and we should ignore the start attempt.
  if (!algorithm_->frames_queued()) {
    DCHECK_EQ(buffering_state_, BUFFERING_HAVE_NOTHING);
    return;
  }

  StartSink();
}

void VideoRendererImpl::OnTimeStopped() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  // WARNING: Do not attempt to use |lock_| here as StopSink() may cause a
  // reentrant call.

  time_progressing_ = false;

  if (!sink_started_)
    return;

  StopSink();

  // Make sure we expire everything we can if we can't read any more currently,
  // otherwise playback may hang indefinitely.  Note: There are no effective
  // frames queued at this point, otherwise FrameReady() would have canceled
  // the underflow state before reaching this point.
  if (buffering_state_ == BUFFERING_HAVE_NOTHING) {
    base::AutoLock al(lock_);
    RemoveFramesForUnderflowOrBackgroundRendering();

    // If we've underflowed, increase the number of frames required to reach
    // BUFFERING_HAVE_ENOUGH upon resume; this will help prevent us from
    // repeatedly underflowing. Providing a |latency_hint_| or enabling
    // |low_delay_| mode disables automatic increases. In these cases the site
    // is expressing a desire to manually control/minimize the buffering
    // threshold for HAVE_ENOUGH.
    const size_t kMaxUnderflowGrowth = 2 * initial_buffering_size_.value();
    if (!latency_hint_.has_value() && !low_delay_) {
      DCHECK_EQ(min_buffered_frames_, max_buffered_frames_);

      if (min_buffered_frames_ < kMaxUnderflowGrowth) {
        min_buffered_frames_++;
        DVLOG(2) << __func__ << " Underflow! Increased min_buffered_frames_: "
                 << min_buffered_frames_;
      }
    }

    // Increase |max_buffered_frames_| irrespective of |latency_hint_| and
    // |low_delay_| mode. Unlike |min_buffered_frames_|, this does not affect
    // the buffering threshold for HAVE_ENOUGH. When max > min, the renderer can
    // buffer frames _beyond_ the HAVE_ENOUGH threshold (assuming decoder is
    // fast enough), which still helps reduce the likelihood of repeat
    // underflow.
    if (max_buffered_frames_ < kMaxUnderflowGrowth) {
      max_buffered_frames_++;
      DVLOG(2) << __func__ << " Underflow! Increased max_buffered_frames_: "
               << max_buffered_frames_;
    }
  }
}

void VideoRendererImpl::SetLatencyHint(
    std::optional<base::TimeDelta> latency_hint) {
  base::AutoLock auto_lock(lock_);

  latency_hint_ = latency_hint;

  // Permanently disable implicit |low_delay_| mode. Apps using latencyHint
  // are taking manual control of how buffering works. Unsetting the hint
  // will make rendering behave as if |low_delay_| were never set.
  low_delay_ = false;

  if (!latency_hint_.has_value()) {
    // Restore default values.
    // NOTE |initial_buffering_size_| the default max, not the max overall.
    min_buffered_frames_ = max_buffered_frames_ =
        initial_buffering_size_.value();
    MEDIA_LOG(DEBUG, media_log_)
        << "Video latency hint cleared. Default buffer size ("
        << min_buffered_frames_ << " frames) restored";
  } else if (latency_hint_->is_zero()) {
    // Zero is a special case implying the bare minimum buffering (1 frame).
    // We apply the hint here outside of UpdateLatencyHintBufferingCaps_Locked()
    // to avoid needless churn since the "bare minimum" buffering doesn't
    // fluctuate with changes to FPS.
    min_buffered_frames_ = 1;
    max_buffered_frames_ = initial_buffering_size_.value();
    MEDIA_LOG(DEBUG, media_log_)
        << "Video latency hint set:" << *latency_hint << ". "
        << "Effective buffering latency: 1 frame";
  } else {
    // Non-zero latency hints are set here. Update buffering caps immediately if
    // we already have an algorithm_. Otherwise, the update will be applied as
    // frames arrive and duration becomes known. The caps will be recalculated
    // for each frame in case |average_frame_druation| changes.
    // |is_latency_hint_media_logged_| ensures that we only MEDIA_LOG on the
    // first application of this hint.
    is_latency_hint_media_logged_ = false;
    if (algorithm_) {
      UpdateLatencyHintBufferingCaps_Locked(
          algorithm_->average_frame_duration());
    }
  }
}

void VideoRendererImpl::UpdateLatencyHintBufferingCaps_Locked(
    base::TimeDelta average_frame_duration) {
  lock_.AssertAcquired();

  // NOTE: this method may be called for every frame. Only perform trivial
  // tasks.

  // This method should only be called for non-zero latency hints. Zero is hard
  // coded to 1 frame inside SetLatencyHint().
  DCHECK(latency_hint_.has_value() && !latency_hint_->is_zero());

  // For hints > 0, we need |average_frame_duration| to determine how many
  // frames would yield the specified target latency. This method will be called
  // again as |average_frame_duration| changes.
  if (average_frame_duration.is_zero())
    return;

  int latency_hint_frames =
      base::ClampRound(*latency_hint_ / average_frame_duration);

  std::string clamp_string;
  if (latency_hint_frames > kAbsoluteMaxFrames) {
    min_buffered_frames_ = kAbsoluteMaxFrames;
    clamp_string = " (clamped to max)";
  } else if (latency_hint_frames < 1) {
    min_buffered_frames_ = 1;
    clamp_string = " (clamped to min)";
  } else {
    min_buffered_frames_ = latency_hint_frames;
  }

  // Use initial capacity limit if possible. Increase if needed.
  max_buffered_frames_ =
      std::max(min_buffered_frames_, initial_buffering_size_.value());

  if (!is_latency_hint_media_logged_) {
    is_latency_hint_media_logged_ = true;
    MEDIA_LOG(DEBUG, media_log_)
        << "Video latency hint set:" << *latency_hint_ << ". "
        << "Effective buffering latency:"
        << (min_buffered_frames_ * average_frame_duration) << clamp_string;
  }
}

void VideoRendererImpl::FrameReady(VideoDecoderStream::ReadResult result) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  base::AutoLock auto_lock(lock_);
  DCHECK_EQ(state_, kPlaying);
  CHECK(pending_read_);
  pending_read_ = false;

  // Can happen when demuxers are preparing for a new Seek().
  switch (result.code()) {
    case DecoderStatus::Codes::kOk:
      break;
    case DecoderStatus::Codes::kAborted:
      // TODO(liberato): This used to check specifically for the value
      // DEMUXER_READ_ABORTED, which was more specific than |kAborted|.
      // However, since it's a dcheck, this seems okay.
      return;
    default:
      // Anything other than `kOk` or `kAborted` is treated as an error.
      DCHECK(!result.has_value());

      PipelineStatus::Codes code =
          result.code() == DecoderStatus::Codes::kDisconnected
              ? PIPELINE_ERROR_DISCONNECTED
              : PIPELINE_ERROR_DECODE;
      PipelineStatus status = {code, std::move(result).error()};
      task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&VideoRendererImpl::OnPlaybackError,
                         weak_factory_.GetWeakPtr(), std::move(status)));
      return;
  }

  DCHECK(result.has_value());
  scoped_refptr<VideoFrame> frame = std::move(result).value();
  DCHECK(frame);

  last_frame_ready_time_ = tick_clock_->NowTicks();
  last_decoder_stream_avg_duration_ = video_decoder_stream_->AverageDuration();

  const bool is_eos = frame->metadata().end_of_stream;
  const bool is_before_start_time = !is_eos && IsBeforeStartTime(*frame);
  const bool cant_read = !video_decoder_stream_->CanReadWithoutStalling();
  const bool has_best_first_frame = !is_eos && HasBestFirstFrame(*frame);
  const auto format = frame->format();
  const auto natural_size = frame->natural_size();

  if (is_eos) {
    DCHECK(!received_end_of_stream_);
    received_end_of_stream_ = true;
    fps_estimator_.Reset();
    ReportFrameRateIfNeeded_Locked();
  } else if ((min_buffered_frames_ == 1 || cant_read) && is_before_start_time) {
    // Don't accumulate frames that are earlier than the start time if we
    // won't have a chance for a better frame, otherwise we could declare
    // HAVE_ENOUGH_DATA and start playback prematurely.
    fps_estimator_.Reset();
    ReportFrameRateIfNeeded_Locked();
    AttemptRead_Locked();
    return;
  } else {
    // If the sink hasn't been started, we still have time to release less
    // than ideal frames prior to startup.  We don't use IsBeforeStartTime()
    // here since it's based on a duration estimate and we can be exact here.
    if (!sink_started_ && frame->timestamp() <= start_timestamp_) {
      algorithm_->Reset();
      fps_estimator_.Reset();
      ReportFrameRateIfNeeded_Locked();
    }

    // Provide frame duration information so that even if we only have one frame
    // in the queue we can properly estimate duration. This allows the call to
    // RemoveFramesForUnderflowOrBackgroundRendering() below to actually expire
    // this frame if it's too far behind the current media time. Without this,
    // we may resume too soon after a track change in the low delay case.
    if (!frame->metadata().frame_duration.has_value())
      frame->metadata().frame_duration = last_decoder_stream_avg_duration_;

    AddReadyFrame_Locked(std::move(frame));
  }

  // Attempt to purge bad frames in case of underflow or backgrounding.
  RemoveFramesForUnderflowOrBackgroundRendering();

  // Paint the first frame if possible and necessary. Paint ahead of
  // HAVE_ENOUGH_DATA to ensure the user sees the frame as early as possible.
  // Paint before calling algorithm_->average_frame_duration(), as the call to
  // Render() will trigger internal duration updates.
  //
  // We want to paint the first frame under two conditions: Either (1) we have
  // enough frames to know it's definitely the first frame or (2) there may be
  // no more frames coming (sometimes unless we paint one of them).
  //
  // We have to check both effective_frames_queued() and |has_best_first_frame|
  // since prior to the clock starting effective_frames_queued() is a guess.
  //
  // NOTE: Do this before using algorithm_->average_frame_duration(). This
  // initial render will update the duration to be non-zero when provided by
  // frame metadata.
  if (!sink_started_ && !painted_first_frame_ && algorithm_->frames_queued()) {
    if (received_end_of_stream_ ||
        (algorithm_->effective_frames_queued() && has_best_first_frame)) {
      PaintFirstFrame_Locked();
    } else if (cant_read) {
      // `cant_read` isn't always reliable, so only paint after 250ms if we
      // haven't gotten anything better. This resets for each frame received. We
      // still kick off any metadata changes to avoid any layout shift though.
      CheckForMetadataChanges(format, natural_size);
      paint_first_frame_cb_.Reset(base::BindOnce(
          &VideoRendererImpl::PaintFirstFrame, base::Unretained(this)));
      task_runner_->PostDelayedTask(FROM_HERE, paint_first_frame_cb_.callback(),
                                    base::Milliseconds(250));
    }
  }

  // Update average frame duration.
  base::TimeDelta frame_duration = algorithm_->average_frame_duration();
  if (frame_duration != kNoTimestamp && frame_duration != base::Seconds(0)) {
    fps_estimator_.AddSample(frame_duration);
  } else {
    fps_estimator_.Reset();
  }
  ReportFrameRateIfNeeded_Locked();

  // Update any statistics since the last call.
  UpdateStats_Locked();

  // Update hint-driven buffering caps to use the latest average frame duration.
  // NOTE: Do this before updating the buffering state below, as it may affect
  // the outcome of HaveEnoughData_Locked().
  // TODO(chcunningham): Duration from |algorithm_| is affected by playback
  // rate. Consider using wall clock frame duration instead.
  if (latency_hint_.has_value() && !latency_hint_->is_zero())
    UpdateLatencyHintBufferingCaps_Locked(frame_duration);

  // Signal buffering state if we've met our conditions.
  if (buffering_state_ == BUFFERING_HAVE_NOTHING && HaveEnoughData_Locked())
    TransitionToHaveEnough_Locked();

  // We may have removed all frames above and have reached end of stream. This
  // must happen after the buffering state change has been signaled.
  MaybeFireEndedCallback_Locked(time_progressing_);

  // Always request more decoded video if we have capacity.
  AttemptRead_Locked();
}

bool VideoRendererImpl::HaveEnoughData_Locked() const {
  DCHECK_EQ(state_, kPlaying);
  lock_.AssertAcquired();

  if (received_end_of_stream_)
    return true;

  if (HaveReachedBufferingCap(min_buffered_frames_))
    return true;

  // If we've decoded any frames since the last render, signal have enough to
  // avoid underflowing when video is not visible unless we run out of frames.
  if (was_background_rendering_ && last_frame_ready_time_ >= last_render_time_)
    return true;

  if (min_buffered_frames_ > 1 &&
      video_decoder_stream_->CanReadWithoutStalling()) {
    return false;
  }

  // Note: We still require an effective frame in the stalling case since this
  // method is also used to inform TransitionToHaveNothing_Locked() and thus
  // would never pause and rebuffer if we always return true here.
  return algorithm_->effective_frames_queued() > 0u;
}

void VideoRendererImpl::TransitionToHaveEnough_Locked() {
  DVLOG(3) << __func__;
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK_EQ(buffering_state_, BUFFERING_HAVE_NOTHING);
  lock_.AssertAcquired();

  buffering_state_ = BUFFERING_HAVE_ENOUGH;
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VideoRendererImpl::OnBufferingStateChange,
                                weak_factory_.GetWeakPtr(), buffering_state_));
}

void VideoRendererImpl::TransitionToHaveNothing() {
  DVLOG(3) << __func__;
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  base::AutoLock auto_lock(lock_);
  TransitionToHaveNothing_Locked();
}

void VideoRendererImpl::TransitionToHaveNothing_Locked() {
  DVLOG(3) << __func__;
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  lock_.AssertAcquired();

  if (buffering_state_ != BUFFERING_HAVE_ENOUGH || HaveEnoughData_Locked())
    return;

  buffering_state_ = BUFFERING_HAVE_NOTHING;
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VideoRendererImpl::OnBufferingStateChange,
                                weak_factory_.GetWeakPtr(), buffering_state_));
}

void VideoRendererImpl::AddReadyFrame_Locked(scoped_refptr<VideoFrame> frame) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  lock_.AssertAcquired();
  DCHECK(!frame->metadata().end_of_stream);

  ++stats_.video_frames_decoded;

  if (frame->metadata().power_efficient)
    ++stats_.video_frames_decoded_power_efficient;

  algorithm_->EnqueueFrame(std::move(frame));
}

void VideoRendererImpl::AttemptRead_Locked() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  lock_.AssertAcquired();

  if (pending_read_ || received_end_of_stream_)
    return;

  if (HaveReachedBufferingCap(max_buffered_frames_))
    return;

  switch (state_) {
    case kPlaying:
      pending_read_ = true;
      video_decoder_stream_->Read(
          base::BindOnce(&VideoRendererImpl::FrameReady,
                         cancel_on_flush_weak_factory_.GetWeakPtr()));
      return;
    case kUninitialized:
    case kInitializing:
    case kFlushing:
    case kFlushed:
      return;
  }
}

void VideoRendererImpl::OnVideoDecoderStreamResetDone() {
  // We don't need to acquire the |lock_| here, because we can only get here
  // when Flush is in progress, so rendering and video sink must be stopped.
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(!sink_started_);
  DCHECK_EQ(kFlushing, state_);
  DCHECK(!received_end_of_stream_);
  DCHECK(!rendered_end_of_stream_);
  DCHECK_EQ(buffering_state_, BUFFERING_HAVE_NOTHING);

  state_ = kFlushed;
  FinishFlush();
}

void VideoRendererImpl::UpdateStats_Locked(bool force_update) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  lock_.AssertAcquired();

  // No need to check for `stats_.video_frames_decoded_power_efficient` because
  // if it is greater than 0, `stats_.video_frames_decoded` will too.
  if (!force_update && !stats_.video_frames_decoded &&
      !stats_.video_frames_dropped) {
    return;
  }

  if (stats_.video_frames_dropped) {
    TRACE_EVENT_INSTANT2("media", "VideoFramesDropped",
                         TRACE_EVENT_SCOPE_THREAD, "count",
                         stats_.video_frames_dropped, "id", player_id_);
  }

  const size_t memory_usage = algorithm_->GetMemoryUsage();
  stats_.video_memory_usage = memory_usage - stats_.video_memory_usage;
  stats_.video_frame_duration_average = algorithm_->average_frame_duration();
  OnStatisticsUpdate(stats_);

  stats_.video_frames_decoded = 0;
  stats_.video_frames_dropped = 0;
  stats_.video_frames_decoded_power_efficient = 0;
  stats_.video_memory_usage = memory_usage;
}

void VideoRendererImpl::ReportFrameRateIfNeeded_Locked() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  lock_.AssertAcquired();

  std::optional<int> current_fps = fps_estimator_.ComputeFPS();
  if (last_reported_fps_ && current_fps &&
      *last_reported_fps_ == *current_fps) {
    // Reported an FPS before, and it hasn't changed.
    return;
  } else if (!last_reported_fps_ && !current_fps) {
    // Did not report an FPS before, and we still don't have one
    return;
  }

  // FPS changed, possibly to unknown.
  last_reported_fps_ = current_fps;
  client_->OnVideoFrameRateChange(current_fps);
}

bool VideoRendererImpl::HaveReachedBufferingCap(size_t buffering_cap) const {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  // When the display rate is less than the frame rate, the effective frames
  // queued may be much smaller than the actual number of frames queued.  Here
  // we ensure that frames_queued() doesn't get excessive.
  return algorithm_->effective_frames_queued() >= buffering_cap ||
         algorithm_->frames_queued() >= kAbsoluteMaxFrames;
}

void VideoRendererImpl::StartSink() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK_GT(algorithm_->frames_queued(), 0u);
  sink_started_ = true;
  was_background_rendering_ = false;
  sink_->Start(this);
}

void VideoRendererImpl::StopSink() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  sink_->Stop();
  algorithm_->set_time_stopped();
  sink_started_ = false;
  was_background_rendering_ = false;
}

void VideoRendererImpl::MaybeFireEndedCallback_Locked(bool time_progressing) {
  lock_.AssertAcquired();

  // If there's only one frame in the video or Render() was never called, the
  // algorithm will have one frame linger indefinitely.  So in cases where the
  // frame duration is unknown and we've received EOS, fire it once we get down
  // to a single frame.

  // Don't fire ended if we haven't received EOS or have already done so.
  if (!received_end_of_stream_ || rendered_end_of_stream_)
    return;

  const bool have_frames_after_start_time =
      algorithm_->frames_queued() > 1 &&
      !IsBeforeStartTime(algorithm_->last_frame());

  // Don't fire ended if time isn't moving and we have frames.
  if (!time_progressing && have_frames_after_start_time)
    return;

  // Fire ended if we have no more effective frames, only ever had one frame, or
  // we only have 1 effective frame and there's less than one render interval
  // left before the ended event should execute.
  base::TimeDelta ended_event_delay;
  bool should_render_end_of_stream = false;
  if (!algorithm_->effective_frames_queued()) {
    // The best frame doesn't exist or was already rendered; end immediately.
    should_render_end_of_stream = true;
  } else if (algorithm_->frames_queued() == 1u &&
             (algorithm_->average_frame_duration().is_zero() ||
              algorithm_->render_interval().is_zero() || !time_progressing)) {
    // We'll end up here if playback never started or there was only one frame.
    should_render_end_of_stream = true;
  } else if (algorithm_->frames_queued() == 1u &&
             algorithm_->effective_frames_queued() == 1 && time_progressing) {
    const auto end_delay =
        std::max(base::TimeDelta(),
                 algorithm_->last_frame_end_time() - tick_clock_->NowTicks());

    // We should only be here if time is progressing, so only fire the ended
    // event now if we have less than one render interval before our next check.
    if (end_delay < algorithm_->render_interval()) {
      should_render_end_of_stream = true;
      ended_event_delay = end_delay;
    }
  }

  if (!should_render_end_of_stream)
    return;

  rendered_end_of_stream_ = true;
  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&VideoRendererImpl::OnPlaybackEnded,
                     cancel_on_flush_weak_factory_.GetWeakPtr()),
      ended_event_delay);
}

base::TimeTicks VideoRendererImpl::ConvertMediaTimestamp(
    base::TimeDelta media_time) {
  std::vector<base::TimeDelta> media_times(1, media_time);
  std::vector<base::TimeTicks> wall_clock_times;
  if (!wall_clock_time_cb_.Run(media_times, &wall_clock_times))
    return base::TimeTicks();
  return wall_clock_times[0];
}

base::TimeTicks VideoRendererImpl::GetCurrentMediaTimeAsWallClockTime() {
  std::vector<base::TimeTicks> current_time;
  wall_clock_time_cb_.Run(std::vector<base::TimeDelta>(), &current_time);
  return current_time[0];
}

bool VideoRendererImpl::IsBeforeStartTime(const VideoFrame& frame) {
  // Prefer the actual frame duration over the average if available.
  return frame.timestamp() + frame.metadata().frame_duration.value_or(
                                 last_decoder_stream_avg_duration_) <
         start_timestamp_;
}

bool VideoRendererImpl::HasBestFirstFrame(const VideoFrame& frame) {
  // We have the best first frame in the queue if our current frame has a
  // timestamp after `start_timestamp_` or straddles `start_timestamp_`.
  return frame.timestamp() >= start_timestamp_ ||
         frame.timestamp() + frame.metadata().frame_duration.value_or(
                                 last_decoder_stream_avg_duration_) >
             start_timestamp_;
}

void VideoRendererImpl::RemoveFramesForUnderflowOrBackgroundRendering() {
  // Nothing to do if frame dropping is disabled for testing or we have nothing.
  if (!drop_frames_ || !algorithm_->frames_queued())
    return;

  // If we're paused for prerolling (current time is 0), don't expire any
  // frames. It's possible that during preroll |have_nothing| is false while
  // |was_background_rendering_| is true. We differentiate this from actual
  // background rendering by checking if current time is 0.
  const base::TimeTicks current_time = GetCurrentMediaTimeAsWallClockTime();
  if (current_time.is_null())
    return;

  // Background rendering updates may not be ticking fast enough to remove
  // expired frames, so provide a boost here by ensuring we don't exit the
  // decoding cycle too early. Dropped frames are not counted in this case.
  if (was_background_rendering_) {
    algorithm_->RemoveExpiredFrames(tick_clock_->NowTicks());
    return;
  }

  // If we've paused for underflow, and still have no effective frames, clear
  // the entire queue.  Note: this may cause slight inaccuracies in the number
  // of dropped frames since the frame may have been rendered before.
  if (!sink_started_ && !algorithm_->effective_frames_queued()) {
    stats_.video_frames_dropped += algorithm_->frames_queued();
    algorithm_->Reset(
        VideoRendererAlgorithm::ResetFlag::kPreserveNextFrameEstimates);
    painted_first_frame_ = false;
    paint_first_frame_cb_.Cancel();

    // It's possible in the background rendering case for us to expire enough
    // frames that we need to transition from HAVE_ENOUGH => HAVE_NOTHING. Just
    // calling this function will check if we need to transition or not.
    if (buffering_state_ == BUFFERING_HAVE_ENOUGH)
      TransitionToHaveNothing_Locked();
    return;
  }

  // Use the current media wall clock time plus the frame duration since
  // RemoveExpiredFrames() is expecting the end point of an interval (it will
  // subtract from the given value). It's important to always call this so
  // that frame statistics are updated correctly.
  if (buffering_state_ == BUFFERING_HAVE_NOTHING) {
    stats_.video_frames_dropped += algorithm_->RemoveExpiredFrames(
        current_time + algorithm_->average_frame_duration());
    return;
  }

  // If we reach this point, the normal rendering process will take care of
  // removing any expired frames.
}

void VideoRendererImpl::CheckForMetadataChanges(VideoPixelFormat pixel_format,
                                                const gfx::Size& natural_size) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  // Notify client of size and opacity changes if this is the first frame
  // or if those have changed from the last frame.
  if (!have_renderered_frames_ || last_frame_natural_size_ != natural_size) {
    last_frame_natural_size_ = natural_size;
    client_->OnVideoNaturalSizeChange(last_frame_natural_size_);
  }

  const bool is_opaque = IsOpaque(pixel_format);
  if (!have_renderered_frames_ || last_frame_opaque_ != is_opaque) {
    last_frame_opaque_ = is_opaque;
    client_->OnVideoOpacityChange(last_frame_opaque_);
  }

  have_renderered_frames_ = true;
}

void VideoRendererImpl::AttemptReadAndCheckForMetadataChanges(
    VideoPixelFormat pixel_format,
    const gfx::Size& natural_size) {
  base::AutoLock auto_lock(lock_);
  CheckForMetadataChanges(pixel_format, natural_size);
  AttemptRead_Locked();
}

void VideoRendererImpl::PaintFirstFrame() {
  base::AutoLock auto_lock(lock_);
  PaintFirstFrame_Locked();
}

void VideoRendererImpl::PaintFirstFrame_Locked() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  lock_.AssertAcquired();

  if (painted_first_frame_ || sink_started_) {
    return;
  }

  DCHECK(algorithm_->frames_queued());

  auto first_frame =
      algorithm_->Render(base::TimeTicks(), base::TimeTicks(), nullptr);
  DCHECK(first_frame);
  sink_->PaintSingleFrame(first_frame);
  CheckForMetadataChanges(first_frame->format(), first_frame->natural_size());
  painted_first_frame_ = true;
  paint_first_frame_cb_.Cancel();
}

}  // namespace media
