// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/remoting/demuxer_stream_adapter.h"

#include "base/base64.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/single_thread_task_runner.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/decoder_buffer.h"
#include "media/base/timestamp_constants.h"
#include "media/remoting/proto_enum_utils.h"
#include "media/remoting/proto_utils.h"

// Convenience logging macro used throughout this file.
#define DEMUXER_VLOG(level) VLOG(level) << __func__ << "[" << name_ << "]: "

namespace media {
namespace remoting {

// static
mojo::DataPipe* DemuxerStreamAdapter::CreateDataPipe() {
  // Capacity in bytes for Mojo data pipe.
  constexpr int kMojoDataPipeCapacityInBytes = 512 * 1024;
  return new mojo::DataPipe(kMojoDataPipeCapacityInBytes);
}

DemuxerStreamAdapter::DemuxerStreamAdapter(
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> media_task_runner,
    const std::string& name,
    DemuxerStream* demuxer_stream,
    const base::WeakPtr<RpcBroker>& rpc_broker,
    int rpc_handle,
    mojom::RemotingDataStreamSenderPtrInfo stream_sender_info,
    mojo::ScopedDataPipeProducerHandle producer_handle,
    const ErrorCallback& error_callback)
    : main_task_runner_(std::move(main_task_runner)),
      media_task_runner_(std::move(media_task_runner)),
      name_(name),
      rpc_broker_(rpc_broker),
      rpc_handle_(rpc_handle),
      demuxer_stream_(demuxer_stream),
      type_(demuxer_stream ? demuxer_stream->type() : DemuxerStream::UNKNOWN),
      error_callback_(error_callback),
      remote_callback_handle_(RpcBroker::kInvalidHandle),
      read_until_callback_handle_(RpcBroker::kInvalidHandle),
      read_until_count_(0),
      last_count_(0),
      pending_flush_(false),
      pending_frame_is_eos_(false),
      media_status_(DemuxerStream::kOk),
      data_pipe_writer_(std::move(producer_handle)),
      bytes_written_to_pipe_(0),
      request_buffer_weak_factory_(this),
      weak_factory_(this) {
  DCHECK(main_task_runner_);
  DCHECK(media_task_runner_);
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DCHECK(demuxer_stream);
  DCHECK(!error_callback.is_null());
  const RpcBroker::ReceiveMessageCallback receive_callback =
      BindToCurrentLoop(base::Bind(&DemuxerStreamAdapter::OnReceivedRpc,
                                   weak_factory_.GetWeakPtr()));
  main_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&RpcBroker::RegisterMessageReceiverCallback,
                                rpc_broker_, rpc_handle_, receive_callback));

  stream_sender_.Bind(std::move(stream_sender_info));
  stream_sender_.set_connection_error_handler(
      base::Bind(&DemuxerStreamAdapter::OnFatalError,
                 weak_factory_.GetWeakPtr(), MOJO_PIPE_ERROR));
}

DemuxerStreamAdapter::~DemuxerStreamAdapter() {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  main_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&RpcBroker::UnregisterMessageReceiverCallback,
                                rpc_broker_, rpc_handle_));
}

int64_t DemuxerStreamAdapter::GetBytesWrittenAndReset() {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  const int64_t current_count = bytes_written_to_pipe_;
  bytes_written_to_pipe_ = 0;
  return current_count;
}

base::Optional<uint32_t> DemuxerStreamAdapter::SignalFlush(bool flushing) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DEMUXER_VLOG(2) << "flushing=" << flushing;

  // Ignores if |pending_flush_| states is same.
  if (pending_flush_ == flushing)
    return base::nullopt;

  // Cleans up pending frame data.
  pending_frame_is_eos_ = false;
  // Invalidates pending Read() tasks.
  request_buffer_weak_factory_.InvalidateWeakPtrs();

  // Cancels in flight data in browser process.
  pending_flush_ = flushing;
  if (flushing) {
    stream_sender_->CancelInFlightData();
  } else {
    // Sets callback handle invalid to abort ongoing read request.
    read_until_callback_handle_ = RpcBroker::kInvalidHandle;
  }
  return last_count_;
}

