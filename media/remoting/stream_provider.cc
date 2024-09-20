// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/remoting/stream_provider.h"

#include <vector>

#include "base/containers/circular_deque.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "media/base/decoder_buffer.h"
#include "media/base/demuxer.h"
#include "media/base/video_transformation.h"
#include "media/cast/openscreen/remoting_proto_enum_utils.h"
#include "media/cast/openscreen/remoting_proto_utils.h"
#include "media/mojo/common/mojo_decoder_buffer_converter.h"
#include "media/remoting/receiver_controller.h"
#include "third_party/openscreen/src/cast/streaming/public/rpc_messenger.h"

using openscreen::cast::RpcMessenger;

namespace media {
namespace remoting {

namespace {
// The number of frames requested in each ReadUntil RPC message.
constexpr int kNumFramesInEachReadUntil = 10;
}

// static
void StreamProvider::MediaStream::CreateOnMainThread(
    RpcMessenger* rpc_messenger,
    Type type,
    int32_t handle,
    const scoped_refptr<base::SequencedTaskRunner>& media_task_runner,
    base::OnceCallback<void(MediaStream::UniquePtr)> callback) {
  MediaStream::UniquePtr stream(
      new MediaStream(rpc_messenger, type, handle, media_task_runner),
      &DestructionHelper);
  std::move(callback).Run(std::move(stream));
}

// static
void StreamProvider::MediaStream::DestructionHelper(MediaStream* stream) {
  stream->Destroy();
}

StreamProvider::MediaStream::MediaStream(
    RpcMessenger* rpc_messenger,
    Type type,
    int remote_handle,
    const scoped_refptr<base::SequencedTaskRunner>& media_task_runner)
    : main_task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()),
      media_task_runner_(media_task_runner),
      rpc_messenger_(rpc_messenger),
      type_(type),
      remote_handle_(remote_handle),
      rpc_handle_(rpc_messenger_->GetUniqueHandle()) {
  DCHECK(remote_handle_ != RpcMessenger::kInvalidHandle);

  media_weak_this_ = media_weak_factory_.GetWeakPtr();

  auto receive_callback = base::BindPostTask(
      media_task_runner_,
      BindRepeating(&MediaStream::OnReceivedRpc, media_weak_this_));
  rpc_messenger_->RegisterMessageReceiverCallback(
      rpc_handle_, [cb = std::move(receive_callback)](
                       std::unique_ptr<openscreen::cast::RpcMessage> message) {
        cb.Run(std::move(message));
      });
}

StreamProvider::MediaStream::~MediaStream() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  rpc_messenger_->UnregisterMessageReceiverCallback(rpc_handle_);
}

void StreamProvider::MediaStream::Destroy() {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());

  // Invalid weak pointers to prevent |this| from receiving RPC calls on the
  // media thread.
  media_weak_factory_.InvalidateWeakPtrs();

  // Unbind all mojo pipes and bindings.
  receiver_.reset();
  decoder_buffer_reader_.reset();

  // After invalidating all weak ptrs of |media_weak_factory_|, MediaStream
  // won't be access anymore, so using |this| here is safe.
  main_task_runner_->DeleteSoon(FROM_HERE, this);
}

void StreamProvider::MediaStream::SendRpcMessageOnMainThread(
    std::unique_ptr<openscreen::cast::RpcMessage> message) {
  // |rpc_messenger_| is owned by |receiver_controller_| which is a singleton
  // per process, so it's safe to use Unretained() here.
  main_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&RpcMessenger::SendMessageToRemote,
                                base::Unretained(rpc_messenger_), *message));
}

void StreamProvider::MediaStream::Initialize(
    base::OnceClosure init_done_callback) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(init_done_callback);

  if (init_done_callback_) {
    OnError("Duplicate initialization");
    return;
  }

  init_done_callback_ = std::move(init_done_callback);

  auto rpc = std::make_unique<openscreen::cast::RpcMessage>();
  rpc->set_handle(remote_handle_);
  rpc->set_proc(openscreen::cast::RpcMessage::RPC_DS_INITIALIZE);
  rpc->set_integer_value(rpc_handle_);
  SendRpcMessageOnMainThread(std::move(rpc));
}

void StreamProvider::MediaStream::InitializeDataPipe(
    mojo::ScopedDataPipeConsumerHandle data_pipe) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());

  decoder_buffer_reader_ =
      std::make_unique<MojoDecoderBufferReader>(std::move(data_pipe));
  CompleteInitialize();
}

