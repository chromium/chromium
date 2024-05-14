// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/remoting/stream_provider.h"

#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/demuxer_stream.h"
#include "media/base/media_util.h"
#include "media/base/test_helpers.h"
#include "media/base/video_decoder_config.h"
#include "media/cast/openscreen/remoting_proto_enum_utils.h"
#include "media/cast/openscreen/remoting_proto_utils.h"
#include "media/remoting/mock_receiver_controller.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using openscreen::cast::RpcMessenger;
using testing::NiceMock;

namespace {
constexpr int kBufferSize = 10;
}  // namespace

namespace media {
namespace remoting {

class StreamProviderTest : public testing::Test {
 public:
  StreamProviderTest()
      : audio_config_(TestAudioConfig::Normal()),
        video_config_(TestVideoConfig::Normal()),
        audio_buffer_(new DecoderBuffer(kBufferSize)),
        video_buffer_(DecoderBuffer::CreateEOSBuffer()) {}

  void SetUp() override {
    mock_controller_ = MockReceiverController::GetInstance();
    mock_controller_->Initialize(
        mock_controller_->mock_remotee()->BindNewPipeAndPassRemote());
    mock_remotee_ = mock_controller_->mock_remotee();
    stream_provider_ = std::make_unique<StreamProvider>(
        mock_controller_, base::SingleThreadTaskRunner::GetCurrentDefault());

    rpc_messenger_ = mock_controller_->rpc_messenger();
    sender_audio_demuxer_stream_handle_ = rpc_messenger_->GetUniqueHandle();
    sender_video_demuxer_stream_handle_ = rpc_messenger_->GetUniqueHandle();
    rpc_messenger_->RegisterMessageReceiverCallback(
        sender_audio_demuxer_stream_handle_,
        [this](std::unique_ptr<openscreen::cast::RpcMessage> message) {
          OnDemuxerStreamReceivedRpc(DemuxerStream::Type::AUDIO,
                                     std::move(message));
        });
    rpc_messenger_->RegisterMessageReceiverCallback(
        sender_video_demuxer_stream_handle_,
        [this](std::unique_ptr<openscreen::cast::RpcMessage> message) {
          OnDemuxerStreamReceivedRpc(DemuxerStream::Type::VIDEO,
                                     std::move(message));
        });
  }

  void TearDown() override {
    // Drop unowned references before `stream_provider_` destroys them.
    audio_stream_ = nullptr;
    video_stream_ = nullptr;

    stream_provider_.reset();
    task_environment_.RunUntilIdle();
  }

  void OnDemuxerStreamReceivedRpc(
      DemuxerStream::Type type,
      std::unique_ptr<openscreen::cast::RpcMessage> message) {
    DCHECK(message);
    switch (message->proc()) {
      case openscreen::cast::RpcMessage::RPC_DS_INITIALIZE:
        if (type == DemuxerStream::Type::AUDIO) {
          receiver_audio_demuxer_stream_handle_ = message->integer_value();
        } else if (type == DemuxerStream::Type::VIDEO) {
          receiver_video_demuxer_stream_handle_ = message->integer_value();
        } else {
          NOTREACHED_IN_MIGRATION();
        }

        RpcInitializeCallback(type);
        break;

      case openscreen::cast::RpcMessage::RPC_DS_READUNTIL:
        ReadUntil(type);
        break;

      default:
        DVLOG(1) << __func__ << "Unknown supported message.";
    }
  }

  void RpcInitializeCallback(DemuxerStream::Type type) {
    // Issues RPC_DS_INITIALIZE_CALLBACK RPC message.
    auto rpc = std::make_unique<openscreen::cast::RpcMessage>();
    rpc->set_handle(type == DemuxerStream::Type::AUDIO
                        ? receiver_audio_demuxer_stream_handle_
                        : receiver_video_demuxer_stream_handle_);
    rpc->set_proc(openscreen::cast::RpcMessage::RPC_DS_INITIALIZE_CALLBACK);
    auto* init_cb_message = rpc->mutable_demuxerstream_initializecb_rpc();
    init_cb_message->set_type(type);

    switch (type) {
      case DemuxerStream::Type::AUDIO: {
        openscreen::cast::AudioDecoderConfig* audio_message =
            init_cb_message->mutable_audio_decoder_config();
        media::cast::ConvertAudioDecoderConfigToProto(audio_config_,
                                                      audio_message);
        break;
      }

      case DemuxerStream::Type::VIDEO: {
        openscreen::cast::VideoDecoderConfig* video_message =
            init_cb_message->mutable_video_decoder_config();
        media::cast::ConvertVideoDecoderConfigToProto(video_config_,
                                                      video_message);
        break;
      }

      default:
        NOTREACHED_IN_MIGRATION();
    }

    rpc_messenger_->SendMessageToRemote(*rpc);
  }

