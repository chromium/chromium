// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/remoting/courier_renderer.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/safe_math.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "media/base/buffering_state.h"
#include "media/base/media_resource.h"
#include "media/base/renderer_client.h"
#include "media/base/video_renderer_sink.h"
#include "media/base/waiting.h"
#include "media/cast/openscreen/remoting_proto_enum_utils.h"
#include "media/cast/openscreen/remoting_proto_utils.h"
#include "media/remoting/demuxer_stream_adapter.h"
#include "media/remoting/renderer_controller.h"

using openscreen::cast::RpcMessenger;

namespace media {
namespace remoting {

namespace {

// The moving time window to track the media time and statistics updates.
constexpr base::TimeDelta kTrackingWindow = base::Seconds(5);

// The allowed delay for the remoting playback. When continuously exceeds this
// limit for |kPlaybackDelayCountThreshold| times, the user experience is likely
// poor and the controller is notified.
constexpr base::TimeDelta kMediaPlaybackDelayThreshold =
    base::Milliseconds(750);
constexpr int kPlaybackDelayCountThreshold = 10;

// The allowed percentage of the number of video frames dropped vs. the number
// of the video frames decoded. When exceeds this limit, the user experience is
// likely poor and the controller is notified.
constexpr int kMaxNumVideoFramesDroppedPercentage = 3;

// The time period to allow receiver get stable after playback rate change or
// Flush().
constexpr base::TimeDelta kStabilizationPeriod = base::Seconds(2);

// The amount of time between polling the DemuxerStreamAdapters to measure their
// data flow rates for metrics.
constexpr base::TimeDelta kDataFlowPollPeriod = base::Seconds(10);

// base::Bind* doesn't understand openscreen::WeakPtr, so we must manually
// check the RpcMessenger pointer before calling into it.
void RegisterForRpcTask(
    openscreen::WeakPtr<openscreen::cast::RpcMessenger> rpc_messenger,
    int rpc_handle,
    openscreen::cast::RpcMessenger::ReceiveMessageCallback message_cb) {
  if (rpc_messenger) {
    rpc_messenger->RegisterMessageReceiverCallback(rpc_handle,
                                                   std::move(message_cb));
  }
}
void DeregisterFromRpcTask(
    openscreen::WeakPtr<openscreen::cast::RpcMessenger> rpc_messenger,
    int rpc_handle) {
  if (rpc_messenger) {
    rpc_messenger->UnregisterMessageReceiverCallback(rpc_handle);
  }
}

}  // namespace

CourierRenderer::CourierRenderer(
    scoped_refptr<base::SequencedTaskRunner> media_task_runner,
    const base::WeakPtr<RendererController>& controller,
    VideoRendererSink* video_renderer_sink)
    : state_(STATE_UNINITIALIZED),
      main_task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()),
      media_task_runner_(std::move(media_task_runner)),
      media_resource_(nullptr),
      client_(nullptr),
      controller_(controller),
      rpc_messenger_(controller_->GetRpcMessenger()),
      rpc_handle_(rpc_messenger_->GetUniqueHandle()),
      remote_renderer_handle_(RpcMessenger::kInvalidHandle),
      video_renderer_sink_(video_renderer_sink),
      clock_(base::DefaultTickClock::GetInstance()) {
  // Note: The constructor is running on the main thread, but will be destroyed
  // on the media thread. Therefore, all weak pointers must be dereferenced on
  // the media thread.
  media_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&CourierRenderer::RegisterForRpcMessaging,
                                weak_factory_.GetWeakPtr()));
}

CourierRenderer::~CourierRenderer() {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());

  DeregisterFromRpcMessaging();
  if (video_renderer_sink_) {
    video_renderer_sink_->PaintSingleFrame(
        VideoFrame::CreateBlackFrame(gfx::Size(1280, 720)));
  }
}

