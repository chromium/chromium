// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/remoting/receiver.h"

#include "base/bind.h"
#include "base/callback.h"
#include "media/base/decoder_buffer.h"
#include "media/base/renderer.h"
#include "media/remoting/proto_enum_utils.h"
#include "media/remoting/proto_utils.h"
#include "media/remoting/stream_provider.h"

namespace media {
namespace remoting {
namespace {

// The period to send the TimeUpdate RPC message to update the media time on
// sender side.
constexpr base::TimeDelta kTimeUpdateInterval =
    base::TimeDelta::FromMilliseconds(250);

}  // namespace

Receiver::Receiver(std::unique_ptr<Renderer> renderer, RpcBroker* rpc_broker)
    : renderer_(std::move(renderer)),
      rpc_broker_(rpc_broker),
      rpc_handle_(rpc_broker_->GetUniqueHandle()) {
  DCHECK(renderer_);
  DCHECK(rpc_broker_);
  rpc_broker_->RegisterMessageReceiverCallback(
      rpc_handle_,
      base::Bind(&Receiver::OnReceivedRpc, weak_factory_.GetWeakPtr()));
  rpc_broker_->RegisterMessageReceiverCallback(
      RpcBroker::kAcquireHandle,
      base::Bind(&Receiver::OnReceivedRpc, weak_factory_.GetWeakPtr()));
}

Receiver::~Receiver() {
  rpc_broker_->UnregisterMessageReceiverCallback(rpc_handle_);
  rpc_broker_->UnregisterMessageReceiverCallback(RpcBroker::kAcquireHandle);
}

void Receiver::OnReceivedRpc(std::unique_ptr<pb::RpcMessage> message) {
  DCHECK(message);
  switch (message->proc()) {
    case pb::RpcMessage::RPC_ACQUIRE_RENDERER:
      AcquireRenderer(std::move(message));
      break;
    case pb::RpcMessage::RPC_R_FLUSHUNTIL:
      FlushUntil(std::move(message));
      break;
    case pb::RpcMessage::RPC_R_STARTPLAYINGFROM:
      StartPlayingFrom(std::move(message));
      break;
    case pb::RpcMessage::RPC_R_SETPLAYBACKRATE:
      SetPlaybackRate(std::move(message));
      break;
    case pb::RpcMessage::RPC_R_SETVOLUME:
      SetVolume(std::move(message));
      break;
    case pb::RpcMessage::RPC_R_INITIALIZE:
      Initialize(std::move(message));
      break;
    default:
      VLOG(1) << __func__ << ": Unknow RPC message. proc=" << message->proc();
  }
}

void Receiver::AcquireRenderer(std::unique_ptr<pb::RpcMessage> message) {
  DVLOG(3) << __func__ << ": Receives RPC_ACQUIRE_RENDERER with remote_handle= "
           << message->integer_value();

  remote_handle_ = message->integer_value();
  if (stream_provider_) {
    VLOG(1) << "Acquire renderer error: Already acquired.";
    OnError(PipelineStatus::PIPELINE_ERROR_DECODE);
    return;
  }

  stream_provider_.reset(new StreamProvider(
      rpc_broker_,
      base::BindOnce(&Receiver::OnError, weak_factory_.GetWeakPtr(),
                     PipelineStatus::PIPELINE_ERROR_DECODE)));

  DVLOG(3) << __func__
           << ": Issues RPC_ACQUIRE_RENDERER_DONE RPC message. remote_handle="
           << remote_handle_ << " rpc_handle=" << rpc_handle_;
  std::unique_ptr<pb::RpcMessage> rpc(new pb::RpcMessage());
  rpc->set_handle(remote_handle_);
  rpc->set_proc(pb::RpcMessage::RPC_ACQUIRE_RENDERER_DONE);
  rpc->set_integer_value(rpc_handle_);
  rpc_broker_->SendMessageToRemote(std::move(rpc));
}

void Receiver::Initialize(std::unique_ptr<pb::RpcMessage> message) {
  DCHECK(stream_provider_);
  DVLOG(3) << __func__ << ": Receives RPC_R_INITIALIZE with callback handle= "
           << message->renderer_initialize_rpc().callback_handle();
  DCHECK(message->renderer_initialize_rpc().callback_handle() ==
         remote_handle_);
  if (!stream_provider_)
    OnRendererInitialized(PipelineStatus::PIPELINE_ERROR_INITIALIZATION_FAILED);

  stream_provider_->Initialize(
      message->renderer_initialize_rpc().audio_demuxer_handle(),
      message->renderer_initialize_rpc().video_demuxer_handle(),
      base::BindOnce(&Receiver::OnStreamInitialized,
                     weak_factory_.GetWeakPtr()));
}

void Receiver::OnStreamInitialized() {
  DCHECK(stream_provider_);
  renderer_->Initialize(stream_provider_.get(), this,
                        base::BindOnce(&Receiver::OnRendererInitialized,
                                       weak_factory_.GetWeakPtr()));
}

void Receiver::OnRendererInitialized(PipelineStatus status) {
  DVLOG(3) << __func__ << ": Issues RPC_R_INITIALIZE_CALLBACK RPC message."
           << "remote_handle=" << remote_handle_;

  std::unique_ptr<pb::RpcMessage> rpc(new pb::RpcMessage());
  rpc->set_handle(remote_handle_);
  rpc->set_proc(pb::RpcMessage::RPC_R_INITIALIZE_CALLBACK);
  rpc->set_boolean_value(status == PIPELINE_OK);
  rpc_broker_->SendMessageToRemote(std::move(rpc));
}

void Receiver::SetPlaybackRate(std::unique_ptr<pb::RpcMessage> message) {
  const double playback_rate = message->double_value();
  DVLOG(3) << __func__
           << ": Receives RPC_R_SETPLAYBACKRATE with rate=" << playback_rate;
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

void Receiver::FlushUntil(std::unique_ptr<pb::RpcMessage> message) {
  DVLOG(3) << __func__ << ": Receives RPC_R_FLUSHUNTIL RPC message.";

  const pb::RendererFlushUntil flush_message =
      message->renderer_flushuntil_rpc();
  DCHECK_EQ(flush_message.callback_handle(), remote_handle_);
  if (stream_provider_) {
    if (flush_message.has_audio_count()) {
      stream_provider_->FlushUntil(DemuxerStream::AUDIO,
                                   flush_message.audio_count());
    }
    if (flush_message.has_video_count()) {
      stream_provider_->FlushUntil(DemuxerStream::VIDEO,
                                   flush_message.video_count());
    }
  }
  time_update_timer_.Stop();
  renderer_->Flush(
      base::BindOnce(&Receiver::OnFlushDone, weak_factory_.GetWeakPtr()));
}

void Receiver::OnFlushDone() {
  DVLOG(3) << __func__ << ": Issues RPC_R_FLUSHUNTIL_CALLBACK RPC message.";

  std::unique_ptr<pb::RpcMessage> rpc(new pb::RpcMessage());
  rpc->set_handle(remote_handle_);
  rpc->set_proc(pb::RpcMessage::RPC_R_FLUSHUNTIL_CALLBACK);
  rpc_broker_->SendMessageToRemote(std::move(rpc));
}

void Receiver::StartPlayingFrom(std::unique_ptr<pb::RpcMessage> message) {
  DVLOG(3) << __func__ << ": Receives RPC_R_STARTPLAYINGFROM message.";
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

void Receiver::SetVolume(std::unique_ptr<pb::RpcMessage> message) {
  DVLOG(3) << __func__ << ": Receives RPC_R_SETVOLUME message.";
  renderer_->SetVolume(message->double_value());
}

void Receiver::SendMediaTimeUpdate() {
  // Issues RPC_RC_ONTIMEUPDATE RPC message.
  std::unique_ptr<pb::RpcMessage> rpc(new pb::RpcMessage());
  rpc->set_handle(remote_handle_);
  rpc->set_proc(pb::RpcMessage::RPC_RC_ONTIMEUPDATE);
  auto* message = rpc->mutable_rendererclient_ontimeupdate_rpc();
  base::TimeDelta media_time = renderer_->GetMediaTime();
  message->set_time_usec(media_time.InMicroseconds());
  base::TimeDelta max_time = media_time;
  message->set_max_time_usec(max_time.InMicroseconds());
  DVLOG(3) << __func__ << ": Issues RPC_RC_ONTIMEUPDATE message."
           << " media_time = " << media_time.InMicroseconds()
           << " max_time= " << max_time.InMicroseconds();
  rpc_broker_->SendMessageToRemote(std::move(rpc));
}

void Receiver::OnReceivedBuffer(DemuxerStream::Type type,
                                scoped_refptr<DecoderBuffer> buffer) {
  DVLOG(3) << __func__
           << ": type=" << (type == DemuxerStream::AUDIO ? "Audio" : "Video");
  DCHECK(stream_provider_);
  stream_provider_->AppendBuffer(type, buffer);
}

void Receiver::OnError(PipelineStatus status) {
  VLOG(1) << __func__ << ": Issues RPC_RC_ONERROR message.";
  std::unique_ptr<pb::RpcMessage> rpc(new pb::RpcMessage());
  rpc->set_handle(remote_handle_);
  rpc->set_proc(pb::RpcMessage::RPC_RC_ONERROR);
  rpc_broker_->SendMessageToRemote(std::move(rpc));
}

void Receiver::OnEnded() {
  DVLOG(3) << __func__ << ": Issues RPC_RC_ONENDED message.";
  std::unique_ptr<pb::RpcMessage> rpc(new pb::RpcMessage());
  rpc->set_handle(remote_handle_);
  rpc->set_proc(pb::RpcMessage::RPC_RC_ONENDED);
  rpc_broker_->SendMessageToRemote(std::move(rpc));
  time_update_timer_.Stop();
}

void Receiver::OnStatisticsUpdate(const PipelineStatistics& stats) {
  DVLOG(3) << __func__ << ": Issues RPC_RC_ONSTATISTICSUPDATE message.";
  std::unique_ptr<pb::RpcMessage> rpc(new pb::RpcMessage());
  rpc->set_handle(remote_handle_);
  rpc->set_proc(pb::RpcMessage::RPC_RC_ONSTATISTICSUPDATE);
  auto* message = rpc->mutable_rendererclient_onstatisticsupdate_rpc();
  message->set_audio_bytes_decoded(stats.audio_bytes_decoded);
  message->set_video_bytes_decoded(stats.video_bytes_decoded);
  message->set_video_frames_decoded(stats.video_frames_decoded);
  message->set_video_frames_dropped(stats.video_frames_dropped);
  message->set_audio_memory_usage(stats.audio_memory_usage);
  message->set_video_memory_usage(stats.video_memory_usage);
  rpc_broker_->SendMessageToRemote(std::move(rpc));
}

void Receiver::OnBufferingStateChange(BufferingState state,
                                      BufferingStateChangeReason reason) {
  DVLOG(3) << __func__
           << ": Issues RPC_RC_ONBUFFERINGSTATECHANGE message: state=" << state;

  // The |reason| is determined on the other side of the RPC in CourierRenderer.
  // For now, there is no reason to provide this in the |message| below.
  DCHECK_EQ(reason, BUFFERING_CHANGE_REASON_UNKNOWN);

  std::unique_ptr<pb::RpcMessage> rpc(new pb::RpcMessage());
  rpc->set_handle(remote_handle_);
  rpc->set_proc(pb::RpcMessage::RPC_RC_ONBUFFERINGSTATECHANGE);
  auto* message = rpc->mutable_rendererclient_onbufferingstatechange_rpc();
  message->set_state(ToProtoMediaBufferingState(state).value());
  rpc_broker_->SendMessageToRemote(std::move(rpc));
}

// TODO: Passes |reason| over.
void Receiver::OnWaiting(WaitingReason reason) {
  DVLOG(3) << __func__ << ": Issues RPC_RC_ONWAITINGFORDECRYPTIONKEY message.";
  std::unique_ptr<pb::RpcMessage> rpc(new pb::RpcMessage());
  rpc->set_handle(remote_handle_);
  rpc->set_proc(pb::RpcMessage::RPC_RC_ONWAITINGFORDECRYPTIONKEY);
  rpc_broker_->SendMessageToRemote(std::move(rpc));
}

void Receiver::OnAudioConfigChange(const AudioDecoderConfig& config) {
  DVLOG(3) << __func__ << ": Issues RPC_RC_ONAUDIOCONFIGCHANGE message.";
  std::unique_ptr<pb::RpcMessage> rpc(new pb::RpcMessage());
  rpc->set_handle(remote_handle_);
  rpc->set_proc(pb::RpcMessage::RPC_RC_ONAUDIOCONFIGCHANGE);
  auto* message = rpc->mutable_rendererclient_onaudioconfigchange_rpc();
  pb::AudioDecoderConfig* proto_audio_config =
      message->mutable_audio_decoder_config();
  ConvertAudioDecoderConfigToProto(config, proto_audio_config);
  rpc_broker_->SendMessageToRemote(std::move(rpc));
}

void Receiver::OnVideoConfigChange(const VideoDecoderConfig& config) {
  DVLOG(3) << __func__ << ": Issues RPC_RC_ONVIDEOCONFIGCHANGE message.";
  std::unique_ptr<pb::RpcMessage> rpc(new pb::RpcMessage());
  rpc->set_handle(remote_handle_);
  rpc->set_proc(pb::RpcMessage::RPC_RC_ONVIDEOCONFIGCHANGE);
  auto* message = rpc->mutable_rendererclient_onvideoconfigchange_rpc();
  pb::VideoDecoderConfig* proto_video_config =
      message->mutable_video_decoder_config();
  ConvertVideoDecoderConfigToProto(config, proto_video_config);
  rpc_broker_->SendMessageToRemote(std::move(rpc));
}

void Receiver::OnVideoNaturalSizeChange(const gfx::Size& size) {
  DVLOG(3) << __func__ << ": Issues RPC_RC_ONVIDEONATURALSIZECHANGE message.";
  std::unique_ptr<pb::RpcMessage> rpc(new pb::RpcMessage());
  rpc->set_handle(remote_handle_);
  rpc->set_proc(pb::RpcMessage::RPC_RC_ONVIDEONATURALSIZECHANGE);
  auto* message = rpc->mutable_rendererclient_onvideonatualsizechange_rpc();
  message->set_width(size.width());
  message->set_height(size.height());
  rpc_broker_->SendMessageToRemote(std::move(rpc));
}

void Receiver::OnVideoOpacityChange(bool opaque) {
  DVLOG(3) << __func__ << ": Issues RPC_RC_ONVIDEOOPACITYCHANGE message.";
  std::unique_ptr<pb::RpcMessage> rpc(new pb::RpcMessage());
  rpc->set_handle(remote_handle_);
  rpc->set_proc(pb::RpcMessage::RPC_RC_ONVIDEOOPACITYCHANGE);
  rpc->set_boolean_value(opaque);
  rpc_broker_->SendMessageToRemote(std::move(rpc));
}

}  // namespace remoting
}  // namespace media
