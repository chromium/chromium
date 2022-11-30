// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/remoting/mock_receiver_controller.h"

#include "base/check.h"
#include "base/no_destructor.h"
#include "media/mojo/common/mojo_decoder_buffer_converter.h"
#include "media/remoting/test_utils.h"
#include "mojo/public/cpp/system/data_pipe.h"

namespace media {
namespace remoting {

MockRemotee::MockRemotee() = default;

MockRemotee::~MockRemotee() = default;

void MockRemotee::BindMojoReceiver(mojo::PendingReceiver<Remotee> receiver) {
  DCHECK(receiver);
  receiver_.Bind(std::move(receiver));
}

void MockRemotee::SendAudioFrame(uint32_t frame_count,
                                 scoped_refptr<DecoderBuffer> buffer) {
  mojom::DecoderBufferPtr mojo_buffer =
      audio_buffer_writer_->WriteDecoderBuffer(std::move(buffer));
  audio_stream_->ReceiveFrame(frame_count, std::move(mojo_buffer));
}

void MockRemotee::SendVideoFrame(uint32_t frame_count,
                                 scoped_refptr<DecoderBuffer> buffer) {
  mojom::DecoderBufferPtr mojo_buffer =
      video_buffer_writer_->WriteDecoderBuffer(std::move(buffer));
  video_stream_->ReceiveFrame(frame_count, std::move(mojo_buffer));
}

void MockRemotee::OnRemotingSinkReady(
    mojo::PendingRemote<mojom::RemotingSink> remoting_sink) {
  DCHECK(remoting_sink);
  remoting_sink_.Bind(std::move(remoting_sink));
}

void MockRemotee::SendMessageToSource(const std::vector<uint8_t>& message) {}

void MockRemotee::StartDataStreams(
    mojo::PendingRemote<mojom::RemotingDataStreamReceiver> audio_stream,
    mojo::PendingRemote<mojom::RemotingDataStreamReceiver> video_stream) {
  if (audio_stream.is_valid()) {
    // Initialize data pipe for audio data stream receiver.
    audio_stream_.Bind(std::move(audio_stream));
    mojo::ScopedDataPipeConsumerHandle audio_data_pipe;
    audio_buffer_writer_ = MojoDecoderBufferWriter::Create(
        GetDefaultDecoderBufferConverterCapacity(DemuxerStream::AUDIO),
        &audio_data_pipe);
    audio_stream_->InitializeDataPipe(std::move(audio_data_pipe));
  }

  if (video_stream.is_valid()) {
    // Initialize data pipe for video data stream receiver.
    video_stream_.Bind(std::move(video_stream));
    mojo::ScopedDataPipeConsumerHandle video_data_pipe;
    video_buffer_writer_ = MojoDecoderBufferWriter::Create(
        GetDefaultDecoderBufferConverterCapacity(DemuxerStream::VIDEO),
        &video_data_pipe);
    video_stream_->InitializeDataPipe(std::move(video_data_pipe));
  }
}

void MockRemotee::OnFlushUntil(uint32_t audio_count, uint32_t video_count) {
  flush_audio_count_ = audio_count;
  flush_video_count_ = video_count;

  if (audio_stream_.is_bound()) {
    audio_stream_->FlushUntil(audio_count);
  }
  if (video_stream_.is_bound()) {
    video_stream_->FlushUntil(video_count);
  }
}

void MockRemotee::OnVideoNaturalSizeChange(const gfx::Size& size) {
  DCHECK(!size.IsEmpty());
  changed_size_ = size;
}

void MockRemotee::Reset() {
  audio_stream_.reset();
  video_stream_.reset();
  receiver_.reset();
  remoting_sink_.reset();
}

// static
MockReceiverController* MockReceiverController::GetInstance() {
  static base::NoDestructor<MockReceiverController> controller;
  ResetForTesting(controller.get());
  controller->mock_remotee_->Reset();
  return controller.get();
}

MockReceiverController::MockReceiverController()
    : mock_remotee_(new MockRemotee()) {
  // Overwrites |rpc_messenger_|.
  rpc_messenger_.set_send_message_cb_for_testing(
      [this](std::vector<uint8_t> message) { OnSendRpc(message); });
}

MockReceiverController::~MockReceiverController() = default;

void MockReceiverController::OnSendRpc(std::vector<uint8_t> message) {
  ReceiverController::OnMessageFromSource(message);
}

}  // namespace remoting
}  // namespace media