void CourierRenderer::Initialize(MediaResource* media_resource,
                                 RendererClient* client,
                                 PipelineStatusCallback init_cb) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(media_resource);
  DCHECK(client);

  if (state_ != STATE_UNINITIALIZED) {
    media_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(init_cb), PIPELINE_ERROR_INVALID_STATE));
    return;
  }

  media_resource_ = media_resource;
  client_ = client;
  init_workflow_done_callback_ = std::move(init_cb);

  state_ = STATE_CREATE_PIPE;

  // TODO(servolk): Add support for multiple streams. For now use the first
  // enabled audio and video streams to preserve the existing behavior.
  ::media::DemuxerStream* audio_demuxer_stream =
      media_resource_->GetFirstStream(DemuxerStream::AUDIO);
  ::media::DemuxerStream* video_demuxer_stream =
      media_resource_->GetFirstStream(DemuxerStream::VIDEO);

  // Establish remoting data pipe connection using main thread.
  uint32_t data_pipe_capacity =
      DemuxerStreamAdapter::kMojoDataPipeCapacityInBytes;
  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &RendererController::StartDataPipe, controller_, data_pipe_capacity,
          audio_demuxer_stream, video_demuxer_stream,
          base::BindOnce(&CourierRenderer::OnDataPipeCreatedOnMainThread,
                         media_task_runner_, weak_factory_.GetWeakPtr(),
                         rpc_messenger_)));
}

void CourierRenderer::SetLatencyHint(
    std::optional<base::TimeDelta> latency_hint) {}

void CourierRenderer::Flush(base::OnceClosure flush_cb) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(!flush_cb_);

  if (state_ != STATE_PLAYING) {
    DCHECK_EQ(state_, STATE_ERROR);
    // In the error state, this renderer will be shut down shortly. To prevent
    // breaking the pipeline impl, just run the done callback (interface
    // requirement).
    media_task_runner_->PostTask(FROM_HERE, std::move(flush_cb));
    return;
  }

  state_ = STATE_FLUSHING;
  std::optional<uint32_t> flush_audio_count;
  if (audio_demuxer_stream_adapter_)
    flush_audio_count = audio_demuxer_stream_adapter_->SignalFlush(true);
  std::optional<uint32_t> flush_video_count;
  if (video_demuxer_stream_adapter_)
    flush_video_count = video_demuxer_stream_adapter_->SignalFlush(true);
  // Makes sure flush count is valid if stream is available or both audio and
  // video agrees on the same flushing state.
  if ((audio_demuxer_stream_adapter_ && !flush_audio_count.has_value()) ||
      (video_demuxer_stream_adapter_ && !flush_video_count.has_value()) ||
      (audio_demuxer_stream_adapter_ && video_demuxer_stream_adapter_ &&
       flush_audio_count.has_value() != flush_video_count.has_value())) {
    return;
  }

  flush_cb_ = std::move(flush_cb);

  // Issues RPC_R_FLUSHUNTIL RPC message.
  auto rpc = std::make_unique<openscreen::cast::RpcMessage>();
  rpc->set_handle(remote_renderer_handle_);
  rpc->set_proc(openscreen::cast::RpcMessage::RPC_R_FLUSHUNTIL);
  openscreen::cast::RendererFlushUntil* message =
      rpc->mutable_renderer_flushuntil_rpc();
  if (flush_audio_count.has_value())
    message->set_audio_count(*flush_audio_count);
  if (flush_video_count.has_value())
    message->set_video_count(*flush_video_count);
  message->set_callback_handle(rpc_handle_);
  SendRpcToRemote(std::move(rpc));
}

void CourierRenderer::StartPlayingFrom(base::TimeDelta time) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());

  if (state_ != STATE_PLAYING) {
    DCHECK_EQ(state_, STATE_ERROR);
    return;
  }

  // Issues RPC_R_STARTPLAYINGFROM RPC message.
  auto rpc = std::make_unique<openscreen::cast::RpcMessage>();
  rpc->set_handle(remote_renderer_handle_);
  rpc->set_proc(openscreen::cast::RpcMessage::RPC_R_STARTPLAYINGFROM);
  rpc->set_integer64_value(time.InMicroseconds());
  SendRpcToRemote(std::move(rpc));

  {
    base::AutoLock auto_lock(time_lock_);
    current_media_time_ = time;
  }
  ResetMeasurements();
}

