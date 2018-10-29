// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/remoting/stream_provider.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/containers/circular_deque.h"
#include "base/logging.h"
#include "media/base/decoder_buffer.h"
#include "media/base/video_rotation.h"
#include "media/remoting/proto_enum_utils.h"
#include "media/remoting/proto_utils.h"

namespace media {
namespace remoting {

namespace {
// The number of frames requested in each ReadUntil RPC message.
constexpr int kNumFramesInEachReadUntil = 10;
}

// An implementation of media::DemuxerStream on Media Remoting receiver.
// Receives data from mojo data pipe, and returns one frame or/and status when
// Read() is called.
class MediaStream final : public DemuxerStream {
 public:
  MediaStream(RpcBroker* rpc_broker,
              Type type,
              int remote_handle,
              const base::Closure& error_callback);
  ~MediaStream() override;

  // DemuxerStream implementation.
  void Read(const ReadCB& read_cb) override;
  AudioDecoderConfig audio_decoder_config() override;
  VideoDecoderConfig video_decoder_config() override;
  DemuxerStream::Type type() const override;
  Liveness liveness() const override;
  bool SupportsConfigChanges() override;

  void Initialize(const base::Closure& init_done_cb);
  void FlushUntil(int count);
  void AppendBuffer(scoped_refptr<DecoderBuffer> buffer);

 private:
  // RPC messages handlers.
  void OnReceivedRpc(std::unique_ptr<pb::RpcMessage> message);
  void OnInitializeCallback(std::unique_ptr<pb::RpcMessage> message);
  void OnReadUntilCallback(std::unique_ptr<pb::RpcMessage> message);

  // Issues the ReadUntil RPC message when read is pending and buffer is empty.
  void SendReadUntil();

  // Run and reset the read callback.
  void CompleteRead(DemuxerStream::Status status);

  // Update the audio/video decoder config When config changes in the mid
  // stream, the new config will be stored in
  // |next_audio/video_decoder_config_|. Old config will be droped when all
  // associated frames are consumed.
  void UpdateConfig(const pb::AudioDecoderConfig* audio_message,
                    const pb::VideoDecoderConfig* video_message);

  // Called when any error occurs.
  void OnError(const std::string& error);

  RpcBroker* const rpc_broker_;  // Outlives this class.
  const Type type_;
  const int remote_handle_;
  const int rpc_handle_;

  // Set when Initialize() is called, and will be run only once after
  // initialization is done.
  base::Closure init_done_callback_;

  // The read until count in the last ReadUntil RPC message.
  int last_read_until_count_ = 0;

  // Indicates whether Audio/VideoDecoderConfig changed and the frames with the
  // old config are not yet consumed. The new config is stored in the end of
  // |audio/video_decoder_config_|;
  bool config_changed_ = false;

  // Indicates whether a ReadUntil RPC message was sent without receiving the
  // ReadUntilCallback message yet.
  bool read_until_sent_ = false;

  // Set when Read() is called. Run only once when read completes.
  ReadCB read_complete_callback_;

  base::Closure error_callback_;  // Called only once when first error occurs.

  base::circular_deque<scoped_refptr<DecoderBuffer>> buffers_;

  // Current audio/video config.
  AudioDecoderConfig audio_decoder_config_;
  VideoDecoderConfig video_decoder_config_;

  // Stores the new auido/video config when config changes.
  AudioDecoderConfig next_audio_decoder_config_;
  VideoDecoderConfig next_video_decoder_config_;