void StreamProvider::MediaStream::ReceiveFrame(uint32_t count,
                                               mojom::DecoderBufferPtr buffer) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(decoder_buffer_reader_);

  auto callback = base::BindPostTaskToCurrentDefault(
      base::BindOnce(&MediaStream::AppendBuffer, media_weak_this_, count));
  decoder_buffer_reader_->ReadDecoderBuffer(std::move(buffer),
                                            std::move(callback));
}

void StreamProvider::MediaStream::FlushUntil(uint32_t count) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());

  if (count < current_frame_count_)
    return;

  uint32_t buffers_to_erase = count - current_frame_count_;

  if (buffers_to_erase > buffers_.size()) {
    buffers_.clear();
  } else {
    buffers_.erase(buffers_.begin(), buffers_.begin() + buffers_to_erase);
  }

  current_frame_count_ = count;

  if (!read_complete_callback_.is_null())
    CompleteRead(DemuxerStream::kAborted);

  read_until_sent_ = false;
}

void StreamProvider::MediaStream::OnReceivedRpc(
    std::unique_ptr<openscreen::cast::RpcMessage> message) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(message->handle() == rpc_handle_);

  switch (message->proc()) {
    case openscreen::cast::RpcMessage::RPC_DS_INITIALIZE_CALLBACK:
      OnInitializeCallback(std::move(message));
      break;
    case openscreen::cast::RpcMessage::RPC_DS_READUNTIL_CALLBACK:
      OnReadUntilCallback(std::move(message));
      break;
    default:
      VLOG(3) << __func__ << "Unknown RPC message.";
  }
}

void StreamProvider::MediaStream::OnInitializeCallback(
    std::unique_ptr<openscreen::cast::RpcMessage> message) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  const openscreen::cast::DemuxerStreamInitializeCallback callback_message =
      message->demuxerstream_initializecb_rpc();
  if (callback_message.type() != type_) {
    OnError("Wrong type");
    return;
  }

  if ((type_ == DemuxerStream::AUDIO &&
       audio_decoder_config_.IsValidConfig()) ||
      (type_ == DemuxerStream::VIDEO &&
       video_decoder_config_.IsValidConfig())) {
    OnError("Duplicate initialization");
    return;
  }

  if (type_ == DemuxerStream::AUDIO &&
      callback_message.has_audio_decoder_config()) {
    const openscreen::cast::AudioDecoderConfig audio_message =
        callback_message.audio_decoder_config();
    media::cast::ConvertProtoToAudioDecoderConfig(audio_message,
                                                  &audio_decoder_config_);
    if (!audio_decoder_config_.IsValidConfig()) {
      OnError("Invalid audio config");
      return;
    }
  } else if (type_ == DemuxerStream::VIDEO &&
             callback_message.has_video_decoder_config()) {
    const openscreen::cast::VideoDecoderConfig video_message =
        callback_message.video_decoder_config();
    media::cast::ConvertProtoToVideoDecoderConfig(video_message,
                                                  &video_decoder_config_);
    if (!video_decoder_config_.IsValidConfig()) {
      OnError("Invalid video config");
      return;
    }
  } else {
    OnError("Config missing");
    return;
  }

  rpc_initialized_ = true;
  CompleteInitialize();
}

void StreamProvider::MediaStream::CompleteInitialize() {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());

  // Initialization finished when received RPC_DS_INITIALIZE_CALLBACK and
  // |decoder_buffer_reader_| is created.
  if (!rpc_initialized_ || !decoder_buffer_reader_)
    return;

  if (!init_done_callback_) {
    OnError("Initialize callback missing");
    return;
  }

  std::move(init_done_callback_).Run();
}

void StreamProvider::MediaStream::OnReadUntilCallback(
    std::unique_ptr<openscreen::cast::RpcMessage> message) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());

  if (!read_until_sent_) {
    OnError("Unexpected ReadUntilCallback");
    return;
  }
  read_until_sent_ = false;
  const openscreen::cast::DemuxerStreamReadUntilCallback callback_message =
      message->demuxerstream_readuntilcb_rpc();
  total_received_frame_count_ = callback_message.count();

  if (media::cast::ToDemuxerStreamStatus(callback_message.status()) ==
      kConfigChanged) {
    if (callback_message.has_audio_decoder_config()) {
      const openscreen::cast::AudioDecoderConfig audio_message =
          callback_message.audio_decoder_config();
      UpdateAudioConfig(audio_message);
    }

    if (callback_message.has_video_decoder_config()) {
      const openscreen::cast::VideoDecoderConfig video_message =
          callback_message.video_decoder_config();
      UpdateVideoConfig(video_message);
    }

    if (buffers_.empty() && !read_complete_callback_.is_null())
      CompleteRead(DemuxerStream::kConfigChanged);

    return;
  }

  if (buffers_.empty() && !read_complete_callback_.is_null())
    SendReadUntil();
}

