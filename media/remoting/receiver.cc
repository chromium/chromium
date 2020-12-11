// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/remoting/receiver.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/notreached.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/decoder_buffer.h"
#include "media/base/renderer.h"
#include "media/remoting/proto_enum_utils.h"
#include "media/remoting/proto_utils.h"
#include "media/remoting/receiver_controller.h"
#include "media/remoting/stream_provider.h"

namespace media {
namespace remoting {
namespace {

// The period to send the TimeUpdate RPC message to update the media time on
// sender side.
constexpr base::TimeDelta kTimeUpdateInterval =
    base::TimeDelta::FromMilliseconds(250);

}  // namespace

Receiver::Receiver(
    int rpc_handle,
    int remote_handle,
    ReceiverController* receiver_controller,
    const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
    std::unique_ptr<Renderer> renderer,
    base::OnceCallback<void(int)> acquire_renderer_done_cb)
    : rpc_handle_(rpc_handle),
      remote_handle_(remote_handle),
      receiver_controller_(receiver_controller),
      rpc_broker_(receiver_controller_->rpc_broker()),
      main_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      media_task_runner_(media_task_runner),
      renderer_(std::move(renderer)),
      acquire_renderer_done_cb_(std::move(acquire_renderer_done_cb)) {
  DCHECK(rpc_handle_ != RpcBroker::kInvalidHandle);
  DCHECK(receiver_controller_);
  DCHECK(rpc_broker_);
  DCHECK(renderer_);

  // Note: The constructor is running on the main thread, but will be destroyed
  // on the media thread. Therefore, all weak pointers must be dereferenced on
  // the media thread.
  const RpcBroker::ReceiveMessageCallback receive_callback = BindToLoop(
      media_task_runner_,
      BindRepeating(&Receiver::OnReceivedRpc, weak_factory_.GetWeakPtr()));

  // Listening all renderer rpc messages.
  rpc_broker_->RegisterMessageReceiverCallback(rpc_handle_, receive_callback);
  VerifyAcquireRendererDone();
}

Receiver::~Receiver() {
  rpc_broker_->UnregisterMessageReceiverCallback(rpc_handle_);
  rpc_broker_->UnregisterMessageReceiverCallback(
      RpcBroker::kAcquireRendererHandle);
}

// Receiver::Initialize() will be called by the local pipeline, it would only
// keep the |init_cb| in order to continue the initialization once it receives
// RPC_R_INITIALIZE, which means Receiver::RpcInitialize() is called.
void Receiver::Initialize(MediaResource* media_resource,
                          RendererClient* client,
                          PipelineStatusCallback init_cb) {
  demuxer_ = media_resource;
  init_cb_ = std::move(init_cb);
  ShouldInitializeRenderer();
}

/* CDM is not supported for remoting media */
void Receiver::SetCdm(CdmContext* cdm_context, CdmAttachedCB cdm_attached_cb) {
  NOTREACHED();
}

// No-op. Controlled by sender via RPC calls instead.
void Receiver::SetLatencyHint(base::Optional<base::TimeDelta> latency_hint) {}

// No-op. Controlled by sender via RPC calls instead.
void Receiver::Flush(base::OnceClosure flush_cb) {}

// No-op. Controlled by sender via RPC calls instead.
void Receiver::StartPlayingFrom(base::TimeDelta time) {}

// No-op. Controlled by sender via RPC calls instead.
void Receiver::SetPlaybackRate(double playback_rate) {}

// No-op. Controlled by sender via RPC calls instead.
void Receiver::SetVolume(float volume) {}

// No-op. Controlled by sender via RPC calls instead.
base::TimeDelta Receiver::GetMediaTime() {
  return base::TimeDelta();
}

void Receiver::SendRpcMessageOnMainThread(
    std::unique_ptr<pb::RpcMessage> message) {
  // |rpc_broker_| is owned by |receiver_controller_| which is a singleton per
  // process, so it's safe to use Unretained() here.
  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&RpcBroker::SendMessageToRemote,
                     base::Unretained(rpc_broker_), std::move(message)));
}

void Receiver::OnReceivedRpc(std::unique_ptr<pb::RpcMessage> message) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DCHECK(message);
  switch (message->proc()) {
    case pb::RpcMessage::RPC_R_INITIALIZE:
      RpcInitialize(std::move(message));
      break;
    case pb::RpcMessage::RPC_R_FLUSHUNTIL:
      RpcFlushUntil(std::move(message));
      break;
    case pb::RpcMessage::RPC_R_STARTPLAYINGFROM:
      RpcStartPlayingFrom(std::move(message));
      break;
    case pb::RpcMessage::RPC_R_SETPLAYBACKRATE:
      RpcSetPlaybackRate(std::move(message));
      break;
    case pb::RpcMessage::RPC_R_SETVOLUME:
      RpcSetVolume(std::move(message));
      break;
    default:
      VLOG(1) << __func__ << ": Unknown RPC message. proc=" << message->proc();
  }
}