  base::WeakPtrFactory<MediaStream> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MediaStream);
};

MediaStream::MediaStream(RpcBroker* rpc_broker,
                         Type type,
                         int remote_handle,
                         const base::Closure& error_callback)
    : rpc_broker_(rpc_broker),
      type_(type),
      remote_handle_(remote_handle),
      rpc_handle_(rpc_broker_->GetUniqueHandle()),
      error_callback_(error_callback),
      weak_factory_(this) {
  DCHECK(remote_handle_ != RpcBroker::kInvalidHandle);
  rpc_broker_->RegisterMessageReceiverCallback(
      rpc_handle_,
      base::Bind(&MediaStream::OnReceivedRpc, weak_factory_.GetWeakPtr()));
}

MediaStream::~MediaStream() {
  rpc_broker_->UnregisterMessageReceiverCallback(rpc_handle_);
}

void MediaStream::Initialize(const base::Closure& init_done_cb) {
  DCHECK(init_done_cb);
  if (!init_done_callback_.is_null()) {
    OnError("Duplicate initialization");
    return;
  }
  init_done_callback_ = init_done_cb;

  DVLOG(3) << __func__ << "Issues RpcMessage::RPC_DS_INITIALIZE with "
           << "remote_handle=" << remote_handle_
           << " rpc_handle=" << rpc_handle_;
  std::unique_ptr<pb::RpcMessage> rpc(new pb::RpcMessage());
  rpc->set_handle(remote_handle_);
  rpc->set_proc(pb::RpcMessage::RPC_DS_INITIALIZE);
  rpc->set_integer_value(rpc_handle_);
  rpc_broker_->SendMessageToRemote(std::move(rpc));
}

void MediaStream::OnReceivedRpc(std::unique_ptr<pb::RpcMessage> message) {
  DCHECK(message->handle() == rpc_handle_);

  switch (message->proc()) {
    case pb::RpcMessage::RPC_DS_INITIALIZE_CALLBACK:
      OnInitializeCallback(std::move(message));
      break;
    case pb::RpcMessage::RPC_DS_READUNTIL_CALLBACK:
      OnReadUntilCallback(std::move(message));
      break;
    default:
      VLOG(3) << __func__ << "Unknow RPC message.";
  }
}

void MediaStream::OnInitializeCallback(
    std::unique_ptr<pb::RpcMessage> message) {
  DVLOG(3) << __func__ << "Receives RPC_DS_INITIALIZE_CALLBACK message.";
  const pb::DemuxerStreamInitializeCallback callback_message =
      message->demuxerstream_initializecb_rpc();
  if (callback_message.type() != type_) {
    OnError("Wrong type");
    return;
  }
  if ((type_ == DemuxerStream::AUDIO &&
       audio_decoder_config_.IsValidConfig()) ||
      (type_ == DemuxerStream::VIDEO &&
       video_decoder_config_.IsValidConfig())) {
    OnError("Duplicate Iniitialize");
    return;
  }
  if (init_done_callback_.is_null()) {
    OnError("Iniitialize callback missing");
    return;
  }

  if (type_ == DemuxerStream::AUDIO &&
      callback_message.has_audio_decoder_config()) {
    const pb::AudioDecoderConfig audio_message =
        callback_message.audio_decoder_config();
    UpdateConfig(&audio_message, nullptr);
  } else if (type_ == DemuxerStream::VIDEO &&
             callback_message.has_video_decoder_config()) {
    const pb::VideoDecoderConfig video_message =
        callback_message.video_decoder_config();
    UpdateConfig(nullptr, &video_message);
  } else {
    OnError("config missing");
    return;
  }
  std::move(init_done_callback_).Run();
}

void MediaStream::OnReadUntilCallback(std::unique_ptr<pb::RpcMessage> message) {
  DVLOG(3) << __func__ << ": Receives RPC_DS_READUNTIL_CALLBACK message.";
  if (!read_until_sent_) {
    OnError("Unexpected ReadUntilCallback");
    return;
  }
  read_until_sent_ = false;
  const pb::DemuxerStreamReadUntilCallback callback_message =
      message->demuxerstream_readuntilcb_rpc();
  last_read_until_count_ = callback_message.count();
  if (ToDemuxerStreamStatus(callback_message.status()) == kConfigChanged) {
    config_changed_ = true;
    if (callback_message.has_audio_decoder_config()) {
      const pb::AudioDecoderConfig audio_message =
          callback_message.audio_decoder_config();
      UpdateConfig(&audio_message, nullptr);
    }
    if (callback_message.has_video_decoder_config()) {
      const pb::VideoDecoderConfig video_message =
          callback_message.video_decoder_config();
      UpdateConfig(nullptr, &video_message);
    }
    if (buffers_.empty() && !read_complete_callback_.is_null())
      CompleteRead(DemuxerStream::kConfigChanged);
    return;
  }
  if (buffers_.empty() && !read_complete_callback_.is_null())
    SendReadUntil();
}

void MediaStream::UpdateConfig(const pb::AudioDecoderConfig* audio_message,
                               const pb::VideoDecoderConfig* video_message) {
  if (type_ == AUDIO) {
    DCHECK(audio_message && !video_message);
    AudioDecoderConfig audio_config;
    ConvertProtoToAudioDecoderConfig(*audio_message, &audio_config);
    if (!audio_config.IsValidConfig()) {
      OnError("Invalid audio config");
      return;
    }
    if (config_changed_) {
      DCHECK(audio_decoder_config_.IsValidConfig());
      DCHECK(!next_audio_decoder_config_.IsValidConfig());
      next_audio_decoder_config_ = audio_config;
    } else {
      DCHECK(!audio_decoder_config_.IsValidConfig());
      audio_decoder_config_ = audio_config;
    }
  } else if (type_ == VIDEO) {
    DCHECK(video_message && !audio_message);
    VideoDecoderConfig video_config;
    ConvertProtoToVideoDecoderConfig(*video_message, &video_config);
    if (!video_config.IsValidConfig()) {
      OnError("Invalid video config");
      return;
    }
    if (config_changed_) {
      DCHECK(video_decoder_config_.IsValidConfig());
      DCHECK(!next_video_decoder_config_.IsValidConfig());
      next_video_decoder_config_ = video_config;
    } else {
      DCHECK(!video_decoder_config_.IsValidConfig());
      video_decoder_config_ = video_config;
    }
  } else {
    NOTREACHED() << ": Only supports video or audio stream.";
  }
}

void MediaStream::SendReadUntil() {
  if (read_until_sent_)
    return;
  DVLOG(3) << "Issues RPC_DS_READUNTIL RPC message to remote_handle_="
           << remote_handle_ << " with callback handle=" << rpc_handle_
           << " count=" << last_read_until_count_;

  std::unique_ptr<pb::RpcMessage> rpc(new pb::RpcMessage());
  rpc->set_handle(remote_handle_);
  rpc->set_proc(pb::RpcMessage::RPC_DS_READUNTIL);
  auto* message = rpc->mutable_demuxerstream_readuntil_rpc();
  last_read_until_count_ += kNumFramesInEachReadUntil;
  message->set_count(last_read_until_count_);
  message->set_callback_handle(rpc_handle_);
  rpc_broker_->SendMessageToRemote(std::move(rpc));
  read_until_sent_ = true;
}

void MediaStream::FlushUntil(int count) {
  while (!buffers_.empty()) {
    buffers_.pop_front();
  }

  last_read_until_count_ = count;
  if (!read_complete_callback_.is_null())
    CompleteRead(DemuxerStream::kAborted);
  read_until_sent_ = false;
}

void MediaStream::Read(const ReadCB& read_cb) {
  DCHECK(read_complete_callback_.is_null());
  DCHECK(read_cb);
  read_complete_callback_ = read_cb;
  if (buffers_.empty() && config_changed_) {
    CompleteRead(DemuxerStream::kConfigChanged);
    return;
  }

  // Wait for more data.
  if (buffers_.empty()) {
    SendReadUntil();
    return;
  }

  CompleteRead(DemuxerStream::kOk);
}

void MediaStream::CompleteRead(DemuxerStream::Status status) {
  DVLOG(3) << __func__ << ": " << status;
  switch (status) {
    case DemuxerStream::kConfigChanged:
      if (type_ == AUDIO) {
        DCHECK(next_audio_decoder_config_.IsValidConfig());
        audio_decoder_config_ = next_audio_decoder_config_;
#if DCHECK_IS_ON()
        next_audio_decoder_config_ = AudioDecoderConfig();
#endif  // DCHECK_IS_ON()
      } else {
        DCHECK(next_video_decoder_config_.IsValidConfig());
        video_decoder_config_ = next_video_decoder_config_;
#if DCHECK_IS_ON()
        next_video_decoder_config_ = VideoDecoderConfig();
#endif  // DCHECK_IS_ON()
      }
      config_changed_ = false;
      std::move(read_complete_callback_).Run(status, nullptr);
      return;
    case DemuxerStream::kAborted:
    case DemuxerStream::kError:
      std::move(read_complete_callback_).Run(status, nullptr);
      return;
    case DemuxerStream::kOk:
      DCHECK(!buffers_.empty());
      scoped_refptr<DecoderBuffer> frame_data = buffers_.front();
      buffers_.pop_front();
      std::move(read_complete_callback_).Run(status, frame_data);
      return;
  }
}

AudioDecoderConfig MediaStream::audio_decoder_config() {
  DVLOG(3) << __func__;
  DCHECK(type_ == DemuxerStream::AUDIO);
  return audio_decoder_config_;
}

VideoDecoderConfig MediaStream::video_decoder_config() {
  DVLOG(3) << __func__;
  DCHECK(type_ == DemuxerStream::VIDEO);
  return video_decoder_config_;
}

DemuxerStream::Type MediaStream::type() const {
  return type_;
}

DemuxerStream::Liveness MediaStream::liveness() const {
  return DemuxerStream::LIVENESS_LIVE;
}

bool MediaStream::SupportsConfigChanges() {
  return true;
}

void MediaStream::AppendBuffer(scoped_refptr<DecoderBuffer> buffer) {
  DVLOG(3) << __func__;
  buffers_.push_back(buffer);
  if (!read_complete_callback_.is_null())
    CompleteRead(DemuxerStream::kOk);
}

void MediaStream::OnError(const std::string& error) {
  VLOG(1) << __func__ << ": " << error;
  if (error_callback_.is_null())
    return;
  std::move(error_callback_).Run();
}

StreamProvider::StreamProvider(RpcBroker* rpc_broker,
                               const base::Closure& error_callback)
    : rpc_broker_(rpc_broker),
      error_callback_(error_callback),
      weak_factory_(this) {}

StreamProvider::~StreamProvider() = default;

void StreamProvider::Initialize(int remote_audio_handle,
                                int remote_video_handle,
                                const base::Closure& callback) {
  DVLOG(3) << __func__ << ": remote_audio_handle=" << remote_audio_handle
           << " remote_video_handle=" << remote_video_handle;
  if (!init_done_callback_.is_null()) {
    OnError("Duplicate initialization.");
    return;
  }
  if (remote_audio_handle == RpcBroker::kInvalidHandle &&
      remote_video_handle == RpcBroker::kInvalidHandle) {
    OnError("Invalid handle.");
    return;
  }

  init_done_callback_ = callback;
  if (remote_audio_handle != RpcBroker::kInvalidHandle) {
    audio_stream_.reset(new MediaStream(
        rpc_broker_, DemuxerStream::AUDIO, remote_audio_handle,
        base::Bind(&StreamProvider::OnError, weak_factory_.GetWeakPtr(),
                   "Media stream error")));
    audio_stream_->Initialize(base::Bind(
        &StreamProvider::AudioStreamInitialized, weak_factory_.GetWeakPtr()));
  }
  if (remote_video_handle != RpcBroker::kInvalidHandle) {
    video_stream_.reset(new MediaStream(
        rpc_broker_, DemuxerStream::VIDEO, remote_video_handle,
        base::Bind(&StreamProvider::OnError, weak_factory_.GetWeakPtr(),
                   "Media stream error")));
    video_stream_->Initialize(base::Bind(
        &StreamProvider::VideoStreamInitialized, weak_factory_.GetWeakPtr()));
  }
}

void StreamProvider::OnError(const std::string& error) {
  VLOG(1) << __func__ << ": " << error;
  if (error_callback_.is_null())
    return;
  std::move(error_callback_).Run();
}

void StreamProvider::AudioStreamInitialized() {
  DCHECK(!init_done_callback_.is_null());
  audio_stream_initialized_ = true;
  if (video_stream_initialized_ || !video_stream_)
    std::move(init_done_callback_).Run();
}

void StreamProvider::VideoStreamInitialized() {
  DCHECK(!init_done_callback_.is_null());
  video_stream_initialized_ = true;
  if (audio_stream_initialized_ || !audio_stream_)
    std::move(init_done_callback_).Run();
}

std::vector<DemuxerStream*> StreamProvider::GetAllStreams() {
  std::vector<DemuxerStream*> streams;
  if (audio_stream_)
    streams.push_back(audio_stream_.get());
  if (video_stream_)
    streams.push_back(video_stream_.get());
  return streams;
}

void StreamProvider::AppendBuffer(DemuxerStream::Type type,
                                  scoped_refptr<DecoderBuffer> buffer) {
  if (type == DemuxerStream::AUDIO)
    audio_stream_->AppendBuffer(buffer);
  else if (type == DemuxerStream::VIDEO)
    video_stream_->AppendBuffer(buffer);
  else
    NOTREACHED() << ": Only supports video or audio stream.";
}

void StreamProvider::FlushUntil(DemuxerStream::Type type, int count) {
  DVLOG(3) << __func__ << ": type=" << type << " count=" << count;
  if (type == DemuxerStream::AUDIO)
    audio_stream_->FlushUntil(count);
  else if (type == DemuxerStream::VIDEO)
    video_stream_->FlushUntil(count);
  else
    NOTREACHED() << ": Only supports video or audio stream.";
}

}  // namespace remoting
}  // namespace media