void StreamProvider::MediaStream::UpdateAudioConfig(
    const openscreen::cast::AudioDecoderConfig& audio_message) {
  DCHECK(type_ == AUDIO);
  AudioDecoderConfig audio_config;
  media::cast::ConvertProtoToAudioDecoderConfig(audio_message, &audio_config);
  if (!audio_config.IsValidConfig()) {
    OnError("Invalid audio config");
    return;
  }
  next_audio_decoder_config_ = audio_config;
}

void StreamProvider::MediaStream::UpdateVideoConfig(
    const openscreen::cast::VideoDecoderConfig& video_message) {
  DCHECK(type_ == VIDEO);
  VideoDecoderConfig video_config;
  media::cast::ConvertProtoToVideoDecoderConfig(video_message, &video_config);
  if (!video_config.IsValidConfig()) {
    OnError("Invalid video config");
    return;
  }
  next_video_decoder_config_ = video_config;
}

void StreamProvider::MediaStream::SendReadUntil() {
  if (read_until_sent_)
    return;

  std::unique_ptr<openscreen::cast::RpcMessage> rpc(
      new openscreen::cast::RpcMessage());
  rpc->set_handle(remote_handle_);
  rpc->set_proc(openscreen::cast::RpcMessage::RPC_DS_READUNTIL);
  auto* message = rpc->mutable_demuxerstream_readuntil_rpc();
  message->set_count(total_received_frame_count_ + kNumFramesInEachReadUntil);
  message->set_callback_handle(rpc_handle_);
  SendRpcMessageOnMainThread(std::move(rpc));
  read_until_sent_ = true;
}

// Only return one buffer at a time so we ignore the count.
void StreamProvider::MediaStream::Read(uint32_t /*count*/, ReadCB read_cb) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(read_complete_callback_.is_null());
  DCHECK(read_cb);

  read_complete_callback_ = std::move(read_cb);
  if (buffers_.empty() && (next_audio_decoder_config_.IsValidConfig() ||
                           next_video_decoder_config_.IsValidConfig())) {
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

void StreamProvider::MediaStream::CompleteRead(DemuxerStream::Status status) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());

  switch (status) {
    case DemuxerStream::kConfigChanged:
      if (next_audio_decoder_config_.IsValidConfig()) {
        audio_decoder_config_ = next_audio_decoder_config_;
        next_audio_decoder_config_ = media::AudioDecoderConfig();
      }
      if (next_video_decoder_config_.IsValidConfig()) {
        video_decoder_config_ = next_video_decoder_config_;
        next_video_decoder_config_ = media::VideoDecoderConfig();
      }
      std::move(read_complete_callback_).Run(status, {});
      return;
    case DemuxerStream::kAborted:
    case DemuxerStream::kError:
      std::move(read_complete_callback_).Run(status, {});
      return;
    case DemuxerStream::kOk:
      DCHECK(read_complete_callback_);
      DCHECK(!buffers_.empty());
      DCHECK_LT(current_frame_count_, buffered_frame_count_);
      scoped_refptr<DecoderBuffer> frame_data = buffers_.front();
      buffers_.pop_front();
      ++current_frame_count_;
      std::move(read_complete_callback_).Run(status, {frame_data});
      return;
  }
}

AudioDecoderConfig StreamProvider::MediaStream::audio_decoder_config() {
  DCHECK(type_ == DemuxerStream::AUDIO);
  return audio_decoder_config_;
}

VideoDecoderConfig StreamProvider::MediaStream::video_decoder_config() {
  DCHECK(type_ == DemuxerStream::VIDEO);
  return video_decoder_config_;
}

DemuxerStream::Type StreamProvider::MediaStream::type() const {
  return type_;
}

StreamLiveness StreamProvider::MediaStream::liveness() const {
  return StreamLiveness::kLive;
}

bool StreamProvider::MediaStream::SupportsConfigChanges() {
  return true;
}