void CourierRenderer::SetPlaybackRate(double playback_rate) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());

  if (state_ != STATE_FLUSHING && state_ != STATE_PLAYING) {
    DCHECK_EQ(state_, STATE_ERROR);
    return;
  }

  // Issues RPC_R_SETPLAYBACKRATE RPC message.
  auto rpc = std::make_unique<openscreen::cast::RpcMessage>();
  rpc->set_handle(remote_renderer_handle_);
  rpc->set_proc(openscreen::cast::RpcMessage::RPC_R_SETPLAYBACKRATE);
  rpc->set_double_value(playback_rate);
  SendRpcToRemote(std::move(rpc));
  playback_rate_ = playback_rate;
  ResetMeasurements();
}

void CourierRenderer::SetVolume(float volume) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());

  volume_ = volume;
  if (state_ != STATE_FLUSHING && state_ != STATE_PLAYING)
    return;

  // Issues RPC_R_SETVOLUME RPC message.
  auto rpc = std::make_unique<openscreen::cast::RpcMessage>();
  rpc->set_handle(remote_renderer_handle_);
  rpc->set_proc(openscreen::cast::RpcMessage::RPC_R_SETVOLUME);
  rpc->set_double_value(volume);
  SendRpcToRemote(std::move(rpc));
}

base::TimeDelta CourierRenderer::GetMediaTime() {
  // No BelongsToCurrentThread() checking because this can be called from other
  // threads.
  // TODO(erickung): Interpolate current media time using local system time.
  // Current receiver is to update |current_media_time_| every 250ms. But it
  // needs to lower the update frequency in order to reduce network usage. Hence
  // the interpolation is needed after receiver implementation is changed.
  base::AutoLock auto_lock(time_lock_);
  return current_media_time_;
}

RendererType CourierRenderer::GetRendererType() {
  return RendererType::kCourier;
}

// static
void CourierRenderer::OnDataPipeCreatedOnMainThread(
    scoped_refptr<base::SequencedTaskRunner> media_task_runner,
    base::WeakPtr<CourierRenderer> self,
    openscreen::WeakPtr<RpcMessenger> rpc_messenger,
    mojo::PendingRemote<mojom::RemotingDataStreamSender> audio,
    mojo::PendingRemote<mojom::RemotingDataStreamSender> video,
    mojo::ScopedDataPipeProducerHandle audio_handle,
    mojo::ScopedDataPipeProducerHandle video_handle) {
  media_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&CourierRenderer::OnDataPipeCreated, self,
                     std::move(audio), std::move(video),
                     std::move(audio_handle), std::move(video_handle),
                     rpc_messenger ? rpc_messenger->GetUniqueHandle()
                                   : RpcMessenger::kInvalidHandle,
                     rpc_messenger ? rpc_messenger->GetUniqueHandle()
                                   : RpcMessenger::kInvalidHandle));
}

