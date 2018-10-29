// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/renderers/video_renderer_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/feature_list.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_util.h"
#include "base/time/default_tick_clock.h"
#include "base/trace_event/trace_event.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/limits.h"
#include "media/base/media_log.h"
#include "media/base/media_switches.h"
#include "media/base/pipeline_status.h"
#include "media/base/renderer_client.h"
#include "media/base/video_frame.h"

namespace media {

namespace {

// Used for UMA stats, only add numbers to end!
enum VideoFrameColorSpaceUMA {
  Unknown = 0,
  UnknownRGB = 1,
  UnknownHDR = 2,
  REC601 = 3,
  REC709 = 4,
  JPEG = 5,
  PQ = 6,
  HLG = 7,
  SCRGB = 8,
  MAX = SCRGB
};

VideoFrameColorSpaceUMA ColorSpaceUMAHelper(
    const gfx::ColorSpace& color_space) {
  if (!color_space.IsHDR()) {
    if (color_space == gfx::ColorSpace::CreateREC709())
      return VideoFrameColorSpaceUMA::REC709;

    // TODO: Check for both PAL & NTSC rec601
    if (color_space == gfx::ColorSpace::CreateREC601())
      return VideoFrameColorSpaceUMA::REC601;

    if (color_space == gfx::ColorSpace::CreateJpeg())
      return VideoFrameColorSpaceUMA::JPEG;

    if (color_space == color_space.GetAsFullRangeRGB())
      return VideoFrameColorSpaceUMA::UnknownRGB;

    return VideoFrameColorSpaceUMA::Unknown;
  }

  if (color_space == gfx::ColorSpace(gfx::ColorSpace::PrimaryID::BT2020,
                                     gfx::ColorSpace::TransferID::SMPTEST2084,
                                     gfx::ColorSpace::MatrixID::BT709,
                                     gfx::ColorSpace::RangeID::LIMITED)) {
    return VideoFrameColorSpaceUMA::PQ;
  }

  if (color_space == gfx::ColorSpace(gfx::ColorSpace::PrimaryID::BT2020,
                                     gfx::ColorSpace::TransferID::SMPTEST2084,
                                     gfx::ColorSpace::MatrixID::BT2020_NCL,
                                     gfx::ColorSpace::RangeID::LIMITED)) {
    return VideoFrameColorSpaceUMA::PQ;
  }

  if (color_space == gfx::ColorSpace(gfx::ColorSpace::PrimaryID::BT2020,
                                     gfx::ColorSpace::TransferID::ARIB_STD_B67,
                                     gfx::ColorSpace::MatrixID::BT709,
                                     gfx::ColorSpace::RangeID::LIMITED)) {
    return VideoFrameColorSpaceUMA::HLG;
  }

  if (color_space == gfx::ColorSpace(gfx::ColorSpace::PrimaryID::BT2020,
                                     gfx::ColorSpace::TransferID::ARIB_STD_B67,
                                     gfx::ColorSpace::MatrixID::BT2020_NCL,
                                     gfx::ColorSpace::RangeID::LIMITED)) {
    return VideoFrameColorSpaceUMA::HLG;
  }

  if (color_space == gfx::ColorSpace::CreateSCRGBLinear())
    return VideoFrameColorSpaceUMA::SCRGB;

  return VideoFrameColorSpaceUMA::UnknownHDR;
}

bool ShouldUseLowDelayMode(DemuxerStream* stream) {
  return base::FeatureList::IsEnabled(kLowDelayVideoRenderingOnLiveStream) &&
         stream->liveness() == DemuxerStream::LIVENESS_LIVE;
}

}  // namespace

VideoRendererImpl::VideoRendererImpl(
    const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
    VideoRendererSink* sink,
    const CreateVideoDecodersCB& create_video_decoders_cb,
    bool drop_frames,
    MediaLog* media_log,
    std::unique_ptr<GpuMemoryBufferVideoFramePool> gmb_pool)
    : task_runner_(media_task_runner),
      sink_(sink),
      sink_started_(false),
      client_(nullptr),
      gpu_memory_buffer_pool_(std::move(gmb_pool)),
      media_log_(media_log),
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
      min_buffered_frames_(limits::kMaxVideoFrames),
      weak_factory_(this),
      frame_callback_weak_factory_(this) {
  DCHECK(create_video_decoders_cb_);
}

VideoRendererImpl::~VideoRendererImpl() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (init_cb_)
    FinishInitialization(PIPELINE_ERROR_ABORT);