void Receiver::SetRemoteHandle(int remote_handle) {
  DCHECK_NE(remote_handle, RpcBroker::kInvalidHandle);
  DCHECK_EQ(remote_handle_, RpcBroker::kInvalidHandle);
  remote_handle_ = remote_handle;
  VerifyAcquireRendererDone();
}

void Receiver::VerifyAcquireRendererDone() {
  if (remote_handle_ == RpcBroker::kInvalidHandle)
    return;

  DCHECK(acquire_renderer_done_cb_);
  std::move(acquire_renderer_done_cb_).Run(rpc_handle_);
}

void Receiver::RpcInitialize(std::unique_ptr<pb::RpcMessage> message) {
  DCHECK(renderer_);
  rpc_initialize_received_ = true;
  ShouldInitializeRenderer();
}

void Receiver::ShouldInitializeRenderer() {
  // ShouldInitializeRenderer() will be called from Initialize() and
  // RpcInitialize() in different orders.
  //
  // |renderer_| must be initialized when both Initialize() and
  // RpcInitialize() are called.
  if (!rpc_initialize_received_ || !init_cb_)
    return;

  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DCHECK(renderer_);
  DCHECK(demuxer_);
  renderer_->Initialize(demuxer_, this,
                        base::BindOnce(&Receiver::OnRendererInitialized,
                                       weak_factory_.GetWeakPtr()));
}

void Receiver::OnRendererInitialized(PipelineStatus status) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DCHECK(init_cb_);
  std::move(init_cb_).Run(status);

  auto rpc = std::make_unique<pb::RpcMessage>();
  rpc->set_handle(remote_handle_);
  rpc->set_proc(pb::RpcMessage::RPC_R_INITIALIZE_CALLBACK);
  rpc->set_boolean_value(status == PIPELINE_OK);
  SendRpcMessageOnMainThread(std::move(rpc));
}

void Receiver::RpcSetPlaybackRate(std::unique_ptr<pb::RpcMessage> message) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  const double playback_rate = message->double_value();
  renderer_->SetPlaybackRate(playback_rate);

  if (playback_rate == 0.0) {
    if (time_update_timer_.IsRunning()) {
      time_update_timer_.Stop();
      // Send one final media time update since the sender will not get any
      // until playback resumes.
      SendMediaTimeUpdate();
    }
  } else {
    ScheduleMediaTimeUpdates();
  }
}

void Receiver::RpcFlushUntil(std::unique_ptr<pb::RpcMessage> message) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DCHECK(message->has_renderer_flushuntil_rpc());

  const pb::RendererFlushUntil flush_message =
      message->renderer_flushuntil_rpc();
  DCHECK_EQ(flush_message.callback_handle(), remote_handle_);

  receiver_controller_->OnRendererFlush(flush_message.audio_count(),
                                        flush_message.video_count());

  time_update_timer_.Stop();
  renderer_->Flush(
      base::BindOnce(&Receiver::OnFlushDone, weak_factory_.GetWeakPtr()));
}

void Receiver::OnFlushDone() {
  auto rpc = std::make_unique<pb::RpcMessage>();
  rpc->set_handle(remote_handle_);
  rpc->set_proc(pb::RpcMessage::RPC_R_FLUSHUNTIL_CALLBACK);
  SendRpcMessageOnMainThread(std::move(rpc));
}

void Receiver::RpcStartPlayingFrom(std::unique_ptr<pb::RpcMessage> message) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  base::TimeDelta time =
      base::TimeDelta::FromMicroseconds(message->integer64_value());
  renderer_->StartPlayingFrom(time);
  ScheduleMediaTimeUpdates();
}

void Receiver::ScheduleMediaTimeUpdates() {
  if (time_update_timer_.IsRunning())
    return;
  SendMediaTimeUpdate();
  time_update_timer_.Start(FROM_HERE, kTimeUpdateInterval,
                           base::BindRepeating(&Receiver::SendMediaTimeUpdate,
                                               weak_factory_.GetWeakPtr()));
}

void Receiver::RpcSetVolume(std::unique_ptr<pb::RpcMessage> message) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  renderer_->SetVolume(message->double_value());
}

void Receiver::SendMediaTimeUpdate() {
  // Issues RPC_RC_ONTIMEUPDATE RPC message.
  auto rpc = std::make_unique<pb::RpcMessage>();
  rpc->set_handle(remote_handle_);
  rpc->set_proc(pb::RpcMessage::RPC_RC_ONTIMEUPDATE);
  auto* message = rpc->mutable_rendererclient_ontimeupdate_rpc();
  base::TimeDelta media_time = renderer_->GetMediaTime();
  message->set_time_usec(media_time.InMicroseconds());
  base::TimeDelta max_time = media_time;
  message->set_max_time_usec(max_time.InMicroseconds());
  SendRpcMessageOnMainThread(std::move(rpc));
}