void CourierRenderer::OnDataPipeCreated(
    mojo::PendingRemote<mojom::RemotingDataStreamSender> audio,
    mojo::PendingRemote<mojom::RemotingDataStreamSender> video,
    mojo::ScopedDataPipeProducerHandle audio_handle,
    mojo::ScopedDataPipeProducerHandle video_handle,
    int audio_rpc_handle,
    int video_rpc_handle) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());

  if (state_ == STATE_ERROR)
    return;  // Abort because something went wrong in the meantime.
  DCHECK_EQ(state_, STATE_CREATE_PIPE);
  DCHECK(!init_workflow_done_callback_.is_null());

  // TODO(servolk): Add support for multiple streams. For now use the first
  // enabled audio and video streams to preserve the existing behavior.
  ::media::DemuxerStream* audio_demuxer_stream =
      media_resource_->GetFirstStream(DemuxerStream::AUDIO);
  ::media::DemuxerStream* video_demuxer_stream =
      media_resource_->GetFirstStream(DemuxerStream::VIDEO);

  // Create audio demuxer stream adapter if audio is available.
  if (audio_demuxer_stream && audio.is_valid() && audio_handle.is_valid() &&
      audio_rpc_handle != RpcMessenger::kInvalidHandle) {
    audio_demuxer_stream_adapter_ = std::make_unique<DemuxerStreamAdapter>(
        main_task_runner_, media_task_runner_, "audio", audio_demuxer_stream,
        rpc_messenger_, audio_rpc_handle, std::move(audio),
        std::move(audio_handle),
        base::BindOnce(&CourierRenderer::OnFatalError, base::Unretained(this)));
  }

  // Create video demuxer stream adapter if video is available.
  if (video_demuxer_stream && video.is_valid() && video_handle.is_valid() &&
      video_rpc_handle != RpcMessenger::kInvalidHandle) {
    video_demuxer_stream_adapter_ = std::make_unique<DemuxerStreamAdapter>(
        main_task_runner_, media_task_runner_, "video", video_demuxer_stream,
        rpc_messenger_, video_rpc_handle, std::move(video),
        std::move(video_handle),
        base::BindOnce(&CourierRenderer::OnFatalError, base::Unretained(this)));
  }

  // Checks if data pipe is created successfully.
  if (!audio_demuxer_stream_adapter_ && !video_demuxer_stream_adapter_) {
    OnFatalError(DATA_PIPE_CREATE_ERROR);
    return;
  }

  state_ = STATE_ACQUIRING;

  // Issues RPC_ACQUIRE_DEMUXER RPC message.
  auto rpc = std::make_unique<openscreen::cast::RpcMessage>();
  rpc->set_handle(RpcMessenger::kAcquireDemuxerHandle);
  rpc->set_proc(openscreen::cast::RpcMessage::RPC_ACQUIRE_DEMUXER);
  openscreen::cast::AcquireDemuxer* message =
      rpc->mutable_acquire_demuxer_rpc();
  message->set_audio_demuxer_handle(
      audio_demuxer_stream_adapter_
          ? audio_demuxer_stream_adapter_->rpc_handle()
          : RpcMessenger::kInvalidHandle);
  message->set_video_demuxer_handle(
      video_demuxer_stream_adapter_
          ? video_demuxer_stream_adapter_->rpc_handle()
          : RpcMessenger::kInvalidHandle);
  SendRpcToRemote(std::move(rpc));

  // Issues RPC_ACQUIRE_RENDERER RPC message.
  rpc = std::make_unique<openscreen::cast::RpcMessage>();
  rpc->set_handle(RpcMessenger::kAcquireRendererHandle);
  rpc->set_proc(openscreen::cast::RpcMessage::RPC_ACQUIRE_RENDERER);
  rpc->set_integer_value(rpc_handle_);
  SendRpcToRemote(std::move(rpc));
}

// static
void CourierRenderer::OnMessageReceivedOnMainThread(
    scoped_refptr<base::SequencedTaskRunner> media_task_runner,
    base::WeakPtr<CourierRenderer> self,
    std::unique_ptr<openscreen::cast::RpcMessage> message) {
  if (media_task_runner) {
    media_task_runner->PostTask(
        FROM_HERE, base::BindOnce(&CourierRenderer::OnReceivedRpc, self,
                                  std::move(message)));
  } else {
    LOG(WARNING) << "No valid task runner.";
  }
}

void CourierRenderer::OnReceivedRpc(
    std::unique_ptr<openscreen::cast::RpcMessage> message) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(message);
  switch (message->proc()) {
    case openscreen::cast::RpcMessage::RPC_ACQUIRE_RENDERER_DONE:
      AcquireRendererDone(std::move(message));
      break;
    case openscreen::cast::RpcMessage::RPC_R_INITIALIZE_CALLBACK:
      InitializeCallback(std::move(message));
      break;
    case openscreen::cast::RpcMessage::RPC_R_FLUSHUNTIL_CALLBACK:
      FlushUntilCallback();
      break;
    case openscreen::cast::RpcMessage::RPC_RC_ONTIMEUPDATE:
      OnTimeUpdate(std::move(message));
      break;
    case openscreen::cast::RpcMessage::RPC_RC_ONBUFFERINGSTATECHANGE:
      OnBufferingStateChange(std::move(message));
      break;
    case openscreen::cast::RpcMessage::RPC_RC_ONENDED:
      client_->OnEnded();
      break;
    case openscreen::cast::RpcMessage::RPC_RC_ONERROR:
      OnFatalError(RECEIVER_PIPELINE_ERROR);
      break;
    case openscreen::cast::RpcMessage::RPC_RC_ONAUDIOCONFIGCHANGE:
      OnAudioConfigChange(std::move(message));
      break;
    case openscreen::cast::RpcMessage::RPC_RC_ONVIDEOCONFIGCHANGE:
      OnVideoConfigChange(std::move(message));
      break;
    case openscreen::cast::RpcMessage::RPC_RC_ONVIDEONATURALSIZECHANGE:
      OnVideoNaturalSizeChange(std::move(message));
      break;
    case openscreen::cast::RpcMessage::RPC_RC_ONVIDEOOPACITYCHANGE:
      OnVideoOpacityChange(std::move(message));
      break;
    case openscreen::cast::RpcMessage::RPC_RC_ONSTATISTICSUPDATE:
      OnStatisticsUpdate(std::move(message));
      break;

    default:
      DVLOG(1) << "Unknown RPC: " << message->proc();
  }
}