void DemuxerStreamAdapter::OnReceivedRpc(
    std::unique_ptr<pb::RpcMessage> message) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DCHECK(message);
  DCHECK(rpc_handle_ == message->handle());

  switch (message->proc()) {
    case pb::RpcMessage::RPC_DS_INITIALIZE:
      Initialize(message->integer_value());
      break;
    case pb::RpcMessage::RPC_DS_READUNTIL:
      ReadUntil(std::move(message));
      break;
    case pb::RpcMessage::RPC_DS_ENABLEBITSTREAMCONVERTER:
      EnableBitstreamConverter();
      break;

    default:
      DEMUXER_VLOG(1) << "Unknown RPC: " << message->proc();
  }
}

void DemuxerStreamAdapter::Initialize(int remote_callback_handle) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DCHECK(!pending_flush_);
  DEMUXER_VLOG(2) << "Received RPC_DS_INITIALIZE with remote_callback_handle="
                  << remote_callback_handle;

  // Checks if initialization had been called or not.
  if (remote_callback_handle_ != RpcBroker::kInvalidHandle) {
    DEMUXER_VLOG(1) << "Duplicated initialization. Have: "
                    << remote_callback_handle_
                    << ", Given: " << remote_callback_handle;
    // Shuts down data pipe if available if providing different remote callback
    // handle for initialization. Otherwise, just silently ignore the duplicated
    // request.
    if (remote_callback_handle_ != remote_callback_handle) {
      OnFatalError(PEERS_OUT_OF_SYNC);
    }
    return;
  }
  remote_callback_handle_ = remote_callback_handle;

  // Issues RPC_DS_INITIALIZE_CALLBACK RPC message.
  std::unique_ptr<pb::RpcMessage> rpc(new pb::RpcMessage());
  rpc->set_handle(remote_callback_handle_);
  rpc->set_proc(pb::RpcMessage::RPC_DS_INITIALIZE_CALLBACK);
  auto* init_cb_message = rpc->mutable_demuxerstream_initializecb_rpc();
  init_cb_message->set_type(type_);
  switch (type_) {
    case DemuxerStream::Type::AUDIO: {
      audio_config_ = demuxer_stream_->audio_decoder_config();
      pb::AudioDecoderConfig* audio_message =
          init_cb_message->mutable_audio_decoder_config();
      ConvertAudioDecoderConfigToProto(audio_config_, audio_message);
      break;
    }
    case DemuxerStream::Type::VIDEO: {
      video_config_ = demuxer_stream_->video_decoder_config();
      pb::VideoDecoderConfig* video_message =
          init_cb_message->mutable_video_decoder_config();
      ConvertVideoDecoderConfigToProto(video_config_, video_message);
      break;
    }
    default:
      NOTREACHED();
  }

  DEMUXER_VLOG(2) << "Sending RPC_DS_INITIALIZE_CALLBACK to " << rpc->handle()
                  << " with decoder_config={"
                  << (type_ == DemuxerStream::Type::AUDIO
                          ? audio_config_.AsHumanReadableString()
                          : video_config_.AsHumanReadableString())
                  << '}';
  main_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&RpcBroker::SendMessageToRemote, rpc_broker_,
                                base::Passed(&rpc)));
}