  void ReadUntil(DemuxerStream::Type type) {
    switch (type) {
      case DemuxerStream::Type::AUDIO:
        SendAudioFrame();
        break;
      case DemuxerStream::Type::VIDEO:
        SendVideoFrame();
        break;
      default:
        NOTREACHED_IN_MIGRATION();
    }
  }

  void SendRpcAcquireDemuxer() {
    auto rpc = std::make_unique<openscreen::cast::RpcMessage>();
    rpc->set_handle(RpcMessenger::kAcquireDemuxerHandle);
    rpc->set_proc(openscreen::cast::RpcMessage::RPC_ACQUIRE_DEMUXER);
    openscreen::cast::AcquireDemuxer* message =
        rpc->mutable_acquire_demuxer_rpc();
    message->set_audio_demuxer_handle(sender_audio_demuxer_stream_handle_);
    message->set_video_demuxer_handle(sender_video_demuxer_stream_handle_);
    rpc_messenger_->SendMessageToRemote(*rpc);
  }

  void OnStreamProviderInitialized(PipelineStatus status) {
    EXPECT_EQ(PIPELINE_OK, status);
    stream_provider_initialized_ = true;
    audio_stream_ =
        stream_provider_->GetFirstStream(DemuxerStream::Type::AUDIO);
    video_stream_ =
        stream_provider_->GetFirstStream(DemuxerStream::Type::VIDEO);

    EXPECT_TRUE(audio_stream_);
    EXPECT_TRUE(video_stream_);
  }

  void InitializeDemuxer() {
    DCHECK(stream_provider_);
    stream_provider_->Initialize(
        nullptr,
        base::BindOnce(&StreamProviderTest::OnStreamProviderInitialized,
                       base::Unretained(this)));
  }

  void SendAudioFrame() {
    mock_remotee_->SendAudioFrame(0, audio_buffer_);
    SendRpcReadUntilCallback(DemuxerStream::Type::AUDIO);
  }

  void SendVideoFrame() {
    mock_remotee_->SendVideoFrame(0, video_buffer_);
    SendRpcReadUntilCallback(DemuxerStream::Type::VIDEO);
  }

  void SendRpcReadUntilCallback(DemuxerStream::Type type) {
    // Issues RPC_DS_READUNTIL_CALLBACK RPC message.
    openscreen::cast::RpcMessage rpc;
    rpc.set_handle(type == DemuxerStream::Type::AUDIO
                       ? receiver_audio_demuxer_stream_handle_
                       : receiver_video_demuxer_stream_handle_);
    rpc.set_proc(openscreen::cast::RpcMessage::RPC_DS_READUNTIL_CALLBACK);
    auto* message = rpc.mutable_demuxerstream_readuntilcb_rpc();
    message->set_count(0);
    message->set_status(
        media::cast::ToProtoDemuxerStreamStatus(DemuxerStream::Status::kOk)
            .value());
    rpc_messenger_->SendMessageToRemote(rpc);
  }

  void FlushUntil(uint32_t flush_audio_count, uint32_t flush_video_count) {
    mock_remotee_->OnFlushUntil(flush_audio_count, flush_video_count);
  }

  uint32_t GetAudioCurrentFrameCount() {
    return stream_provider_->audio_stream_->current_frame_count_;
  }

  uint32_t GetVideoCurrentFrameCount() {
    return stream_provider_->video_stream_->current_frame_count_;
  }

  void OnBufferReadFromDemuxerStream(
      DemuxerStream::Type type,
      DemuxerStream::Status status,
      DemuxerStream::DecoderBufferVector buffers) {
    EXPECT_EQ(status, DemuxerStream::Status::kOk);
    EXPECT_EQ(buffers.size(), 1u)
        << "StreamProviderTest only reads a single-buffer.";
    scoped_refptr<DecoderBuffer> buffer = std::move(buffers[0]);
    switch (type) {
      case DemuxerStream::Type::AUDIO:
        received_audio_buffer_ = buffer;
        break;
      case DemuxerStream::Type::VIDEO:
        received_video_buffer_ = buffer;
        break;
      default:
        NOTREACHED_IN_MIGRATION();
    }
  }