void CourierRenderer::SendRpcToRemote(
    std::unique_ptr<openscreen::cast::RpcMessage> message) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(main_task_runner_);
  main_task_runner_->PostTask(FROM_HERE,
                              base::BindOnce(&RpcMessenger::SendMessageToRemote,
                                             rpc_messenger_, *message));
}

void CourierRenderer::AcquireRendererDone(
    std::unique_ptr<openscreen::cast::RpcMessage> message) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(message);

  remote_renderer_handle_ = message->integer_value();

  if (state_ != STATE_ACQUIRING || init_workflow_done_callback_.is_null()) {
    OnFatalError(PEERS_OUT_OF_SYNC);
    return;
  }
  state_ = STATE_INITIALIZING;

  // Issues RPC_R_INITIALIZE RPC message to initialize renderer.
  auto rpc = std::make_unique<openscreen::cast::RpcMessage>();
  rpc->set_handle(remote_renderer_handle_);
  rpc->set_proc(openscreen::cast::RpcMessage::RPC_R_INITIALIZE);
  openscreen::cast::RendererInitialize* init =
      rpc->mutable_renderer_initialize_rpc();
  init->set_client_handle(rpc_handle_);
  init->set_audio_demuxer_handle(
      audio_demuxer_stream_adapter_
          ? audio_demuxer_stream_adapter_->rpc_handle()
          : RpcMessenger::kInvalidHandle);
  init->set_video_demuxer_handle(
      video_demuxer_stream_adapter_
          ? video_demuxer_stream_adapter_->rpc_handle()
          : RpcMessenger::kInvalidHandle);
  init->set_callback_handle(rpc_handle_);
  SendRpcToRemote(std::move(rpc));
}

void CourierRenderer::InitializeCallback(
    std::unique_ptr<openscreen::cast::RpcMessage> message) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(message);

  const bool success = message->boolean_value();
  if (state_ != STATE_INITIALIZING || init_workflow_done_callback_.is_null()) {
    OnFatalError(PEERS_OUT_OF_SYNC);
    return;
  }

  if (!success) {
    OnFatalError(RECEIVER_INITIALIZE_FAILED);
    return;
  }

  metrics_recorder_.OnRendererInitialized();

  state_ = STATE_PLAYING;

  SetVolume(volume_);
  std::move(init_workflow_done_callback_).Run(PIPELINE_OK);
}

void CourierRenderer::FlushUntilCallback() {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());

  if (state_ != STATE_FLUSHING || !flush_cb_) {
    OnFatalError(PEERS_OUT_OF_SYNC);
    return;
  }

  state_ = STATE_PLAYING;
  if (audio_demuxer_stream_adapter_)
    audio_demuxer_stream_adapter_->SignalFlush(false);
  if (video_demuxer_stream_adapter_)
    video_demuxer_stream_adapter_->SignalFlush(false);
  std::move(flush_cb_).Run();
  ResetMeasurements();
}

void CourierRenderer::OnTimeUpdate(
    std::unique_ptr<openscreen::cast::RpcMessage> message) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(message);
  // Shutdown remoting session if receiving malformed RPC message.
  if (!message->has_rendererclient_ontimeupdate_rpc()) {
    OnFatalError(RPC_INVALID);
    return;
  }
  const int64_t time_usec =
      message->rendererclient_ontimeupdate_rpc().time_usec();
  const int64_t max_time_usec =
      message->rendererclient_ontimeupdate_rpc().max_time_usec();
  // Ignores invalid time, such as negative value, or time larger than max value
  // (usually the time stamp that all streams are pushed into AV pipeline).
  if (time_usec < 0 || max_time_usec < 0 || time_usec > max_time_usec)
    return;

  {
    // Updates current time information.
    base::AutoLock auto_lock(time_lock_);
    current_media_time_ = base::Microseconds(time_usec);
    current_max_time_ = base::Microseconds(max_time_usec);
  }

  metrics_recorder_.OnEvidenceOfPlayoutAtReceiver();
  OnMediaTimeUpdated();
}