  if (flush_cb_)
    FinishFlush();

  if (sink_started_)
    StopSink();
}

void VideoRendererImpl::Flush(const base::Closure& callback) {
  DVLOG(1) << __func__;
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (sink_started_)
    StopSink();

  base::AutoLock auto_lock(lock_);

  DCHECK_EQ(state_, kPlaying);
  flush_cb_ = callback;
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
  frame_callback_weak_factory_.InvalidateWeakPtrs();
  video_decoder_stream_->Reset(
      base::BindOnce(&VideoRendererImpl::OnVideoDecoderStreamResetDone,
                     weak_factory_.GetWeakPtr()));

  // To avoid unnecessary work by VDAs, only delete queued frames after
  // resetting |video_decoder_stream_|. If this is done in the opposite order
  // VDAs will get a bunch of ReusePictureBuffer() calls before the Reset(),
  // which they may use to output more frames that won't be used.
  algorithm_->Reset();
  painted_first_frame_ = false;

  // Reset preroll capacity so seek time is not penalized.
  min_buffered_frames_ = limits::kMaxVideoFrames;
}

void VideoRendererImpl::StartPlayingFrom(base::TimeDelta timestamp) {
  DVLOG(1) << __func__ << "(" << timestamp.InMicroseconds() << ")";
  DCHECK(task_runner_->BelongsToCurrentThread());
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
    const PipelineStatusCB& init_cb) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  TRACE_EVENT_ASYNC_BEGIN0("media", "VideoRendererImpl::Initialize", this);

  base::AutoLock auto_lock(lock_);
  DCHECK(stream);
  DCHECK_EQ(stream->type(), DemuxerStream::VIDEO);
  DCHECK(init_cb);
  DCHECK(wall_clock_time_cb);
  DCHECK(kUninitialized == state_ || kFlushed == state_);
  DCHECK(!was_background_rendering_);
  DCHECK(!time_progressing_);

  video_decoder_stream_.reset(new VideoDecoderStream(
      std::make_unique<VideoDecoderStream::StreamTraits>(media_log_),
      task_runner_, create_video_decoders_cb_, media_log_));
  video_decoder_stream_->set_config_change_observer(base::BindRepeating(
      &VideoRendererImpl::OnConfigChange, weak_factory_.GetWeakPtr()));
  if (gpu_memory_buffer_pool_) {
    video_decoder_stream_->SetPrepareCB(base::BindRepeating(
        &GpuMemoryBufferVideoFramePool::MaybeCreateHardwareFrame,
        // Safe since VideoDecoderStream won't issue calls after destruction.
        base::Unretained(gpu_memory_buffer_pool_.get())));
  }

  low_delay_ = ShouldUseLowDelayMode(stream);

  UMA_HISTOGRAM_BOOLEAN("Media.VideoRenderer.LowDelay", low_delay_);
  if (low_delay_)
    MEDIA_LOG(DEBUG, media_log_) << "Video rendering in low delay mode.";

  // Always post |init_cb_| because |this| could be destroyed if initialization
  // failed.
  init_cb_ = BindToCurrentLoop(init_cb);

  client_ = client;
  wall_clock_time_cb_ = wall_clock_time_cb;
  state_ = kInitializing;

  current_decoder_config_ = stream->video_decoder_config();
  DCHECK(current_decoder_config_.IsValidConfig());

  video_decoder_stream_->Initialize(
      stream,
      base::BindOnce(&VideoRendererImpl::OnVideoDecoderStreamInitialized,
                     weak_factory_.GetWeakPtr()),
      cdm_context,
      base::BindRepeating(&VideoRendererImpl::OnStatisticsUpdate,
                          weak_factory_.GetWeakPtr()),
      base::BindRepeating(&VideoRendererImpl::OnWaitingForDecryptionKey,
                          weak_factory_.GetWeakPtr()));
}

