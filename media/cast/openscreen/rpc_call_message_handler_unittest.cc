// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/openscreen/rpc_call_message_handler.h"

#include <memory>

#include "media/base/media_util.h"
#include "media/cast/openscreen/remoting_proto_enum_utils.h"
#include "media/cast/openscreen/remoting_proto_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/openscreen/src/cast/streaming/remoting.pb.h"

using testing::_;
using testing::Invoke;
using testing::Return;
using testing::StrictMock;

namespace media::cast {

class RpcCallMessageHandlerTest : public testing::Test {
 public:
  class MockRpcRendererCallMessageHandler
      : public RpcRendererCallMessageHandler {
   public:
    ~MockRpcRendererCallMessageHandler() override = default;

    MOCK_METHOD0(OnRpcInitialize, void());
    MOCK_METHOD2(OnRpcFlush, void(uint32_t, uint32_t));
    MOCK_METHOD1(OnRpcStartPlayingFrom, void(base::TimeDelta));
    MOCK_METHOD1(OnRpcSetPlaybackRate, void(double));
    MOCK_METHOD1(OnRpcSetVolume, void(double));
  };

  class MockRpcInitializationCallMessageHandler
      : public RpcInitializationCallMessageHandler {
   public:
    ~MockRpcInitializationCallMessageHandler() override = default;

    MOCK_METHOD1(OnRpcAcquireRenderer, void(int));
    MOCK_METHOD2(OnRpcAcquireDemuxer, void(int, int));
  };

  class MockRpcDemuxerStreamCBMessageHandler
      : public RpcDemuxerStreamCBMessageHandler {
   public:
    ~MockRpcDemuxerStreamCBMessageHandler() override = default;

    MOCK_METHOD3(OnRpcInitializeCallback,
                 void(int,
                      std::optional<media::AudioDecoderConfig>,
                      std::optional<media::VideoDecoderConfig>));
    MOCK_METHOD4(OnRpcReadUntilCallback,
                 void(int,
                      std::optional<media::AudioDecoderConfig>,
                      std::optional<media::VideoDecoderConfig>,
                      uint32_t));
    MOCK_METHOD2(OnRpcEnableBitstreamConverterCallback, void(int, bool));
  };

  RpcCallMessageHandlerTest() = default;

  media::AudioDecoderConfig test_audio_config_ =
      media::AudioDecoderConfig(media::AudioCodec::kAAC,
                                media::SampleFormat::kSampleFormatF32,
                                media::CHANNEL_LAYOUT_MONO,
                                10000,
                                media::EmptyExtraData(),
                                media::EncryptionScheme::kUnencrypted);
  media::VideoDecoderConfig test_video_config_ =
      media::VideoDecoderConfig(media::VideoCodec::kH264,
                                media::VideoCodecProfile::H264PROFILE_MAIN,
                                media::VideoDecoderConfig::AlphaMode::kIsOpaque,
                                media::VideoColorSpace::JPEG(),
                                media::VideoTransformation(),
                                {1920, 1080},
                                {1920, 1080},
                                {1920, 1080},
                                media::EmptyExtraData(),
                                media::EncryptionScheme::kUnencrypted);