void DemuxerStreamAdapter::ReadUntil(std::unique_ptr<pb::RpcMessage> message) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DCHECK(message);
  if (!message->has_demuxerstream_readuntil_rpc()) {
    DEMUXER_VLOG(1) << "Missing required DemuxerStreamReadUntil struct in RPC";
    OnFatalError(RPC_INVALID);
    return;
  }

  const pb::DemuxerStreamReadUntil& rpc_message =
      message->demuxerstream_readuntil_rpc();
  DEMUXER_VLOG(2) << "Received RPC_DS_READUNTIL with callback_handle="
                  << rpc_message.callback_handle()
                  << ", count=" << rpc_message.count();

  if (pending_flush_) {
    DEMUXER_VLOG(2) << "Skip actions since it's in the flushing state";
    return;
  }

  if (is_processing_read_request()) {
    DEMUXER_VLOG(2) << "Ignore read request while it's in the reading state.";
    return;
  }

  if (rpc_message.count() <= last_count_) {
    DEMUXER_VLOG(1) << "Request count shouldn't be smaller than or equal to "
                       "current frame count";
    return;
  }

  read_until_count_ = rpc_message.count();
  read_until_callback_handle_ = rpc_message.callback_handle();
  RequestBuffer();
}

void DemuxerStreamAdapter::EnableBitstreamConverter() {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DEMUXER_VLOG(2) << "Received RPC_DS_ENABLEBITSTREAMCONVERTER";
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
  demuxer_stream_->EnableBitstreamConverter();
#else
  DEMUXER_VLOG(1) << "Ignoring EnableBitstreamConverter() RPC: Proprietary "
                     "codecs not enabled in this Chromium build.";
#endif
}

void DemuxerStreamAdapter::RequestBuffer() {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  if (!is_processing_read_request() || pending_flush_) {
    DEMUXER_VLOG(2) << "Skip actions since it's not in the reading state";
    return;
  }
  demuxer_stream_->Read(base::Bind(&DemuxerStreamAdapter::OnNewBuffer,
                                   request_buffer_weak_factory_.GetWeakPtr()));
}

void DemuxerStreamAdapter::OnNewBuffer(DemuxerStream::Status status,
                                       scoped_refptr<DecoderBuffer> input) {
  DEMUXER_VLOG(3) << "status=" << status;
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  if (!is_processing_read_request() || pending_flush_) {
    DEMUXER_VLOG(2) << "Skip actions since it's not in the reading state";
    return;
  }

  switch (status) {
    case DemuxerStream::kAborted:
      DCHECK(!input);
      SendReadAck();
      return;
    case DemuxerStream::kError:
      // Currently kError can only happen because of DECRYPTION_ERROR.
      OnFatalError(DECRYPTION_ERROR);
      return;
    case DemuxerStream::kConfigChanged:
      // TODO(erickung): Notify controller of new decoder config, just in case
      // that will require remoting to be shutdown (due to known
      // lack-of-support).
      // Stores available audio/video decoder config and issues
      // RPC_DS_READUNTIL_CALLBACK RPC to notify receiver.
      DCHECK(!input);
      media_status_ = status;
      if (demuxer_stream_->type() == DemuxerStream::VIDEO)
        video_config_ = demuxer_stream_->video_decoder_config();
      if (demuxer_stream_->type() == DemuxerStream::AUDIO)
        audio_config_ = demuxer_stream_->audio_decoder_config();
      SendReadAck();
      return;
    case DemuxerStream::kOk: {
      media_status_ = status;
      DCHECK(pending_frame_.empty());
      if (!data_pipe_writer_.IsPipeValid())
        return;  // Do not start sending (due to previous fatal error).
      pending_frame_ = DecoderBufferToByteArray(*input);
      pending_frame_is_eos_ = input->end_of_stream();
      WriteFrame();
    } break;
  }
}

void DemuxerStreamAdapter::WriteFrame() {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DCHECK(!pending_flush_);
  DCHECK(is_processing_read_request());
  DCHECK(!pending_frame_.empty());

  if (!stream_sender_ || !data_pipe_writer_.IsPipeValid()) {
    DEMUXER_VLOG(1) << "Ignore since data pipe stream sender is invalid";
    return;
  }

  stream_sender_->SendFrame(pending_frame_.size());
  data_pipe_writer_.Write(pending_frame_.data(), pending_frame_.size(),
                          base::BindOnce(&DemuxerStreamAdapter::OnFrameWritten,
                                         base::Unretained(this)));
}