scoped_refptr<VideoFrame> VideoRendererImpl::Render(
    base::TimeTicks deadline_min,
    base::TimeTicks deadline_max,
    bool background_rendering) {
  TRACE_EVENT1("media", "VideoRendererImpl::Render", "id", media_log_->id());
  base::AutoLock auto_lock(lock_);
  DCHECK_EQ(state_, kPlaying);
  last_render_time_ = tick_clock_->NowTicks();

  size_t frames_dropped = 0;
  scoped_refptr<VideoFrame> result =
      algorithm_->Render(deadline_min, deadline_max, &frames_dropped);

  // Due to how the |algorithm_| holds frames, this should never be null if
  // we've had a proper startup sequence.
  DCHECK(result);

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

  return result;
}

void VideoRendererImpl::OnFrameDropped() {
  base::AutoLock auto_lock(lock_);
  algorithm_->OnLastFrameDropped();
}

void VideoRendererImpl::OnVideoDecoderStreamInitialized(bool success) {
  DCHECK(task_runner_->BelongsToCurrentThread());
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

  algorithm_.reset(new VideoRendererAlgorithm(wall_clock_time_cb_, media_log_));
  if (!drop_frames_)
    algorithm_->disable_frame_dropping();

  FinishInitialization(PIPELINE_OK);
}

void VideoRendererImpl::FinishInitialization(PipelineStatus status) {
  DCHECK(init_cb_);
  TRACE_EVENT_ASYNC_END1("media", "VideoRendererImpl::Initialize", this,
                         "status", MediaLog::PipelineStatusToString(status));
  std::move(init_cb_).Run(status);
}

void VideoRendererImpl::FinishFlush() {
  DCHECK(flush_cb_);
  TRACE_EVENT_ASYNC_END0("media", "VideoRendererImpl::Flush", this);
  std::move(flush_cb_).Run();
}

void VideoRendererImpl::OnPlaybackError(PipelineStatus error) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  client_->OnError(error);
}

void VideoRendererImpl::OnPlaybackEnded() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  {
    // Send one last stats update so things like memory usage are correct.
    base::AutoLock auto_lock(lock_);
    UpdateStats_Locked(true);
  }

  client_->OnEnded();
}

void VideoRendererImpl::OnStatisticsUpdate(const PipelineStatistics& stats) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  client_->OnStatisticsUpdate(stats);
}

void VideoRendererImpl::OnBufferingStateChange(BufferingState state) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  media_log_->AddEvent(media_log_->CreateBufferingStateChangedEvent(
      "video_buffering_state", state));
  client_->OnBufferingStateChange(state);
}

void VideoRendererImpl::OnWaitingForDecryptionKey() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  client_->OnWaitingForDecryptionKey();
}

void VideoRendererImpl::OnConfigChange(const VideoDecoderConfig& config) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(config.IsValidConfig());

  // RendererClient only cares to know about config changes that differ from
  // previous configs.
  if (!current_decoder_config_.Matches(config)) {
    current_decoder_config_ = config;
    client_->OnVideoConfigChange(config);
  }
}

void VideoRendererImpl::SetTickClockForTesting(
    const base::TickClock* tick_clock) {
  tick_clock_ = tick_clock;
}

void VideoRendererImpl::OnTimeProgressing() {
  DCHECK(task_runner_->BelongsToCurrentThread());

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
  DCHECK(task_runner_->BelongsToCurrentThread());

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
    // repeatedly underflowing.
    const size_t kMaxBufferedFrames = 2 * limits::kMaxVideoFrames;
    if (min_buffered_frames_ < kMaxBufferedFrames) {
      ++min_buffered_frames_;
      DVLOG(2) << "Increased min buffered frames to " << min_buffered_frames_;
    }
  }
}