void CourierRenderer::OnBufferingStateChange(
    std::unique_ptr<openscreen::cast::RpcMessage> message) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(message);
  if (!message->has_rendererclient_onbufferingstatechange_rpc()) {
    OnFatalError(RPC_INVALID);
    return;
  }
  std::optional<BufferingState> state = media::cast::ToMediaBufferingState(
      message->rendererclient_onbufferingstatechange_rpc().state());
  BufferingStateChangeReason reason = BUFFERING_CHANGE_REASON_UNKNOWN;
  if (!state.has_value())
    return;
  if (state == BufferingState::BUFFERING_HAVE_NOTHING) {
    receiver_is_blocked_on_local_demuxers_ = IsWaitingForDataFromDemuxers();
    reason = receiver_is_blocked_on_local_demuxers_
                 ? DEMUXER_UNDERFLOW
                 : REMOTING_NETWORK_CONGESTION;
  } else if (receiver_is_blocked_on_local_demuxers_) {
    receiver_is_blocked_on_local_demuxers_ = false;
    ResetMeasurements();
  }

  client_->OnBufferingStateChange(state.value(), reason);
}

void CourierRenderer::OnAudioConfigChange(
    std::unique_ptr<openscreen::cast::RpcMessage> message) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(message);
  // Shutdown remoting session if receiving malformed RPC message.
  if (!message->has_rendererclient_onaudioconfigchange_rpc()) {
    OnFatalError(RPC_INVALID);
    return;
  }

  const auto* audio_config_message =
      message->mutable_rendererclient_onaudioconfigchange_rpc();
  const openscreen::cast::AudioDecoderConfig pb_audio_config =
      audio_config_message->audio_decoder_config();
  AudioDecoderConfig out_audio_config;
  media::cast::ConvertProtoToAudioDecoderConfig(pb_audio_config,
                                                &out_audio_config);
  DCHECK(out_audio_config.IsValidConfig());

  client_->OnAudioConfigChange(out_audio_config);
}

void CourierRenderer::OnVideoConfigChange(
    std::unique_ptr<openscreen::cast::RpcMessage> message) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(message);
  // Shutdown remoting session if receiving malformed RPC message.
  if (!message->has_rendererclient_onvideoconfigchange_rpc()) {
    OnFatalError(RPC_INVALID);
    return;
  }

  const auto* video_config_message =
      message->mutable_rendererclient_onvideoconfigchange_rpc();
  const openscreen::cast::VideoDecoderConfig pb_video_config =
      video_config_message->video_decoder_config();
  VideoDecoderConfig out_video_config;
  media::cast::ConvertProtoToVideoDecoderConfig(pb_video_config,
                                                &out_video_config);
  DCHECK(out_video_config.IsValidConfig());

  client_->OnVideoConfigChange(out_video_config);
}

void CourierRenderer::OnVideoNaturalSizeChange(
    std::unique_ptr<openscreen::cast::RpcMessage> message) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(message);
  // Shutdown remoting session if receiving malformed RPC message.
  if (!message->has_rendererclient_onvideonatualsizechange_rpc()) {
    OnFatalError(RPC_INVALID);
    return;
  }
  const auto& size_change =
      message->rendererclient_onvideonatualsizechange_rpc();
  if (size_change.width() <= 0 || size_change.height() <= 0)
    return;
  client_->OnVideoNaturalSizeChange(
      gfx::Size(size_change.width(), size_change.height()));
}

void CourierRenderer::OnVideoOpacityChange(
    std::unique_ptr<openscreen::cast::RpcMessage> message) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(message);
  const bool opaque = message->boolean_value();
  client_->OnVideoOpacityChange(opaque);
}