void Receiver::OnError(PipelineStatus status) {
  auto rpc = std::make_unique<pb::RpcMessage>();
  rpc->set_handle(remote_handle_);
  rpc->set_proc(pb::RpcMessage::RPC_RC_ONERROR);
  SendRpcMessageOnMainThread(std::move(rpc));
}

void Receiver::OnEnded() {
  auto rpc = std::make_unique<pb::RpcMessage>();
  rpc->set_handle(remote_handle_);
  rpc->set_proc(pb::RpcMessage::RPC_RC_ONENDED);
  SendRpcMessageOnMainThread(std::move(rpc));
  time_update_timer_.Stop();
}

void Receiver::OnStatisticsUpdate(const PipelineStatistics& stats) {
  auto rpc = std::make_unique<pb::RpcMessage>();
  rpc->set_handle(remote_handle_);
  rpc->set_proc(pb::RpcMessage::RPC_RC_ONSTATISTICSUPDATE);
  auto* message = rpc->mutable_rendererclient_onstatisticsupdate_rpc();
  message->set_audio_bytes_decoded(stats.audio_bytes_decoded);
  message->set_video_bytes_decoded(stats.video_bytes_decoded);
  message->set_video_frames_decoded(stats.video_frames_decoded);
  message->set_video_frames_dropped(stats.video_frames_dropped);
  message->set_audio_memory_usage(stats.audio_memory_usage);
  message->set_video_memory_usage(stats.video_memory_usage);
  SendRpcMessageOnMainThread(std::move(rpc));
}

void Receiver::OnBufferingStateChange(BufferingState state,
                                      BufferingStateChangeReason reason) {
  auto rpc = std::make_unique<pb::RpcMessage>();
  rpc->set_handle(remote_handle_);
  rpc->set_proc(pb::RpcMessage::RPC_RC_ONBUFFERINGSTATECHANGE);
  auto* message = rpc->mutable_rendererclient_onbufferingstatechange_rpc();
  message->set_state(ToProtoMediaBufferingState(state).value());
  SendRpcMessageOnMainThread(std::move(rpc));
}

void Receiver::OnWaiting(WaitingReason reason) {
  // Media Remoting has not implemented this concept.
  NOTIMPLEMENTED();
}

void Receiver::OnAudioConfigChange(const AudioDecoderConfig& config) {
  auto rpc = std::make_unique<pb::RpcMessage>();
  rpc->set_handle(remote_handle_);
  rpc->set_proc(pb::RpcMessage::RPC_RC_ONAUDIOCONFIGCHANGE);
  auto* message = rpc->mutable_rendererclient_onaudioconfigchange_rpc();
  pb::AudioDecoderConfig* proto_audio_config =
      message->mutable_audio_decoder_config();
  ConvertAudioDecoderConfigToProto(config, proto_audio_config);
  SendRpcMessageOnMainThread(std::move(rpc));
}

void Receiver::OnVideoConfigChange(const VideoDecoderConfig& config) {
  auto rpc = std::make_unique<pb::RpcMessage>();
  rpc->set_handle(remote_handle_);
  rpc->set_proc(pb::RpcMessage::RPC_RC_ONVIDEOCONFIGCHANGE);
  auto* message = rpc->mutable_rendererclient_onvideoconfigchange_rpc();
  pb::VideoDecoderConfig* proto_video_config =
      message->mutable_video_decoder_config();
  ConvertVideoDecoderConfigToProto(config, proto_video_config);
  SendRpcMessageOnMainThread(std::move(rpc));
}

void Receiver::OnVideoNaturalSizeChange(const gfx::Size& size) {
  auto rpc = std::make_unique<pb::RpcMessage>();
  rpc->set_handle(remote_handle_);
  rpc->set_proc(pb::RpcMessage::RPC_RC_ONVIDEONATURALSIZECHANGE);
  auto* message = rpc->mutable_rendererclient_onvideonatualsizechange_rpc();
  message->set_width(size.width());
  message->set_height(size.height());
  SendRpcMessageOnMainThread(std::move(rpc));

  // Notify the host.
  receiver_controller_->OnVideoNaturalSizeChange(size);
}

void Receiver::OnVideoOpacityChange(bool opaque) {
  auto rpc = std::make_unique<pb::RpcMessage>();
  rpc->set_handle(remote_handle_);
  rpc->set_proc(pb::RpcMessage::RPC_RC_ONVIDEOOPACITYCHANGE);
  rpc->set_boolean_value(opaque);
  SendRpcMessageOnMainThread(std::move(rpc));
}

void Receiver::OnVideoFrameRateChange(base::Optional<int>) {}

}  // namespace remoting
}  // namespace media
