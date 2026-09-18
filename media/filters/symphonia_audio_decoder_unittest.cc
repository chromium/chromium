// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/symphonia_audio_decoder.h"

#include <stdint.h>

#include <memory>
#include <optional>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "media/base/audio_buffer.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/channel_layout.h"
#include "media/base/decoder_buffer.h"
#include "media/base/decoder_status.h"
#include "media/base/limits.h"
#include "media/base/media_switches.h"
#include "media/base/media_util.h"
#include "media/base/sample_format.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace {

rust::Vec<uint8_t> CreateZeroedRustVec(size_t len) {
  rust::Vec<uint8_t> data;
  data.reserve(len);
  for (size_t i = 0; i < len; ++i) {
    data.push_back(0);
  }
  return data;
}

SymphoniaAudioBuffer CreateValidSymphoniaBuffer(
    SymphoniaSampleFormat sample_format = SymphoniaSampleFormat::F32,
    size_t channel_count = 2,
    uint32_t sample_rate = 44100,
    size_t num_frames = 100,
    uint32_t channel_mask = 0) {
  SymphoniaAudioBuffer buf;
  buf.sample_format = sample_format;
  buf.channel_count = channel_count;
  buf.sample_rate = sample_rate;
  buf.num_frames = num_frames;
  buf.channel_mask = channel_mask;
  const size_t bytes_per_sample =
      sample_format == SymphoniaSampleFormat::F32 ? 4 : 2;
  buf.data = CreateZeroedRustVec(num_frames * channel_count * bytes_per_sample);
  return buf;
}

}  // namespace

TEST(SymphoniaAudioDecoderTest, ToSymphoniaPacketNullTimestamp) {
  auto buffer = base::MakeRefCounted<DecoderBuffer>(10);
  buffer->set_timestamp(base::Microseconds(100));
  SymphoniaPacket packet =
      SymphoniaAudioDecoder::ToSymphoniaPacketForTesting(*buffer, std::nullopt);
  EXPECT_EQ(packet.timestamp_us, 0u);
}

TEST(SymphoniaAudioDecoderTest, DecodeTruncatedBufferDoesNotFail) {
  base::test::TaskEnvironment task_environment;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {kSymphoniaAudioDecoding, kSymphoniaMp3Decoding}, {});

  NullMediaLog media_log;
  auto decoder = std::make_unique<SymphoniaAudioDecoder>(
      task_environment.GetMainThreadTaskRunner(), &media_log);

  AudioDecoderConfig config(AudioCodec::kMP3, kSampleFormatF32,
                            ChannelLayoutConfig::Stereo(), 48000,
                            EmptyExtraData(), EncryptionScheme::kUnencrypted);

  base::test::TestFuture<DecoderStatus> init_future;
  base::test::TestFuture<scoped_refptr<AudioBuffer>> output_future;
  decoder->Initialize(config, nullptr, init_future.GetCallback(),
                      output_future.GetRepeatingCallback(), base::DoNothing());
  EXPECT_TRUE(init_future.Get().is_ok());

  // Send a truncated MP3 buffer with a valid syncword but incomplete frame.
  const uint8_t truncated_mp3_data[] = {0xFF, 0xFB, 0x90, 0x00, 0x01, 0x02};
  auto buffer = DecoderBuffer::CopyFrom(truncated_mp3_data);
  buffer->set_timestamp(base::Microseconds(0));

  base::test::TestFuture<DecoderStatus> decode_future;
  decoder->Decode(std::move(buffer), decode_future.GetCallback());
  EXPECT_TRUE(decode_future.Get().is_ok());
  EXPECT_FALSE(output_future.IsReady());

  // An EOS buffer sent after a dropped truncated buffer should also succeed.
  base::test::TestFuture<DecoderStatus> eos_future;
  decoder->Decode(DecoderBuffer::CreateEOSBuffer(), eos_future.GetCallback());
  EXPECT_TRUE(eos_future.Get().is_ok());
}

TEST(SymphoniaAudioDecoderTest, BackToBackDecodeErrorsFail) {
  base::test::TaskEnvironment task_environment;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {kSymphoniaAudioDecoding, kSymphoniaMp3Decoding}, {});

  NullMediaLog media_log;
  auto decoder = std::make_unique<SymphoniaAudioDecoder>(
      task_environment.GetMainThreadTaskRunner(), &media_log);

  AudioDecoderConfig config(AudioCodec::kMP3, kSampleFormatF32,
                            ChannelLayoutConfig::Stereo(), 48000,
                            EmptyExtraData(), EncryptionScheme::kUnencrypted);

  base::test::TestFuture<DecoderStatus> init_future;
  decoder->Initialize(config, nullptr, init_future.GetCallback(),
                      base::DoNothing(), base::DoNothing());
  EXPECT_TRUE(init_future.Get().is_ok());

  const uint8_t truncated_mp3_data[] = {0xFF, 0xFB, 0x90, 0x00, 0x01, 0x02};

  // First truncated buffer should be dropped gracefully (Ok).
  {
    auto buffer = DecoderBuffer::CopyFrom(truncated_mp3_data);
    buffer->set_timestamp(base::Microseconds(0));
    base::test::TestFuture<DecoderStatus> decode_future;
    decoder->Decode(std::move(buffer), decode_future.GetCallback());
    EXPECT_TRUE(decode_future.Get().is_ok());
  }

  // Second consecutive truncated buffer (back-to-back) must be treated as
  // fatal.
  {
    auto buffer = DecoderBuffer::CopyFrom(truncated_mp3_data);
    buffer->set_timestamp(base::Microseconds(1000));
    base::test::TestFuture<DecoderStatus> decode_future;
    decoder->Decode(std::move(buffer), decode_future.GetCallback());
    EXPECT_FALSE(decode_future.Get().is_ok());
  }
}