void CourierRenderer::OnStatisticsUpdate(
    std::unique_ptr<openscreen::cast::RpcMessage> message) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(message);
  // Shutdown remoting session if receiving malformed RPC message.
  if (!message->has_rendererclient_onstatisticsupdate_rpc()) {
    OnFatalError(RPC_INVALID);
    return;
  }
  PipelineStatistics stats;
  media::cast::ConvertProtoToPipelineStatistics(
      message->rendererclient_onstatisticsupdate_rpc(), &stats);
  // Note: Each field in |stats| is a delta, not the aggregate amount.
  if (stats.audio_bytes_decoded > 0 || stats.video_frames_decoded > 0 ||
      stats.video_frames_dropped > 0) {
    metrics_recorder_.OnEvidenceOfPlayoutAtReceiver();
  }
  UpdateVideoStatsQueue(stats.video_frames_decoded, stats.video_frames_dropped);
  client_->OnStatisticsUpdate(stats);
}

void CourierRenderer::OnFatalError(StopTrigger stop_trigger) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  DCHECK_NE(UNKNOWN_STOP_TRIGGER, stop_trigger);

  // If this is the first error, notify the controller. It is expected the
  // controller will cause this renderer to shut down shortly.
  if (state_ != STATE_ERROR) {
    state_ = STATE_ERROR;
    main_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&RendererController::OnRendererFatalError,
                                  controller_, stop_trigger));
  }

  data_flow_poll_timer_.Stop();

  // This renderer will be shut down shortly. To prevent breaking the pipeline,
  // just run the callback without reporting error.
  if (!init_workflow_done_callback_.is_null()) {
    std::move(init_workflow_done_callback_).Run(PIPELINE_OK);
    return;
  }

  if (flush_cb_)
    std::move(flush_cb_).Run();
}

void CourierRenderer::OnMediaTimeUpdated() {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  if (flush_cb_)
    return;  // Don't manage and check the queue when Flush() is on-going.
  if (receiver_is_blocked_on_local_demuxers_)
    return;  // Don't manage and check the queue when buffering is on-going.

  base::TimeTicks current_time = clock_->NowTicks();
  if (current_time < ignore_updates_until_time_)
    return;  // Not stable yet.

  media_time_queue_.push_back(
      std::make_pair(current_time, current_media_time_));
  base::TimeDelta window_duration =
      current_time - media_time_queue_.front().first;
  if (window_duration < kTrackingWindow)
    return;  // Not enough data to make a reliable decision.

  base::TimeDelta media_duration =
      media_time_queue_.back().second - media_time_queue_.front().second;
  base::TimeDelta update_duration =
      (media_time_queue_.back().first - media_time_queue_.front().first) *
      playback_rate_;
  if ((media_duration - update_duration).magnitude() >=
      kMediaPlaybackDelayThreshold) {
    ++times_playback_delayed_;
    if (times_playback_delayed_ == kPlaybackDelayCountThreshold)
      OnFatalError(PACING_TOO_SLOWLY);
  } else {
    times_playback_delayed_ = 0;
  }

  // Prune |media_time_queue_|.
  while (media_time_queue_.back().first - media_time_queue_.front().first >=
         kTrackingWindow)
    media_time_queue_.pop_front();
}

void CourierRenderer::UpdateVideoStatsQueue(int video_frames_decoded,
                                            int video_frames_dropped) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  if (flush_cb_)
    return;  // Don't manage and check the queue when Flush() is on-going.

  if (!stats_updated_) {
    if (video_frames_decoded)
      stats_updated_ = true;
    // Ignore the first stats since it may include the information during
    // unstable period.
    return;
  }

  base::TimeTicks current_time = clock_->NowTicks();
  if (current_time < ignore_updates_until_time_)
    return;  // Not stable yet.

  video_stats_queue_.push_back(std::make_tuple(
      current_time, video_frames_decoded, video_frames_dropped));
  sum_video_frames_decoded_ += video_frames_decoded;
  sum_video_frames_dropped_ += video_frames_dropped;
  base::TimeDelta window_duration =
      current_time - std::get<0>(video_stats_queue_.front());
  if (window_duration < kTrackingWindow)
    return;  // Not enough data to make a reliable decision.

  if (sum_video_frames_decoded_ &&
      sum_video_frames_dropped_ * 100 >
          sum_video_frames_decoded_ * kMaxNumVideoFramesDroppedPercentage) {
    OnFatalError(FRAME_DROP_RATE_HIGH);
  }
  // Prune |video_stats_queue_|.
  while (std::get<0>(video_stats_queue_.back()) -
             std::get<0>(video_stats_queue_.front()) >=
         kTrackingWindow) {
    sum_video_frames_decoded_ -= std::get<1>(video_stats_queue_.front());
    sum_video_frames_dropped_ -= std::get<2>(video_stats_queue_.front());
    video_stats_queue_.pop_front();
  }
}

