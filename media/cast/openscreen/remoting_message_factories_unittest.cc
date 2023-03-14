// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/openscreen/remoting_message_factories.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/ref_counted.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/decoder_buffer.h"
#include "media/base/demuxer_stream.h"
#include "media/base/encryption_scheme.h"
#include "media/base/test_helpers.h"
#include "media/base/video_decoder_config.h"
#include "media/cast/openscreen/remoting_proto_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/openscreen/src/cast/streaming/remoting.pb.h"

using testing::_;
using testing::Invoke;
using testing::Return;

namespace media::cast {

class RemotingMessageFactoriesTest : public testing::Test {
 protected:
  void SetUp() override {}
};

TEST_F(RemotingMessageFactoriesTest, CreateMessageForError) {
  auto rpc = CreateMessageForError();
  EXPECT_EQ(rpc->proc(), openscreen::cast::RpcMessage::RPC_RC_ONERROR);
}

TEST_F(RemotingMessageFactoriesTest, CreateMessageForMediaEnded) {
  auto rpc = CreateMessageForMediaEnded();
  EXPECT_EQ(rpc->proc(), openscreen::cast::RpcMessage::RPC_RC_ONENDED);
}

TEST_F(RemotingMessageFactoriesTest, CreateMessageForStatisticsUpdate) {
  media::PipelineStatistics stats;
  stats.audio_bytes_decoded = 1;
  stats.video_bytes_decoded = 2;
  stats.video_frames_decoded = 3;
  stats.video_frames_dropped = 5;
  stats.audio_memory_usage = 8;
  stats.video_memory_usage = 13;

  auto rpc = CreateMessageForStatisticsUpdate(stats);
  EXPECT_EQ(rpc->proc(),
            openscreen::cast::RpcMessage::RPC_RC_ONSTATISTICSUPDATE);
  auto* message = rpc->mutable_rendererclient_onstatisticsupdate_rpc();
  ASSERT_NE(message, nullptr);
  EXPECT_EQ(message->audio_bytes_decoded(), stats.audio_bytes_decoded);
  EXPECT_EQ(message->video_bytes_decoded(), stats.video_bytes_decoded);
  EXPECT_EQ(message->video_frames_decoded(), stats.video_frames_decoded);
  EXPECT_EQ(message->video_frames_dropped(), stats.video_frames_dropped);
  EXPECT_EQ(message->audio_memory_usage(), stats.audio_memory_usage);
  EXPECT_EQ(message->video_memory_usage(), stats.video_memory_usage);
}

TEST_F(RemotingMessageFactoriesTest, CreateMessageForBufferingStateChange) {
  auto rpc = CreateMessageForBufferingStateChange(
      media::BufferingState::BUFFERING_HAVE_NOTHING);
  EXPECT_EQ(rpc->proc(),
            openscreen::cast::RpcMessage::RPC_RC_ONBUFFERINGSTATECHANGE);
  auto* message = rpc->mutable_rendererclient_onbufferingstatechange_rpc();
  ASSERT_NE(message, nullptr);
  EXPECT_EQ(message->state(),
            openscreen::cast::RendererClientOnBufferingStateChange::
                BUFFERING_HAVE_NOTHING);

  rpc = CreateMessageForBufferingStateChange(
      media::BufferingState::BUFFERING_HAVE_ENOUGH);
  EXPECT_EQ(rpc->proc(),
            openscreen::cast::RpcMessage::RPC_RC_ONBUFFERINGSTATECHANGE);
  message = rpc->mutable_rendererclient_onbufferingstatechange_rpc();
  ASSERT_NE(message, nullptr);
  EXPECT_EQ(message->state(),
            openscreen::cast::RendererClientOnBufferingStateChange::
                BUFFERING_HAVE_ENOUGH);
}

TEST_F(RemotingMessageFactoriesTest, CreateMessageForAudioConfigChange) {
  const char extra_data[4] = {'A', 'C', 'E', 'G'};
  media::AudioDecoderConfig audio_config(
      media::AudioCodec::kOpus, media::kSampleFormatF32,
      media::CHANNEL_LAYOUT_MONO, 48000,
      std::vector<uint8_t>(std::begin(extra_data), std::end(extra_data)),
      media::EncryptionScheme::kUnencrypted);
  ASSERT_TRUE(audio_config.IsValidConfig());

  auto rpc = CreateMessageForAudioConfigChange(audio_config);
  EXPECT_EQ(rpc->proc(),
            openscreen::cast::RpcMessage::RPC_RC_ONAUDIOCONFIGCHANGE);
  auto* message = rpc->mutable_rendererclient_onaudioconfigchange_rpc();
  ASSERT_NE(message, nullptr);
  openscreen::cast::AudioDecoderConfig* proto_audio_config =
      message->mutable_audio_decoder_config();
  ASSERT_NE(proto_audio_config, nullptr);

  media::AudioDecoderConfig audio_output_config;
  ASSERT_TRUE(ConvertProtoToAudioDecoderConfig(*proto_audio_config,
                                               &audio_output_config));

  ASSERT_TRUE(audio_config.Matches(audio_output_config));
}

TEST_F(RemotingMessageFactoriesTest, CreateMessageForVideoConfigChange) {
  const media::VideoDecoderConfig video_config =
      media::TestVideoConfig::Normal();
  ASSERT_TRUE(video_config.IsValidConfig());

  auto rpc = CreateMessageForVideoConfigChange(video_config);
  EXPECT_EQ(rpc->proc(),
            openscreen::cast::RpcMessage::RPC_RC_ONVIDEOCONFIGCHANGE);
  auto* message = rpc->mutable_rendererclient_onvideoconfigchange_rpc();
  ASSERT_NE(message, nullptr);
  openscreen::cast::VideoDecoderConfig* proto_video_config =
      message->mutable_video_decoder_config();
  ASSERT_NE(proto_video_config, nullptr);

  media::VideoDecoderConfig converted;
  ASSERT_TRUE(
      ConvertProtoToVideoDecoderConfig(*proto_video_config, &converted));
  ASSERT_TRUE(converted.Matches(video_config));
}

TEST_F(RemotingMessageFactoriesTest, CreateMessageForVideoNaturalSizeChange) {
  auto rpc = CreateMessageForVideoNaturalSizeChange({42, 24});
  EXPECT_EQ(rpc->proc(),
            openscreen::cast::RpcMessage::RPC_RC_ONVIDEONATURALSIZECHANGE);
  auto* message = rpc->mutable_rendererclient_onvideonatualsizechange_rpc();
  ASSERT_NE(message, nullptr);
  EXPECT_EQ(message->width(), 42);
  EXPECT_EQ(message->height(), 24);
}

TEST_F(RemotingMessageFactoriesTest, CreateMessageForVideoOpacityChange) {
  auto rpc = CreateMessageForVideoOpacityChange(true);
  EXPECT_EQ(rpc->proc(),
            openscreen::cast::RpcMessage::RPC_RC_ONVIDEOOPACITYCHANGE);
  EXPECT_EQ(rpc->boolean_value(), true);

  rpc = CreateMessageForVideoOpacityChange(false);
  EXPECT_EQ(rpc->proc(),
            openscreen::cast::RpcMessage::RPC_RC_ONVIDEOOPACITYCHANGE);
  EXPECT_EQ(rpc->boolean_value(), false);
}

TEST_F(RemotingMessageFactoriesTest, CreateMessageForMediaTimeUpdate) {
  auto rpc = CreateMessageForMediaTimeUpdate(base::Microseconds(42));
  EXPECT_EQ(rpc->proc(), openscreen::cast::RpcMessage::RPC_RC_ONTIMEUPDATE);
  auto* message = rpc->mutable_rendererclient_ontimeupdate_rpc();
  ASSERT_NE(message, nullptr);
  EXPECT_EQ(message->time_usec(), 42);
  EXPECT_EQ(message->time_usec(), 42);
}

TEST_F(RemotingMessageFactoriesTest, CreateMessageForInitializationComplete) {
  auto rpc = CreateMessageForInitializationComplete(true);
  EXPECT_EQ(rpc->proc(),
            openscreen::cast::RpcMessage::RPC_R_INITIALIZE_CALLBACK);
  EXPECT_EQ(rpc->boolean_value(), true);

  rpc = CreateMessageForInitializationComplete(false);
  EXPECT_EQ(rpc->proc(),
            openscreen::cast::RpcMessage::RPC_R_INITIALIZE_CALLBACK);
  EXPECT_EQ(rpc->boolean_value(), false);
}

TEST_F(RemotingMessageFactoriesTest, CreateMessageForFlushComplete) {
  auto rpc = CreateMessageForFlushComplete();
  EXPECT_EQ(rpc->proc(),
            openscreen::cast::RpcMessage::RPC_R_FLUSHUNTIL_CALLBACK);
}

TEST_F(RemotingMessageFactoriesTest, CreateMessageForAcquireRendererDone) {
  int kTestHandle = 42;
  const auto rpc = CreateMessageForAcquireRendererDone(kTestHandle);
  EXPECT_EQ(rpc->proc(),
            openscreen::cast::RpcMessage::RPC_ACQUIRE_RENDERER_DONE);
  EXPECT_EQ(rpc->integer_value(), kTestHandle);
}

TEST_F(RemotingMessageFactoriesTest, CreateMessageForDemuxerStreamInitialize) {
  constexpr int kTestHandle = 42;
  const auto rpc = CreateMessageForDemuxerStreamInitialize(kTestHandle);
  EXPECT_EQ(rpc->proc(), openscreen::cast::RpcMessage::RPC_DS_INITIALIZE);
  EXPECT_EQ(rpc->integer_value(), kTestHandle);
}

TEST_F(RemotingMessageFactoriesTest, CreateMessageForDemuxerStreamReadUntil) {
  constexpr int kTestHandle = 42;
  constexpr uint32_t kTotalCount = 63;
  const auto rpc =
      CreateMessageForDemuxerStreamReadUntil(kTestHandle, kTotalCount);
  EXPECT_EQ(rpc->proc(), openscreen::cast::RpcMessage::RPC_DS_READUNTIL);
  ASSERT_TRUE(rpc->has_demuxerstream_readuntil_rpc());
  auto& message = rpc->demuxerstream_readuntil_rpc();
  EXPECT_EQ(message.count(), kTotalCount);
  EXPECT_EQ(message.callback_handle(), kTestHandle);
}

TEST_F(RemotingMessageFactoriesTest,
       CreateMessageForDemuxerStreamEnableBitstreamConverter) {
  const auto rpc = CreateMessageForDemuxerStreamEnableBitstreamConverter();
  EXPECT_EQ(rpc->proc(),
            openscreen::cast::RpcMessage::RPC_DS_ENABLEBITSTREAMCONVERTER);
}

TEST_F(RemotingMessageFactoriesTest, CreateMessageForDemuxerStreamError) {
  const auto rpc = CreateMessageForDemuxerStreamError();
  EXPECT_EQ(rpc->proc(), openscreen::cast::RpcMessage::RPC_DS_ONERROR);
}

}  // namespace media::cast