void StreamProvider::MediaStream::AppendBuffer(
    uint32_t count,
    scoped_refptr<DecoderBuffer> buffer) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());

  // Drop flushed frame.
  if (count < current_frame_count_)
    return;

  // Continuity check.
  DCHECK(buffers_.empty() || buffered_frame_count_ == count);

  buffers_.push_back(buffer);
  buffered_frame_count_ = count + 1;

  if (!read_complete_callback_.is_null())
    CompleteRead(DemuxerStream::kOk);
}

void StreamProvider::MediaStream::OnError(const std::string& error) {
  auto rpc = std::make_unique<openscreen::cast::RpcMessage>();
  rpc->set_handle(remote_handle_);
  rpc->set_proc(openscreen::cast::RpcMessage::RPC_DS_ONERROR);
  SendRpcMessageOnMainThread(std::move(rpc));
}

StreamProvider::StreamProvider(
    ReceiverController* receiver_controller,
    const scoped_refptr<base::SequencedTaskRunner>& media_task_runner)
    : main_task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()),
      media_task_runner_(media_task_runner),
      receiver_controller_(receiver_controller),
      rpc_messenger_(receiver_controller_->rpc_messenger()) {
  DCHECK(receiver_controller_);
  DCHECK(rpc_messenger_);

  media_weak_this_ = media_weak_factory_.GetWeakPtr();

  auto callback = base::BindPostTask(
      media_task_runner_,
      base::BindRepeating(&StreamProvider::OnReceivedRpc, media_weak_this_));
  rpc_messenger_->RegisterMessageReceiverCallback(
      RpcMessenger::kAcquireDemuxerHandle,
      [cb = std::move(callback)](
          std::unique_ptr<openscreen::cast::RpcMessage> message) {
        cb.Run(std::move(message));
      });
}

StreamProvider::~StreamProvider() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  rpc_messenger_->UnregisterMessageReceiverCallback(
      RpcMessenger::kAcquireDemuxerHandle);
}

std::string StreamProvider::GetDisplayName() const {
  return "media::remoting::StreamProvider";
}

DemuxerType StreamProvider::GetDemuxerType() const {
  return DemuxerType::kStreamProviderDemuxer;
}

void StreamProvider::Initialize(DemuxerHost* host,
                                PipelineStatusCallback status_cb) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  init_done_callback_ = std::move(status_cb);
  CompleteInitialize();
}

void StreamProvider::AbortPendingReads() {}

void StreamProvider::StartWaitingForSeek(base::TimeDelta seek_time) {}

void StreamProvider::CancelPendingSeek(base::TimeDelta seek_time) {}

void StreamProvider::Seek(base::TimeDelta time,
                          PipelineStatusCallback seek_cb) {
  media_task_runner_->PostTask(FROM_HERE,
                               base::BindOnce(std::move(seek_cb), PIPELINE_OK));
}

bool StreamProvider::IsSeekable() const {
  return false;
}

void StreamProvider::Stop() {}

base::TimeDelta StreamProvider::GetStartTime() const {
  return base::TimeDelta();
}

base::Time StreamProvider::GetTimelineOffset() const {
  return base::Time();
}

int64_t StreamProvider::GetMemoryUsage() const {
  return 0;
}

std::optional<container_names::MediaContainerName>
StreamProvider::GetContainerForMetrics() const {
  return std::optional<container_names::MediaContainerName>();
}

void StreamProvider::OnEnabledAudioTracksChanged(
    const std::vector<MediaTrack::Id>& track_ids,
    base::TimeDelta curr_time,
    TrackChangeCB change_completed_cb) {
  std::vector<DemuxerStream*> streams;
  std::move(change_completed_cb).Run(streams);
  DVLOG(1) << "Track changes are not supported.";
}

void StreamProvider::OnSelectedVideoTrackChanged(
    const std::vector<media::MediaTrack::Id>& track_ids,
    base::TimeDelta curr_time,
    TrackChangeCB change_completed_cb) {
  std::vector<DemuxerStream*> streams;
  std::move(change_completed_cb).Run(streams);
  DVLOG(1) << "Track changes are not supported.";
}