void CourierRenderer::ResetMeasurements() {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  media_time_queue_.clear();
  video_stats_queue_.clear();
  sum_video_frames_dropped_ = 0;
  sum_video_frames_decoded_ = 0;
  stats_updated_ = false;
  ignore_updates_until_time_ = clock_->NowTicks() + kStabilizationPeriod;

  if (state_ != STATE_ERROR &&
      (audio_demuxer_stream_adapter_ || video_demuxer_stream_adapter_)) {
    data_flow_poll_timer_.Start(FROM_HERE, kDataFlowPollPeriod, this,
                                &CourierRenderer::MeasureAndRecordDataRates);
  }
}

void CourierRenderer::MeasureAndRecordDataRates() {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());

  // Whenever media is first started or flushed/seeked, there is a "burst
  // bufferring" period as the remote device rapidly fills its buffer before
  // resuming playback. Since the goal here is to measure the sustained content
  // bitrates, ignore the byte counts the first time since the last
  // ResetMeasurements() call.
  const base::TimeTicks current_time = clock_->NowTicks();
  if (current_time < ignore_updates_until_time_ + kDataFlowPollPeriod) {
    if (audio_demuxer_stream_adapter_)
      audio_demuxer_stream_adapter_->GetBytesWrittenAndReset();
    if (video_demuxer_stream_adapter_)
      video_demuxer_stream_adapter_->GetBytesWrittenAndReset();
    return;
  }

  const int kBytesPerKilobit = 1024 / 8;
  if (audio_demuxer_stream_adapter_) {
    const double kilobits_per_second =
        (audio_demuxer_stream_adapter_->GetBytesWrittenAndReset() /
         kDataFlowPollPeriod.InSecondsF()) /
        kBytesPerKilobit;
    DCHECK_GE(kilobits_per_second, 0);
    const base::CheckedNumeric<int> checked_kbps = kilobits_per_second;
    metrics_recorder_.OnAudioRateEstimate(
        checked_kbps.ValueOrDefault(std::numeric_limits<int>::max()));
  }
  if (video_demuxer_stream_adapter_) {
    const double kilobits_per_second =
        (video_demuxer_stream_adapter_->GetBytesWrittenAndReset() /
         kDataFlowPollPeriod.InSecondsF()) /
        kBytesPerKilobit;
    DCHECK_GE(kilobits_per_second, 0);
    const base::CheckedNumeric<int> checked_kbps = kilobits_per_second;
    metrics_recorder_.OnVideoRateEstimate(
        checked_kbps.ValueOrDefault(std::numeric_limits<int>::max()));
  }
}

bool CourierRenderer::IsWaitingForDataFromDemuxers() const {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  return ((video_demuxer_stream_adapter_ &&
           video_demuxer_stream_adapter_->is_processing_read_request() &&
           !video_demuxer_stream_adapter_->is_data_pending()) ||
          (audio_demuxer_stream_adapter_ &&
           audio_demuxer_stream_adapter_->is_processing_read_request() &&
           !audio_demuxer_stream_adapter_->is_data_pending()));
}

void CourierRenderer::RegisterForRpcMessaging() {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  auto receive_callback =
      base::BindPostTaskToCurrentDefault(base::BindRepeating(
          &CourierRenderer::OnReceivedRpc, weak_factory_.GetWeakPtr()));

  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &RegisterForRpcTask, rpc_messenger_, rpc_handle_,
          [cb = std::move(receive_callback)](

              std::unique_ptr<openscreen::cast::RpcMessage> message) {
            cb.Run(std::move(message));
          }));
}

void CourierRenderer::DeregisterFromRpcMessaging() {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  if (rpc_messenger_) {
    main_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&DeregisterFromRpcTask, rpc_messenger_, rpc_handle_));
  }
}

}  // namespace remoting
}  // namespace media