TEST(SymphoniaAudioDecoderTest, ToMediaAudioBufferValidation) {
  const ChannelLayoutConfig layout_config = ChannelLayoutConfig::Stereo();

  // Valid buffer succeeds.
  {
    auto buf = CreateValidSymphoniaBuffer();
    auto audio_buf_or = SymphoniaAudioDecoder::ToMediaAudioBufferForTesting(
        std::move(buf), layout_config, base::Microseconds(0));
    ASSERT_TRUE(audio_buf_or.has_value());
    auto audio_buf = std::move(audio_buf_or).value();
    ASSERT_TRUE(audio_buf);
    EXPECT_EQ(audio_buf->sample_rate(), 44100);
    EXPECT_EQ(audio_buf->channel_count(), 2);
    EXPECT_EQ(audio_buf->frame_count(), 100);
  }

  // Unknown sample format fails validation.
  {
    auto buf = CreateValidSymphoniaBuffer();
    buf.sample_format = SymphoniaSampleFormat::Unknown;
    EXPECT_FALSE(SymphoniaAudioDecoder::ToMediaAudioBufferForTesting(
                     std::move(buf), layout_config, base::Microseconds(0))
                     .has_value());
  }

  // Invalid channel count fails validation.
  {
    auto buf = CreateValidSymphoniaBuffer();
    buf.channel_count = 0;
    EXPECT_FALSE(SymphoniaAudioDecoder::ToMediaAudioBufferForTesting(
                     std::move(buf), layout_config, base::Microseconds(0))
                     .has_value());

    buf = CreateValidSymphoniaBuffer();
    buf.channel_count = limits::kMaxChannels + 1;
    EXPECT_FALSE(SymphoniaAudioDecoder::ToMediaAudioBufferForTesting(
                     std::move(buf), layout_config, base::Microseconds(0))
                     .has_value());
  }

  // Invalid sample rate fails validation.
  {
    auto buf = CreateValidSymphoniaBuffer();
    buf.sample_rate = 0;
    EXPECT_FALSE(SymphoniaAudioDecoder::ToMediaAudioBufferForTesting(
                     std::move(buf), layout_config, base::Microseconds(0))
                     .has_value());

    buf = CreateValidSymphoniaBuffer();
    buf.sample_rate = limits::kMinSampleRate - 1;
    EXPECT_FALSE(SymphoniaAudioDecoder::ToMediaAudioBufferForTesting(
                     std::move(buf), layout_config, base::Microseconds(0))
                     .has_value());

    buf = CreateValidSymphoniaBuffer();
    buf.sample_rate = limits::kMaxSampleRate + 1;
    EXPECT_FALSE(SymphoniaAudioDecoder::ToMediaAudioBufferForTesting(
                     std::move(buf), layout_config, base::Microseconds(0))
                     .has_value());
  }

  // Invalid frame count fails validation.
  {
    auto buf = CreateValidSymphoniaBuffer();
    buf.num_frames = 0;
    EXPECT_FALSE(SymphoniaAudioDecoder::ToMediaAudioBufferForTesting(
                     std::move(buf), layout_config, base::Microseconds(0))
                     .has_value());

    buf = CreateValidSymphoniaBuffer();
    buf.num_frames = static_cast<size_t>(limits::kMaxSamplesPerPacket) + 1;
    buf.data = CreateZeroedRustVec(100);
    EXPECT_FALSE(SymphoniaAudioDecoder::ToMediaAudioBufferForTesting(
                     std::move(buf), layout_config, base::Microseconds(0))
                     .has_value());
  }

  // Insufficient data size fails validation.
  {
    auto buf = CreateValidSymphoniaBuffer();
    buf.data.clear();
    EXPECT_FALSE(SymphoniaAudioDecoder::ToMediaAudioBufferForTesting(
                     std::move(buf), layout_config, base::Microseconds(0))
                     .has_value());
  }

  // Channel count change with channel_mask == 0 safely resolves layout.
  {
    auto buf = CreateValidSymphoniaBuffer(SymphoniaSampleFormat::F32,
                                          /*channel_count=*/1,
                                          /*sample_rate=*/44100,
                                          /*num_frames=*/100,
                                          /*channel_mask=*/0);
    auto audio_buf_or = SymphoniaAudioDecoder::ToMediaAudioBufferForTesting(
        std::move(buf), layout_config, base::Microseconds(0));
    ASSERT_TRUE(audio_buf_or.has_value());
    auto audio_buf = std::move(audio_buf_or).value();
    ASSERT_TRUE(audio_buf);
    EXPECT_EQ(audio_buf->channel_layout(), CHANNEL_LAYOUT_MONO);
    EXPECT_NE(audio_buf->channel_layout(), CHANNEL_LAYOUT_NONE);
    EXPECT_NE(audio_buf->channel_layout(), CHANNEL_LAYOUT_UNSUPPORTED);
  }
}

}  // namespace media