void DemuxerStreamAdapter::OnFrameWritten(bool success) {
  if (!success) {
    OnFatalError(MOJO_PIPE_ERROR);
    return;
  }

  bytes_written_to_pipe_ += pending_frame_.size();
  // Resets frame buffer variables.
  bool pending_frame_is_eos = pending_frame_is_eos_;
  ++last_count_;
  ResetPendingFrame();

  // Checks if it needs to send RPC_DS_READUNTIL_CALLBACK RPC message.
  if (read_until_count_ == last_count_ || pending_frame_is_eos) {
    SendReadAck();
    return;
  }

  // Contiune to read decoder buffer until reaching |read_until_count_| or
  // end of stream.
  media_task_runner_->PostTask(FROM_HERE,
                               base::Bind(&DemuxerStreamAdapter::RequestBuffer,
                                          weak_factory_.GetWeakPtr()));
}

void DemuxerStreamAdapter::SendReadAck() {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DEMUXER_VLOG(3) << "last_count_=" << last_count_
                  << ", remote_read_callback_handle="
                  << read_until_callback_handle_
                  << ", media_status=" << media_status_;
  // Issues RPC_DS_READUNTIL_CALLBACK RPC message.
  std::unique_ptr<pb::RpcMessage> rpc(new pb::RpcMessage());
  rpc->set_handle(read_until_callback_handle_);
  rpc->set_proc(pb::RpcMessage::RPC_DS_READUNTIL_CALLBACK);
  auto* message = rpc->mutable_demuxerstream_readuntilcb_rpc();
  message->set_count(last_count_);
  message->set_status(ToProtoDemuxerStreamStatus(media_status_).value());
  if (media_status_ == DemuxerStream::kConfigChanged) {
    if (audio_config_.IsValidConfig()) {
      pb::AudioDecoderConfig* audio_message =
          message->mutable_audio_decoder_config();
      ConvertAudioDecoderConfigToProto(audio_config_, audio_message);
    } else if (video_config_.IsValidConfig()) {
      pb::VideoDecoderConfig* video_message =
          message->mutable_video_decoder_config();
      ConvertVideoDecoderConfigToProto(video_config_, video_message);
    } else {
      NOTREACHED();
    }
  }

  DEMUXER_VLOG(2) << "Sending RPC_DS_READUNTIL_CALLBACK to " << rpc->handle()
                  << " with count=" << message->count()
                  << ", status=" << message->status() << ", decoder_config={"
                  << (audio_config_.IsValidConfig()
                          ? audio_config_.AsHumanReadableString()
                          : video_config_.IsValidConfig()
                                ? video_config_.AsHumanReadableString()
                                : "DID NOT CHANGE")
                  << '}';
  main_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&RpcBroker::SendMessageToRemote, rpc_broker_,
                                base::Passed(&rpc)));
  // Resets callback handle after completing the reading request.
  read_until_callback_handle_ = RpcBroker::kInvalidHandle;

  // Resets audio/video decoder config since it only sends once.
  if (audio_config_.IsValidConfig())
    audio_config_ = AudioDecoderConfig();
  if (video_config_.IsValidConfig())
    video_config_ = VideoDecoderConfig();
}

void DemuxerStreamAdapter::ResetPendingFrame() {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  pending_frame_.clear();
  pending_frame_is_eos_ = false;
}

void DemuxerStreamAdapter::OnFatalError(StopTrigger stop_trigger) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  DEMUXER_VLOG(1) << __func__ << " with StopTrigger " << stop_trigger;

  if (error_callback_.is_null())
    return;

  data_pipe_writer_.Close();

  std::move(error_callback_).Run(stop_trigger);
}

}  // namespace remoting
}  // namespace media
