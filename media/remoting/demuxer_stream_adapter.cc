// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/remoting/demuxer_stream_adapter.h"

#include <utility>

#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "media/base/decoder_buffer.h"
#include "media/base/timestamp_constants.h"
#include "media/cast/openscreen/remoting_proto_enum_utils.h"
#include "media/cast/openscreen/remoting_proto_utils.h"
#include "media/mojo/common/media_type_converters.h"

// Convenience logging macro used throughout this file.
#define DEMUXER_VLOG(level) VLOG(level) << __func__ << "[" << name_ << "]: "

using openscreen::cast::RpcMessenger;

namespace media {
namespace remoting {

namespace {
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

DemuxerStreamAdapter::DemuxerStreamAdapter(
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    scoped_refptr<base::SequencedTaskRunner> media_task_runner,
    const std::string& name,
    DemuxerStream* demuxer_stream,
    const openscreen::WeakPtr<RpcMessenger>& rpc_messenger,
    int rpc_handle,
    mojo::PendingRemote<mojom::RemotingDataStreamSender> stream_sender_remote,
    mojo::ScopedDataPipeProducerHandle producer_handle,
    ErrorCallback error_callback)
    : main_task_runner_(std::move(main_task_runner)),
      media_task_runner_(std::move(media_task_runner)),
      name_(name),
      rpc_messenger_(rpc_messenger),
      rpc_handle_(rpc_handle),
      demuxer_stream_(demuxer_stream),
      type_(demuxer_stream ? demuxer_stream->type() : DemuxerStream::UNKNOWN),
      error_callback_(std::move(error_callback)),
      remote_callback_handle_(RpcMessenger::kInvalidHandle),
      read_until_callback_handle_(RpcMessenger::kInvalidHandle),
      read_until_count_(0),
      last_count_(0),
      pending_flush_(false),
      media_status_(DemuxerStream::kOk),
      data_pipe_writer_(std::move(producer_handle)),
      bytes_written_to_pipe_(0) {
  DCHECK(main_task_runner_);
  DCHECK(media_task_runner_);
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(demuxer_stream);
  DCHECK(!error_callback_.is_null());

  RegisterForRpcMessaging();

  stream_sender_.Bind(std::move(stream_sender_remote));
  stream_sender_.set_disconnect_handler(
      base::BindOnce(&DemuxerStreamAdapter::OnFatalError,
                     weak_factory_.GetWeakPtr(), MOJO_DISCONNECTED));
}

DemuxerStreamAdapter::~DemuxerStreamAdapter() {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  DeregisterFromRpcMessaging();
}

int64_t DemuxerStreamAdapter::GetBytesWrittenAndReset() {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  const int64_t current_count = bytes_written_to_pipe_;
  bytes_written_to_pipe_ = 0;
  return current_count;
}

std::optional<uint32_t> DemuxerStreamAdapter::SignalFlush(bool flushing) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  DEMUXER_VLOG(2) << "flushing=" << flushing;

  // Ignores if |pending_flush_| states is same.
  if (pending_flush_ == flushing)
    return std::nullopt;

  // Invalidates pending Read() tasks.
  request_buffer_weak_factory_.InvalidateWeakPtrs();

  // Cancels in flight data in browser process.
  pending_flush_ = flushing;
  if (flushing) {
    stream_sender_->CancelInFlightData();
  } else {
    // Sets callback handle invalid to abort ongoing read request.
    read_until_callback_handle_ = RpcMessenger::kInvalidHandle;
  }
  return last_count_;
}

void DemuxerStreamAdapter::OnReceivedRpc(
    std::unique_ptr<openscreen::cast::RpcMessage> message) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(message);
  DCHECK(rpc_handle_ == message->handle());

  switch (message->proc()) {
    case openscreen::cast::RpcMessage::RPC_DS_INITIALIZE:
      Initialize(message->integer_value());
      break;
    case openscreen::cast::RpcMessage::RPC_DS_READUNTIL:
      ReadUntil(std::move(message));
      break;
    case openscreen::cast::RpcMessage::RPC_DS_ENABLEBITSTREAMCONVERTER:
      EnableBitstreamConverter();
      break;
    case openscreen::cast::RpcMessage::RPC_DS_ONERROR:
      OnFatalError(UNEXPECTED_FAILURE);
      break;
    default:
      DEMUXER_VLOG(1) << "Unknown RPC: " << message->proc();
  }
}

void DemuxerStreamAdapter::Initialize(int remote_callback_handle) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(!pending_flush_);
  DEMUXER_VLOG(2) << "Received RPC_DS_INITIALIZE with remote_callback_handle="
                  << remote_callback_handle;