void VideoRendererImpl::FrameReady(VideoDecoderStream::Status status,
                                   const scoped_refptr<VideoFrame>& frame) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  base::AutoLock auto_lock(lock_);
  DCHECK_EQ(state_, kPlaying);
  CHECK(pending_read_);
  pending_read_ = false;

  if (status == VideoDecoderStream::DECODE_ERROR) {
    DCHECK(!frame);
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&VideoRendererImpl::OnPlaybackError,
                       weak_factory_.GetWeakPtr(), PIPELINE_ERROR_DECODE));
    return;
  }

  // Can happen when demuxers are preparing for a new Seek().
  if (!frame) {
    DCHECK_EQ(status, VideoDecoderStream::DEMUXER_READ_ABORTED);
    return;
  }

  last_frame_ready_time_ = tick_clock_->NowTicks();

  UMA_HISTOGRAM_ENUMERATION("Media.VideoFrame.ColorSpace",
                            ColorSpaceUMAHelper(frame->ColorSpace()),
                            static_cast<int>(VideoFrameColorSpaceUMA::MAX) + 1);
  const bool is_eos =
      frame->metadata()->IsTrue(VideoFrameMetadata::END_OF_STREAM);
  const bool is_before_start_time =
      !is_eos && IsBeforeStartTime(frame->timestamp());
  const bool cant_read = !video_decoder_stream_->CanReadWithoutStalling();

  if (is_eos) {
    DCHECK(!received_end_of_stream_);
    received_end_of_stream_ = true;
  } else if ((low_delay_ || cant_read) && is_before_start_time) {
    // Don't accumulate frames that are earlier than the start time if we
    // won't have a chance for a better frame, otherwise we could declare
    // HAVE_ENOUGH_DATA and start playback prematurely.
    AttemptRead_Locked();
    return;
  } else {
    // If the sink hasn't been started, we still have time to release less
    // than ideal frames prior to startup.  We don't use IsBeforeStartTime()
    // here since it's based on a duration estimate and we can be exact here.
    if (!sink_started_ && frame->timestamp() <= start_timestamp_)
      algorithm_->Reset();

    // Provide frame duration information so that even if we only have one frame
    // in the queue we can properly estimate duration. This allows the call to
    // RemoveFramesForUnderflowOrBackgroundRendering() below to actually expire
    // this frame if it's too far behind the current media time. Without this,
    // we may resume too soon after a track change in the low delay case.
    if (!frame->metadata()->HasKey(VideoFrameMetadata::FRAME_DURATION)) {
      frame->metadata()->SetTimeDelta(VideoFrameMetadata::FRAME_DURATION,
                                      video_decoder_stream_->AverageDuration());
    }

    AddReadyFrame_Locked(frame);
  }

  // Attempt to purge bad frames in case of underflow or backgrounding.
  RemoveFramesForUnderflowOrBackgroundRendering();

  // We may have removed all frames above and have reached end of stream.
  MaybeFireEndedCallback_Locked(time_progressing_);

  // Update any statistics since the last call.
  UpdateStats_Locked();

  // Paint the first frame if possible and necessary. Paint ahead of
  // HAVE_ENOUGH_DATA to ensure the user sees the frame as early as possible.
  //
  // We want to paint the first frame under two conditions: Either (1) we have
  // enough frames to know it's definitely the first frame or (2) there may be
  // no more frames coming (sometimes unless we paint one of them).
  //
  // We have to check both effective_frames_queued() and |is_before_start_time|
  // since prior to the clock starting effective_frames_queued() is a guess.
  if (!sink_started_ && !painted_first_frame_ && algorithm_->frames_queued() &&
      (received_end_of_stream_ || cant_read ||
       (algorithm_->effective_frames_queued() && !is_before_start_time))) {
    scoped_refptr<VideoFrame> first_frame =
        algorithm_->Render(base::TimeTicks(), base::TimeTicks(), nullptr);
    CheckForMetadataChanges(first_frame->format(), first_frame->natural_size());
    sink_->PaintSingleFrame(first_frame);
    painted_first_frame_ = true;
  }

  // Signal buffering state if we've met our conditions.
  if (buffering_state_ == BUFFERING_HAVE_NOTHING && HaveEnoughData_Locked())
    TransitionToHaveEnough_Locked();

  // Always request more decoded video if we have capacity.
  AttemptRead_Locked();
}

bool VideoRendererImpl::HaveEnoughData_Locked() const {
  DCHECK_EQ(state_, kPlaying);
  lock_.AssertAcquired();

  if (received_end_of_stream_)
    return true;

  if (HaveReachedBufferingCap())
    return true;

  // If we've decoded any frames since the last render, signal have enough to
  // avoid underflowing when video is not visible unless we run out of frames.
  if (was_background_rendering_ && last_frame_ready_time_ >= last_render_time_)
    return true;

  if (!low_delay_ && video_decoder_stream_->CanReadWithoutStalling())
    return false;

  // Note: We still require an effective frame in the stalling case since this
  // method is also used to inform TransitionToHaveNothing_Locked() and thus
  // would never pause and rebuffer if we always return true here.
  return algorithm_->effective_frames_queued() > 0u;
}