  StrictMock<MockRpcRendererCallMessageHandler> renderer_client_;
  StrictMock<MockRpcInitializationCallMessageHandler> initialization_client_;
  StrictMock<MockRpcDemuxerStreamCBMessageHandler> demuxer_stream_client_;
};

TEST_F(RpcCallMessageHandlerTest, OnRpcInitialize) {
  auto rpc = std::make_unique<openscreen::cast::RpcMessage>();
  rpc->set_proc(openscreen::cast::RpcMessage::RPC_R_INITIALIZE);
  EXPECT_CALL(renderer_client_, OnRpcInitialize());
  EXPECT_TRUE(DispatchRendererRpcCall(rpc.get(), &renderer_client_));
}

TEST_F(RpcCallMessageHandlerTest, OnRpcFlush) {
  auto rpc = std::make_unique<openscreen::cast::RpcMessage>();
  rpc->set_proc(openscreen::cast::RpcMessage::RPC_R_FLUSHUNTIL);
  auto* flush_command = rpc->mutable_renderer_flushuntil_rpc();
  flush_command->set_audio_count(32);
  flush_command->set_video_count(42);
  EXPECT_CALL(renderer_client_, OnRpcFlush(32, 42));
  EXPECT_TRUE(DispatchRendererRpcCall(rpc.get(), &renderer_client_));
}

TEST_F(RpcCallMessageHandlerTest, OnRpcStartPlayingFrom) {
  auto rpc = std::make_unique<openscreen::cast::RpcMessage>();
  rpc->set_proc(openscreen::cast::RpcMessage::RPC_R_STARTPLAYINGFROM);
  rpc->set_integer64_value(42);
  EXPECT_CALL(renderer_client_, OnRpcStartPlayingFrom(base::Microseconds(42)));
  EXPECT_TRUE(DispatchRendererRpcCall(rpc.get(), &renderer_client_));
}

TEST_F(RpcCallMessageHandlerTest, OnRpcSetPlaybackRate) {
  auto rpc = std::make_unique<openscreen::cast::RpcMessage>();
  rpc->set_proc(openscreen::cast::RpcMessage::RPC_R_SETPLAYBACKRATE);
  rpc->set_double_value(112358.13);
  EXPECT_CALL(renderer_client_, OnRpcSetPlaybackRate(112358.13));
  EXPECT_TRUE(DispatchRendererRpcCall(rpc.get(), &renderer_client_));
}

TEST_F(RpcCallMessageHandlerTest, OnRpcSetVolume) {
  auto rpc = std::make_unique<openscreen::cast::RpcMessage>();
  rpc->set_proc(openscreen::cast::RpcMessage::RPC_R_SETVOLUME);
  rpc->set_double_value(112358.13);
  EXPECT_CALL(renderer_client_, OnRpcSetVolume(112358.13));
  EXPECT_TRUE(DispatchRendererRpcCall(rpc.get(), &renderer_client_));
}

TEST_F(RpcCallMessageHandlerTest, OnInvalidRendererMessage) {
  auto rpc = std::make_unique<openscreen::cast::RpcMessage>();
  rpc->set_proc(openscreen::cast::RpcMessage::RPC_ACQUIRE_DEMUXER);
  EXPECT_FALSE(DispatchRendererRpcCall(rpc.get(), &renderer_client_));
}

TEST_F(RpcCallMessageHandlerTest, OnRpcAcquireDemuxer) {
  auto rpc = std::make_unique<openscreen::cast::RpcMessage>();
  rpc->set_proc(openscreen::cast::RpcMessage::RPC_ACQUIRE_DEMUXER);
  auto* acquire_demuxer_command = rpc->mutable_acquire_demuxer_rpc();
  acquire_demuxer_command->set_audio_demuxer_handle(32);
  acquire_demuxer_command->set_video_demuxer_handle(42);
  EXPECT_CALL(initialization_client_, OnRpcAcquireDemuxer(32, 42));
  EXPECT_TRUE(
      DispatchInitializationRpcCall(rpc.get(), &initialization_client_));
}

TEST_F(RpcCallMessageHandlerTest, OnRpcAcquireRenderer) {
  auto rpc = std::make_unique<openscreen::cast::RpcMessage>();
  rpc->set_proc(openscreen::cast::RpcMessage::RPC_ACQUIRE_RENDERER);
  rpc->set_integer_value(42);
  EXPECT_CALL(initialization_client_, OnRpcAcquireRenderer(42));
  EXPECT_TRUE(
      DispatchInitializationRpcCall(rpc.get(), &initialization_client_));
}

TEST_F(RpcCallMessageHandlerTest, OnInvalidInitializationMessage) {
  auto rpc = std::make_unique<openscreen::cast::RpcMessage>();
  rpc->set_proc(openscreen::cast::RpcMessage::RPC_R_SETVOLUME);
  EXPECT_FALSE(
      DispatchInitializationRpcCall(rpc.get(), &initialization_client_));
}

TEST_F(RpcCallMessageHandlerTest, OnDemuxerStreamInitializeCallbackValid) {
  auto rpc = std::make_unique<openscreen::cast::RpcMessage>();
  constexpr int kHandle = 123;
  rpc->set_proc(openscreen::cast::RpcMessage::RPC_DS_INITIALIZE_CALLBACK);
  rpc->set_handle(kHandle);
  auto* initialize_cb = rpc->mutable_demuxerstream_initializecb_rpc();
  auto* audio_config = initialize_cb->mutable_audio_decoder_config();
  auto* video_config = initialize_cb->mutable_video_decoder_config();

  ConvertAudioDecoderConfigToProto(test_audio_config_, audio_config);
  ConvertVideoDecoderConfigToProto(test_video_config_, video_config);
  EXPECT_CALL(demuxer_stream_client_, OnRpcInitializeCallback(kHandle, _, _))
      .WillOnce([this](openscreen::cast::RpcMessenger::Handle handle,
                       std::optional<media::AudioDecoderConfig> audio_config,
                       std::optional<media::VideoDecoderConfig> video_config) {
        EXPECT_TRUE(audio_config.has_value());
        EXPECT_TRUE(test_audio_config_.Matches(audio_config.value()));
        EXPECT_TRUE(video_config.has_value());
        EXPECT_TRUE(test_video_config_.Matches(video_config.value()));
      });
  EXPECT_TRUE(
      DispatchDemuxerStreamCBRpcCall(rpc.get(), &demuxer_stream_client_));
}

TEST_F(RpcCallMessageHandlerTest, OnDemuxerStreamInitializeCallbackOneConfig) {
  auto rpc = std::make_unique<openscreen::cast::RpcMessage>();
  rpc->set_proc(openscreen::cast::RpcMessage::RPC_DS_INITIALIZE_CALLBACK);
  constexpr int kHandle = 123;
  rpc->set_handle(kHandle);
  auto* initialize_cb = rpc->mutable_demuxerstream_initializecb_rpc();
  auto* video_config = initialize_cb->mutable_video_decoder_config();
  ConvertVideoDecoderConfigToProto(test_video_config_, video_config);
  EXPECT_CALL(demuxer_stream_client_, OnRpcInitializeCallback(kHandle, _, _))
      .WillOnce([this](openscreen::cast::RpcMessenger::Handle handle,
                       std::optional<media::AudioDecoderConfig> audio_config,
                       std::optional<media::VideoDecoderConfig> video_config) {
        EXPECT_FALSE(audio_config.has_value());
        EXPECT_TRUE(video_config.has_value());
        EXPECT_TRUE(test_video_config_.Matches(video_config.value()));
      });
  EXPECT_TRUE(
      DispatchDemuxerStreamCBRpcCall(rpc.get(), &demuxer_stream_client_));
}

TEST_F(RpcCallMessageHandlerTest, OnDemuxerStreamInitializeCallbackNoConfig) {
  auto rpc = std::make_unique<openscreen::cast::RpcMessage>();
  rpc->set_proc(openscreen::cast::RpcMessage::RPC_DS_INITIALIZE_CALLBACK);
  rpc->set_handle(123);
  EXPECT_FALSE(
      DispatchDemuxerStreamCBRpcCall(rpc.get(), &demuxer_stream_client_));
}

TEST_F(RpcCallMessageHandlerTest, OnDemuxerStreamReadUntilCallbackValid) {
  constexpr int kHandle = 123;
  constexpr int kCount = 456;
  auto rpc = std::make_unique<openscreen::cast::RpcMessage>();
  rpc->set_proc(openscreen::cast::RpcMessage::RPC_DS_READUNTIL_CALLBACK);
  rpc->set_handle(kHandle);
  auto* readuntil_cb = rpc->mutable_demuxerstream_readuntilcb_rpc();
  auto* audio_config = readuntil_cb->mutable_audio_decoder_config();
  auto* video_config = readuntil_cb->mutable_video_decoder_config();

  ConvertAudioDecoderConfigToProto(test_audio_config_, audio_config);
  ConvertVideoDecoderConfigToProto(test_video_config_, video_config);
  readuntil_cb->set_count(kCount);
  readuntil_cb->set_status(
      ToProtoDemuxerStreamStatus(media::DemuxerStream::kConfigChanged).value());
  EXPECT_CALL(demuxer_stream_client_,
              OnRpcReadUntilCallback(kHandle, _, _, kCount))
      .WillOnce([this](openscreen::cast::RpcMessenger::Handle handle,
                       std::optional<media::AudioDecoderConfig> audio_config,
                       std::optional<media::VideoDecoderConfig> video_config,
                       uint32_t count) {
        EXPECT_TRUE(audio_config.has_value());
        EXPECT_TRUE(test_audio_config_.Matches(audio_config.value()));
        EXPECT_TRUE(video_config.has_value());
        EXPECT_TRUE(test_video_config_.Matches(video_config.value()));
      });
  EXPECT_TRUE(
      DispatchDemuxerStreamCBRpcCall(rpc.get(), &demuxer_stream_client_));
}

TEST_F(RpcCallMessageHandlerTest, OnDemuxerStreamReadUntilCallbackOneConfig) {
  constexpr int kHandle = 123;
  constexpr int kCount = 456;
  auto rpc = std::make_unique<openscreen::cast::RpcMessage>();
  rpc->set_proc(openscreen::cast::RpcMessage::RPC_DS_READUNTIL_CALLBACK);
  rpc->set_handle(kHandle);
  auto* readuntil_cb = rpc->mutable_demuxerstream_readuntilcb_rpc();
  auto* video_config = readuntil_cb->mutable_video_decoder_config();
  ConvertVideoDecoderConfigToProto(test_video_config_, video_config);
  readuntil_cb->set_count(kCount);
  readuntil_cb->set_status(
      ToProtoDemuxerStreamStatus(media::DemuxerStream::kConfigChanged).value());
  EXPECT_CALL(demuxer_stream_client_,
              OnRpcReadUntilCallback(kHandle, _, _, kCount))
      .WillOnce([this](openscreen::cast::RpcMessenger::Handle handle,
                       std::optional<media::AudioDecoderConfig> audio_config,
                       std::optional<media::VideoDecoderConfig> video_config,
                       uint32_t count) {
        EXPECT_FALSE(audio_config.has_value());
        EXPECT_TRUE(video_config.has_value());
        EXPECT_TRUE(test_video_config_.Matches(video_config.value()));
      });
  EXPECT_TRUE(
      DispatchDemuxerStreamCBRpcCall(rpc.get(), &demuxer_stream_client_));
}

TEST_F(RpcCallMessageHandlerTest, OnDemuxerStreamReadUntilCallbackNoConfig) {
  constexpr int kHandle = 123;
  auto rpc = std::make_unique<openscreen::cast::RpcMessage>();
  rpc->set_proc(openscreen::cast::RpcMessage::RPC_DS_READUNTIL_CALLBACK);
  rpc->set_handle(kHandle);
  auto* readuntil_cb = rpc->mutable_demuxerstream_readuntilcb_rpc();
  readuntil_cb->set_status(
      ToProtoDemuxerStreamStatus(media::DemuxerStream::kConfigChanged).value());
  EXPECT_FALSE(
      DispatchDemuxerStreamCBRpcCall(rpc.get(), &demuxer_stream_client_));
}

TEST_F(RpcCallMessageHandlerTest, OnDemuxerStreamReadUntilCallbackNonConfig) {
  constexpr int kHandle = 123;
  constexpr int kCount = 456;
  auto rpc = std::make_unique<openscreen::cast::RpcMessage>();
  rpc->set_proc(openscreen::cast::RpcMessage::RPC_DS_READUNTIL_CALLBACK);
  rpc->set_handle(kHandle);
  auto* readuntil_cb = rpc->mutable_demuxerstream_readuntilcb_rpc();
  readuntil_cb->set_count(kCount);
  readuntil_cb->set_status(
      ToProtoDemuxerStreamStatus(media::DemuxerStream::kOk).value());
  EXPECT_CALL(demuxer_stream_client_,
              OnRpcReadUntilCallback(kHandle, _, _, kCount))
      .WillOnce([](openscreen::cast::RpcMessenger::Handle handle,
                   std::optional<media::AudioDecoderConfig> audio_config,
                   std::optional<media::VideoDecoderConfig> video_config,
                   uint32_t count) {
        EXPECT_FALSE(audio_config.has_value());
        EXPECT_FALSE(video_config.has_value());
      });
  EXPECT_TRUE(
      DispatchDemuxerStreamCBRpcCall(rpc.get(), &demuxer_stream_client_));
}

TEST_F(RpcCallMessageHandlerTest, OnInvalidDemuxerStreamMessage) {
  auto rpc = std::make_unique<openscreen::cast::RpcMessage>();
  rpc->set_proc(openscreen::cast::RpcMessage::RPC_ACQUIRE_RENDERER);
  rpc->set_integer_value(42);
  EXPECT_FALSE(
      DispatchDemuxerStreamCBRpcCall(rpc.get(), &demuxer_stream_client_));
}

TEST_F(RpcCallMessageHandlerTest, OnRpcEnableBitstreamConverterCallback) {
  constexpr int kHandle = 123;
  auto rpc = std::make_unique<openscreen::cast::RpcMessage>();
  rpc->set_proc(
      openscreen::cast::RpcMessage::RPC_DS_ENABLEBITSTREAMCONVERTER_CALLBACK);
  rpc->set_handle(kHandle);
  rpc->set_boolean_value(true);
  EXPECT_CALL(demuxer_stream_client_,
              OnRpcEnableBitstreamConverterCallback(kHandle, true));
  EXPECT_TRUE(
      DispatchDemuxerStreamCBRpcCall(rpc.get(), &demuxer_stream_client_));
}

}  // namespace media::cast