  base::test::TaskEnvironment task_environment_;

  AudioDecoderConfig audio_config_;
  VideoDecoderConfig video_config_;

  raw_ptr<DemuxerStream> audio_stream_;
  raw_ptr<DemuxerStream> video_stream_;

  scoped_refptr<DecoderBuffer> audio_buffer_;
  scoped_refptr<DecoderBuffer> video_buffer_;

  bool stream_provider_initialized_{false};
  scoped_refptr<DecoderBuffer> received_audio_buffer_;
  scoped_refptr<DecoderBuffer> received_video_buffer_;

  int sender_audio_demuxer_stream_handle_ = RpcMessenger::kInvalidHandle;
  int sender_video_demuxer_stream_handle_ = RpcMessenger::kInvalidHandle;
  int receiver_audio_demuxer_stream_handle_ = RpcMessenger::kInvalidHandle;
  int receiver_video_demuxer_stream_handle_ = RpcMessenger::kInvalidHandle;

  raw_ptr<RpcMessenger> rpc_messenger_;
  raw_ptr<MockReceiverController> mock_controller_;
  raw_ptr<MockRemotee> mock_remotee_;
  std::unique_ptr<StreamProvider> stream_provider_;
};

TEST_F(StreamProviderTest, InitializeBeforeRpcAcquireDemuxer) {
  InitializeDemuxer();
  EXPECT_FALSE(stream_provider_initialized_);

  SendRpcAcquireDemuxer();
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(mock_remotee_->audio_stream_.is_bound());
  EXPECT_TRUE(mock_remotee_->video_stream_.is_bound());
  EXPECT_TRUE(stream_provider_initialized_);

  // 1 audio stream and 1 video stream
  EXPECT_EQ(size_t(2), stream_provider_->GetAllStreams().size());
}

TEST_F(StreamProviderTest, InitializeAfterRpcAcquireDemuxer) {
  SendRpcAcquireDemuxer();
  EXPECT_FALSE(stream_provider_initialized_);

  InitializeDemuxer();
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(mock_remotee_->audio_stream_.is_bound());
  EXPECT_TRUE(mock_remotee_->video_stream_.is_bound());
  EXPECT_TRUE(stream_provider_initialized_);

  // 1 audio stream and 1 video stream
  EXPECT_EQ(size_t(2), stream_provider_->GetAllStreams().size());
}

TEST_F(StreamProviderTest, ReadBuffer) {
  InitializeDemuxer();
  SendRpcAcquireDemuxer();
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(mock_remotee_->audio_stream_.is_bound());
  EXPECT_TRUE(mock_remotee_->video_stream_.is_bound());
  EXPECT_TRUE(stream_provider_initialized_);

  audio_stream_->Read(
      1, base::BindOnce(&StreamProviderTest::OnBufferReadFromDemuxerStream,
                        base::Unretained(this), DemuxerStream::Type::AUDIO));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(audio_buffer_->size(), received_audio_buffer_->size());
  EXPECT_EQ(audio_buffer_->end_of_stream(),
            received_audio_buffer_->end_of_stream());
  EXPECT_EQ(audio_buffer_->is_key_frame(),
            received_audio_buffer_->is_key_frame());

  video_stream_->Read(
      1, base::BindOnce(&StreamProviderTest::OnBufferReadFromDemuxerStream,
                        base::Unretained(this), DemuxerStream::Type::VIDEO));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(video_buffer_->end_of_stream(),
            received_video_buffer_->end_of_stream());
}

TEST_F(StreamProviderTest, FlushUntil) {
  InitializeDemuxer();
  SendRpcAcquireDemuxer();
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(mock_remotee_->audio_stream_.is_bound());
  EXPECT_TRUE(mock_remotee_->video_stream_.is_bound());
  EXPECT_TRUE(stream_provider_initialized_);

  uint32_t flush_audio_count = 10;
  uint32_t flush_video_count = 20;
  FlushUntil(flush_audio_count, flush_video_count);
  task_environment_.RunUntilIdle();

  EXPECT_EQ(GetAudioCurrentFrameCount(), flush_audio_count);
  EXPECT_EQ(GetVideoCurrentFrameCount(), flush_video_count);
}

}  // namespace remoting
}  // namespace media