  // Checks if initialization had been called or not.
  if (remote_callback_handle_ != RpcMessenger::kInvalidHandle) {
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
  std::unique_ptr<openscreen::cast::RpcMessage> rpc(
      new openscreen::cast::RpcMessage());
  rpc->set_handle(remote_callback_handle_);
  rpc->set_proc(openscreen::cast::RpcMessage::RPC_DS_INITIALIZE_CALLBACK);
  auto* init_cb_message = rpc->mutable_demuxerstream_initializecb_rpc();
  init_cb_message->set_type(type_);
  switch (type_) {
    case DemuxerStream::Type::AUDIO: {
      audio_config_ = demuxer_stream_->audio_decoder_config();
      openscreen::cast::AudioDecoderConfig* audio_message =
          init_cb_message->mutable_audio_decoder_config();
      media::cast::ConvertAudioDecoderConfigToProto(audio_config_,
                                                    audio_message);
      break;
    }
    case DemuxerStream::Type::VIDEO: {
      video_config_ = demuxer_stream_->video_decoder_config();
      openscreen::cast::VideoDecoderConfig* video_message =
          init_cb_message->mutable_video_decoder_config();
      media::cast::ConvertVideoDecoderConfigToProto(video_config_,
                                                    video_message);
      break;
    }
    default:
      NOTREACHED_IN_MIGRATION();
  }

  DEMUXER_VLOG(2) << "Sending RPC_DS_INITIALIZE_CALLBACK to " << rpc->handle()
                  << " with decoder_config={"
                  << (type_ == DemuxerStream::Type::AUDIO
                          ? audio_config_.AsHumanReadableString()
                          : video_config_.AsHumanReadableString())
                  << '}';
  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&RpcMessenger::SendMessageToRemote, rpc_messenger_, *rpc));
}

void DemuxerStreamAdapter::ReadUntil(
    std::unique_ptr<openscreen::cast::RpcMessage> message) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(message);
  if (!message->has_demuxerstream_readuntil_rpc()) {
    DEMUXER_VLOG(1) << "Missing required DemuxerStreamReadUntil struct in RPC";
    OnFatalError(RPC_INVALID);
    return;
  }

  const openscreen::cast::DemuxerStreamReadUntil& rpc_message =
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
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  DEMUXER_VLOG(2) << "Received RPC_DS_ENABLEBITSTREAMCONVERTER";
  bool is_command_sent = true;
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
  demuxer_stream_->EnableBitstreamConverter();
#else
  is_command_sent = false;
  DEMUXER_VLOG(1) << "Ignoring EnableBitstreamConverter() RPC: Proprietary "
                     "codecs not enabled in this Chromium build.";
#endif

  if (remote_callback_handle_ != RpcMessenger::kInvalidHandle) {
    auto rpc = std::make_unique<openscreen::cast::RpcMessage>();
    rpc->set_handle(remote_callback_handle_);
    rpc->set_proc(
        openscreen::cast::RpcMessage::RPC_DS_ENABLEBITSTREAMCONVERTER_CALLBACK);
    rpc->set_boolean_value(is_command_sent);
    main_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&RpcMessenger::SendMessageToRemote,
                                  rpc_messenger_, *rpc));
  }
}

void DemuxerStreamAdapter::RequestBuffer() {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  if (!is_processing_read_request() || pending_flush_) {
    DEMUXER_VLOG(2) << "Skip actions since it's not in the reading state";
    return;
  }
  demuxer_stream_->Read(
      1, base::BindOnce(&DemuxerStreamAdapter::OnNewBuffersRead,
                        request_buffer_weak_factory_.GetWeakPtr()));
}

void DemuxerStreamAdapter::OnNewBuffersRead(
    DemuxerStream::Status status,
    DemuxerStream::DecoderBufferVector buffers_queue) {
  DCHECK_LE(buffers_queue.size(), 1u)
      << "DemuxerStreamAdapter only reads a single-buffer.";
  OnNewBuffer(status,
              buffers_queue.empty() ? nullptr : std::move(buffers_queue[0]));
}

void DemuxerStreamAdapter::OnNewBuffer(DemuxerStream::Status status,
                                       scoped_refptr<DecoderBuffer> input) {
  DEMUXER_VLOG(3) << "status=" << status;
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
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
      DCHECK(!pending_frame_);
      if (!data_pipe_writer_.IsPipeValid())
        return;  // Do not start sending (due to previous fatal error).
      pending_frame_ = std::move(input);
      WriteFrame();
    } break;
  }
}

void DemuxerStreamAdapter::WriteFrame() {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(!pending_flush_);
  DCHECK(is_processing_read_request());
  DCHECK(pending_frame_);

  if (!stream_sender_ || !data_pipe_writer_.IsPipeValid()) {
    DEMUXER_VLOG(1) << "Ignore since data pipe stream sender is invalid";
    return;
  }

  // Unretained is safe because `this` owns the mojo::Remote.
  stream_sender_->SendFrame(
      media::mojom::DecoderBuffer::From(*pending_frame_),
      base::BindOnce(&DemuxerStreamAdapter::OnWrittenFrameRead,
                     base::Unretained(this)));

  if (!pending_frame_->end_of_stream()) {
    data_pipe_writer_.Write(
        pending_frame_->data(), pending_frame_->size(),
        base::BindOnce(&DemuxerStreamAdapter::OnFrameWritten,
                       base::Unretained(this)));
  } else {
    DemuxerStreamAdapter::OnFrameWritten(true);
  }
}