void VideoRendererImpl::TransitionToHaveEnough_Locked() {
  DVLOG(3) << __func__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(buffering_state_, BUFFERING_HAVE_NOTHING);
  lock_.AssertAcquired();

  buffering_state_ = BUFFERING_HAVE_ENOUGH;
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VideoRendererImpl::OnBufferingStateChange,
                                weak_factory_.GetWeakPtr(), buffering_state_));
}

void VideoRendererImpl::TransitionToHaveNothing() {
  DVLOG(3) << __func__;
  DCHECK(task_runner_->BelongsToCurrentThread());

  base::AutoLock auto_lock(lock_);
  TransitionToHaveNothing_Locked();
}

void VideoRendererImpl::TransitionToHaveNothing_Locked() {
  DVLOG(3) << __func__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  lock_.AssertAcquired();

  if (buffering_state_ != BUFFERING_HAVE_ENOUGH || HaveEnoughData_Locked())
    return;

  buffering_state_ = BUFFERING_HAVE_NOTHING;
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VideoRendererImpl::OnBufferingStateChange,
                                weak_factory_.GetWeakPtr(), buffering_state_));
}

void VideoRendererImpl::AddReadyFrame_Locked(
    const scoped_refptr<VideoFrame>& frame) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  lock_.AssertAcquired();
  DCHECK(!frame->metadata()->IsTrue(VideoFrameMetadata::END_OF_STREAM));

  ++stats_.video_frames_decoded;

  bool power_efficient = false;
  if (frame->metadata()->GetBoolean(VideoFrameMetadata::POWER_EFFICIENT,
                                    &power_efficient) &&
      power_efficient) {
    ++stats_.video_frames_decoded_power_efficient;
  }

  algorithm_->EnqueueFrame(frame);
}

void VideoRendererImpl::AttemptRead_Locked() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  lock_.AssertAcquired();

  if (pending_read_ || received_end_of_stream_)
    return;

  if (HaveReachedBufferingCap())
    return;

  switch (state_) {
    case kPlaying:
      pending_read_ = true;
      video_decoder_stream_->Read(
          base::BindOnce(&VideoRendererImpl::FrameReady,
                         frame_callback_weak_factory_.GetWeakPtr()));
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
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(!sink_started_);
  DCHECK_EQ(kFlushing, state_);
  DCHECK(!received_end_of_stream_);
  DCHECK(!rendered_end_of_stream_);
  DCHECK_EQ(buffering_state_, BUFFERING_HAVE_NOTHING);

  state_ = kFlushed;
  FinishFlush();
}

void VideoRendererImpl::UpdateStats_Locked(bool force_update) {
  DCHECK(task_runner_->BelongsToCurrentThread());
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
                         stats_.video_frames_dropped, "id", media_log_->id());
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

bool VideoRendererImpl::HaveReachedBufferingCap() const {
  DCHECK(task_runner_->BelongsToCurrentThread());

  // When the display rate is less than the frame rate, the effective frames
  // queued may be much smaller than the actual number of frames queued.  Here
  // we ensure that frames_queued() doesn't get excessive.
  return algorithm_->effective_frames_queued() >= min_buffered_frames_ ||
         algorithm_->frames_queued() >= 3 * min_buffered_frames_;
}

void VideoRendererImpl::StartSink() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_GT(algorithm_->frames_queued(), 0u);
  sink_started_ = true;
  was_background_rendering_ = false;
  sink_->Start(this);
}

void VideoRendererImpl::StopSink() {
  DCHECK(task_runner_->BelongsToCurrentThread());
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

  // Don't fire ended if time isn't moving and we have frames.
  if (!time_progressing && algorithm_->frames_queued())
    return;

  // Fire ended if we have no more effective frames or only ever had one frame.
  if (!algorithm_->effective_frames_queued() ||
      (algorithm_->frames_queued() == 1u &&
       algorithm_->average_frame_duration().is_zero())) {
    rendered_end_of_stream_ = true;
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(&VideoRendererImpl::OnPlaybackEnded,
                                          weak_factory_.GetWeakPtr()));
  }
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

bool VideoRendererImpl::IsBeforeStartTime(base::TimeDelta timestamp) {
  return timestamp + video_decoder_stream_->AverageDuration() <
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
  DCHECK(task_runner_->BelongsToCurrentThread());

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

}  // namespace media