void StreamProvider::Destroy() {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());

  if (init_done_callback_)
    std::move(init_done_callback_).Run(PIPELINE_ERROR_ABORT);

  // Invalid weak pointers to prevent |this| from receiving RPC calls on the
  // media thread.
  media_weak_factory_.InvalidateWeakPtrs();

  audio_stream_.reset();
  video_stream_.reset();

  // After invalidating all weak ptrs of |media_weak_factory_|, StreamProvider
  // won't be access anymore, so using |this| here is safe.
  main_task_runner_->DeleteSoon(FROM_HERE, this);
}

void StreamProvider::OnReceivedRpc(
    std::unique_ptr<openscreen::cast::RpcMessage> message) {
  switch (message->proc()) {
    case openscreen::cast::RpcMessage::RPC_ACQUIRE_DEMUXER:
      OnAcquireDemuxer(std::move(message));
      break;
    default:
      VLOG(3) << __func__ << "Unknown RPC message.";
  }
}

void StreamProvider::OnAcquireDemuxer(
    std::unique_ptr<openscreen::cast::RpcMessage> message) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(message->has_acquire_demuxer_rpc());

  int32_t audio_demuxer_handle =
      message->acquire_demuxer_rpc().audio_demuxer_handle();
  int32_t video_demuxer_handle =
      message->acquire_demuxer_rpc().video_demuxer_handle();
  has_audio_ = audio_demuxer_handle != RpcMessenger::kInvalidHandle;
  has_video_ = video_demuxer_handle != RpcMessenger::kInvalidHandle;

  DCHECK(has_audio_ || has_video_);

  if (has_audio_) {
    auto callback = base::BindPostTaskToCurrentDefault(base::BindOnce(
        &StreamProvider::OnAudioStreamCreated, media_weak_this_));
    main_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&MediaStream::CreateOnMainThread, rpc_messenger_,
                       DemuxerStream::AUDIO, audio_demuxer_handle,
                       media_task_runner_, std::move(callback)));
  }

  if (has_video_) {
    auto callback = base::BindPostTaskToCurrentDefault(base::BindOnce(
        &StreamProvider::OnVideoStreamCreated, media_weak_this_));
    main_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&MediaStream::CreateOnMainThread, rpc_messenger_,
                       DemuxerStream::VIDEO, video_demuxer_handle,
                       media_task_runner_, std::move(callback)));
  }
}

void StreamProvider::OnAudioStreamCreated(MediaStream::UniquePtr stream) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  audio_stream_ = std::move(stream);
  audio_stream_->Initialize(base::BindOnce(
      &StreamProvider::OnAudioStreamInitialized, media_weak_this_));
  InitializeDataPipe();
}

void StreamProvider::OnVideoStreamCreated(MediaStream::UniquePtr stream) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  video_stream_ = std::move(stream);
  video_stream_->Initialize(base::BindOnce(
      &StreamProvider::OnVideoStreamInitialized, media_weak_this_));
  InitializeDataPipe();
}

void StreamProvider::InitializeDataPipe() {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());

  if ((has_audio_ && !audio_stream_) || (has_video_ && !video_stream_))
    return;

  receiver_controller_->StartDataStreams(
      has_audio_ ? audio_stream_->BindNewPipeAndPassRemote()
                 : mojo::NullRemote(),
      has_video_ ? video_stream_->BindNewPipeAndPassRemote()
                 : mojo::NullRemote());
}

void StreamProvider::OnAudioStreamInitialized() {
  audio_stream_initialized_ = true;
  CompleteInitialize();
}

void StreamProvider::OnVideoStreamInitialized() {
  video_stream_initialized_ = true;
  CompleteInitialize();
}

void StreamProvider::CompleteInitialize() {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());

  // Haven't receive RpcAcquireRenderer message
  if (!has_audio_ && !has_video_)
    return;

  if ((has_audio_ && !audio_stream_initialized_) ||
      (has_video_ && !video_stream_initialized_) || !init_done_callback_)
    return;

  // |init_done_callback_| should be called on |media_task_runner_|.
  std::move(init_done_callback_).Run(PIPELINE_OK);
}

std::vector<DemuxerStream*> StreamProvider::GetAllStreams() {
  std::vector<DemuxerStream*> streams;
  if (audio_stream_) {
    streams.push_back(audio_stream_.get());
  }
  if (video_stream_) {
    streams.push_back(video_stream_.get());
  }
  return streams;
}

}  // namespace remoting
}  // namespace media

namespace std {

void default_delete<media::remoting::StreamProvider>::operator()(
    media::remoting::StreamProvider* ptr) const {
  ptr->Destroy();
}

}  // namespace std