void DemuxerStreamAdapter::OnFrameWritten(bool success) {
  if (!success) {
    OnFatalError(DATA_PIPE_WRITE_ERROR);
    return;
  }

  was_pending_frame_written_ = true;
  TryCompleteFrameWrite();
}

void DemuxerStreamAdapter::OnWrittenFrameRead() {
  was_pending_frame_read_ = true;
  TryCompleteFrameWrite();
}

void DemuxerStreamAdapter::TryCompleteFrameWrite() {
  if (!was_pending_frame_written_ || !was_pending_frame_read_) {
    return;
  }

  // Resets frame buffer variables.
  const bool pending_frame_is_eos = pending_frame_->end_of_stream();
  if (!pending_frame_is_eos) {
    bytes_written_to_pipe_ += pending_frame_->size();
  }

  ++last_count_;
  ResetPendingFrame();

  // Checks if it needs to send RPC_DS_READUNTIL_CALLBACK RPC message.
  if (read_until_count_ == last_count_ || pending_frame_is_eos) {
    SendReadAck();
    return;
  }

  // Contiune to read decoder buffer until reaching |read_until_count_| or
  // end of stream.
  media_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&DemuxerStreamAdapter::RequestBuffer,
                                weak_factory_.GetWeakPtr()));
}

void DemuxerStreamAdapter::SendReadAck() {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  DEMUXER_VLOG(3) << "last_count_=" << last_count_
                  << ", remote_read_callback_handle="
                  << read_until_callback_handle_
                  << ", media_status=" << media_status_;
  // Issues RPC_DS_READUNTIL_CALLBACK RPC message.
  std::unique_ptr<openscreen::cast::RpcMessage> rpc(
      new openscreen::cast::RpcMessage());
  rpc->set_handle(read_until_callback_handle_);
  rpc->set_proc(openscreen::cast::RpcMessage::RPC_DS_READUNTIL_CALLBACK);
  auto* message = rpc->mutable_demuxerstream_readuntilcb_rpc();
  message->set_count(last_count_);
  message->set_status(
      media::cast::ToProtoDemuxerStreamStatus(media_status_).value());
  if (media_status_ == DemuxerStream::kConfigChanged) {
    if (audio_config_.IsValidConfig()) {
      openscreen::cast::AudioDecoderConfig* audio_message =
          message->mutable_audio_decoder_config();
      media::cast::ConvertAudioDecoderConfigToProto(audio_config_,
                                                    audio_message);
    } else if (video_config_.IsValidConfig()) {
      openscreen::cast::VideoDecoderConfig* video_message =
          message->mutable_video_decoder_config();
      media::cast::ConvertVideoDecoderConfigToProto(video_config_,
                                                    video_message);
    } else {
      NOTREACHED_IN_MIGRATION();
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
      FROM_HERE,
      base::BindOnce(&RpcMessenger::SendMessageToRemote, rpc_messenger_, *rpc));
  // Resets callback handle after completing the reading request.
  read_until_callback_handle_ = RpcMessenger::kInvalidHandle;

  // Resets audio/video decoder config since it only sends once.
  if (audio_config_.IsValidConfig())
    audio_config_ = AudioDecoderConfig();
  if (video_config_.IsValidConfig())
    video_config_ = VideoDecoderConfig();
}

void DemuxerStreamAdapter::ResetPendingFrame() {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  pending_frame_.reset();
  was_pending_frame_read_ = false;
  was_pending_frame_written_ = false;
}

void DemuxerStreamAdapter::OnFatalError(StopTrigger stop_trigger) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());

  DEMUXER_VLOG(1) << __func__ << " with StopTrigger " << stop_trigger;

  if (error_callback_.is_null())
    return;

  data_pipe_writer_.Close();

  std::move(error_callback_).Run(stop_trigger);
}

void DemuxerStreamAdapter::RegisterForRpcMessaging() {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  auto receive_callback =
      base::BindPostTaskToCurrentDefault(base::BindRepeating(
          &DemuxerStreamAdapter::OnReceivedRpc, weak_factory_.GetWeakPtr()));
  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &RegisterForRpcTask, rpc_messenger_, rpc_handle_,
          [cb = std::move(receive_callback)](
              std::unique_ptr<openscreen::cast::RpcMessage> message) {
            cb.Run(std::move(message));
          }));
}

void DemuxerStreamAdapter::DeregisterFromRpcMessaging() {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  if (rpc_messenger_) {
    main_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&DeregisterFromRpcTask, rpc_messenger_, rpc_handle_));
  }
}

}  // namespace remoting
}  // namespace media
