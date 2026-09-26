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
#include "media/base/audio_bus.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/channel_layout.h"
#include "media/base/decoder_buffer.h"
#include "media/base/decoder_status.h"
#include "media/base/limits.h"
#include "media/base/media_switches.h"
#include "media/base/media_util.h"
#include "media/base/sample_format.h"
#include "media/base/test_data_util.h"
#include "media/ffmpeg/ffmpeg_common.h"
#include "media/ffmpeg/scoped_av_packet.h"
#include "media/filters/audio_file_reader.h"
#include "media/filters/in_memory_url_protocol.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

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

TEST(SymphoniaAudioDecoderTest, PlanarF32ZeroCopyAudioBusAlignment) {
  base::test::TaskEnvironment task_environment;
  base::test::ScopedFeatureList features(kSymphoniaPcmDecoding);

  NullMediaLog media_log;
  auto decoder = std::make_unique<SymphoniaAudioDecoder>(
      task_environment.GetMainThreadTaskRunner(), &media_log);

  AudioDecoderConfig config(AudioCodec::kPCM, kSampleFormatF32,
                            ChannelLayoutConfig::Stereo(), 48000,
                            EmptyExtraData(), EncryptionScheme::kUnencrypted);

  base::test::TestFuture<scoped_refptr<AudioBuffer>> output_future;
  base::test::TestFuture<DecoderStatus> init_future;
  decoder->Initialize(config, nullptr, init_future.GetCallback(),
                      output_future.GetRepeatingCallback(), base::DoNothing());
  EXPECT_TRUE(init_future.Get().is_ok());

  // 3 stereo frames of interleaved F32 PCM (3 frames * 4 bytes = 12 bytes per
  // plane, not 32-byte aligned).
  constexpr std::array<float, 6> kInputSamples = {0.1f,  -0.1f, 0.2f,
                                                  -0.2f, 0.3f,  -0.3f};
  auto buf = DecoderBuffer::CopyFrom(
      base::as_byte_span(base::allow_nonunique_obj, kInputSamples));
  buf->set_timestamp(base::TimeDelta());

  base::test::TestFuture<DecoderStatus> decode_future;
  decoder->Decode(std::move(buf), decode_future.GetCallback());
  EXPECT_TRUE(decode_future.Get().is_ok());

  scoped_refptr<AudioBuffer> decoded = output_future.Take();
  ASSERT_TRUE(decoded);
  EXPECT_EQ(decoded->sample_format(), kSampleFormatPlanarF32);
  EXPECT_EQ(decoded->frame_count(), 3);
  EXPECT_EQ(decoded->channel_count(), 2);

  // Verify that every channel plane pointer is 32-byte aligned despite the odd
  // frame count.
  for (int ch = 0; ch < decoded->channel_count(); ++ch) {
    EXPECT_TRUE(AudioBus::IsAligned(decoded->channel_data()[ch]));
  }

  // Verify that WrapOrCopyToAudioBus wraps zero-copy instead of falling back
  // to copying.
  std::unique_ptr<AudioBus> bus = AudioBuffer::WrapOrCopyToAudioBus(decoded);
  ASSERT_TRUE(bus);
  EXPECT_EQ(bus->channel(0).data(),
            reinterpret_cast<const float*>(decoded->channel_data()[0].get()));
  EXPECT_EQ(bus->channel(1).data(),
            reinterpret_cast<const float*>(decoded->channel_data()[1].get()));

  EXPECT_THAT(bus->channel(0), testing::ElementsAre(0.1f, 0.2f, 0.3f));
  EXPECT_THAT(bus->channel(1), testing::ElementsAre(-0.1f, -0.2f, -0.3f));
}

TEST(SymphoniaAudioDecoderTest, DecodeMp3WithBlockTypeMismatchSucceeds) {
  base::test::TaskEnvironment task_environment;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {kSymphoniaAudioDecoding, kSymphoniaMp3Decoding}, {});

  scoped_refptr<DecoderBuffer> data = ReadTestDataFile("repro-minimal.mp3");
  InMemoryUrlProtocol protocol(*data, false);
  AudioFileReader reader(&protocol);
  ASSERT_TRUE(reader.OpenDemuxerForTesting());

  NullMediaLog media_log;
  auto decoder = std::make_unique<SymphoniaAudioDecoder>(
      task_environment.GetMainThreadTaskRunner(), &media_log);

  AudioDecoderConfig config(AudioCodec::kMP3, kSampleFormatF32,
                            ChannelLayoutConfig::Stereo(), 44100,
                            EmptyExtraData(), EncryptionScheme::kUnencrypted);

  base::test::TestFuture<DecoderStatus> init_future;
  size_t output_buffer_count = 0;
  decoder->Initialize(
      config, nullptr, init_future.GetCallback(),
      base::BindRepeating(
          [](size_t* count, scoped_refptr<AudioBuffer> buffer) { (*count)++; },
          &output_buffer_count),
      base::DoNothing());
  EXPECT_TRUE(init_future.Get().is_ok());

  auto packet = ScopedAVPacket::Allocate();
  while (reader.ReadPacketForTesting(packet.get())) {
    auto buffer = DecoderBuffer::CopyFrom(AVPacketData(*packet));
    buffer->set_timestamp(ConvertStreamTimestamp(
        reader.GetAVStreamForTesting()->time_base, packet->pts));
    buffer->set_duration(ConvertStreamTimestamp(
        reader.GetAVStreamForTesting()->time_base, packet->duration));
    if (packet->flags & AV_PKT_FLAG_KEY) {
      buffer->set_is_key_frame(true);
    }
    av_packet_unref(packet.get());

    base::test::TestFuture<DecoderStatus> decode_future;
    decoder->Decode(std::move(buffer), decode_future.GetCallback());
    EXPECT_TRUE(decode_future.Get().is_ok());
  }

  base::test::TestFuture<DecoderStatus> eos_future;
  decoder->Decode(DecoderBuffer::CreateEOSBuffer(), eos_future.GetCallback());
  EXPECT_TRUE(eos_future.Get().is_ok());
  EXPECT_GT(output_buffer_count, 0u);
}

TEST(SymphoniaAudioDecoderTest, MidstreamSampleRateAndChannelChange) {
  base::test::TaskEnvironment task_environment;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {kSymphoniaAudioDecoding, kSymphoniaMp3Decoding}, {});

  NullMediaLog media_log;
  auto decoder = std::make_unique<SymphoniaAudioDecoder>(
      task_environment.GetMainThreadTaskRunner(), &media_log);

  AudioDecoderConfig config(AudioCodec::kMP3, kSampleFormatF32,
                            ChannelLayoutConfig::Mono(), 48000,
                            EmptyExtraData(), EncryptionScheme::kUnencrypted);

  base::test::TestFuture<DecoderStatus> init_future;
  base::test::TestFuture<scoped_refptr<AudioBuffer>> output_future;
  decoder->Initialize(config, nullptr, init_future.GetCallback(),
                      output_future.GetRepeatingCallback(), base::DoNothing());
  EXPECT_TRUE(init_future.Get().is_ok());

  // Packet 1: MP2 96kbps 48kHz Mono (288 bytes).
  std::vector<uint8_t> mp2_mono_48k(288, 0);
  mp2_mono_48k[0] = 0xFF;
  mp2_mono_48k[1] = 0xFD;
  mp2_mono_48k[2] = 0x64;
  mp2_mono_48k[3] = 0xD0;
  auto buf1 = DecoderBuffer::CopyFrom(mp2_mono_48k);
  buf1->set_timestamp(base::Microseconds(0));

  base::test::TestFuture<DecoderStatus> decode_future1;
  decoder->Decode(std::move(buf1), decode_future1.GetCallback());
  EXPECT_TRUE(decode_future1.Get().is_ok());
  scoped_refptr<AudioBuffer> out1 = output_future.Take();
  ASSERT_TRUE(out1);
  EXPECT_EQ(out1->sample_rate(), 48000);
  EXPECT_EQ(out1->channel_count(), 1);
  EXPECT_EQ(out1->channel_layout(), CHANNEL_LAYOUT_MONO);

  // Packet 2: Midstream change to MP2 96kbps 44.1kHz Stereo (313 bytes).
  std::vector<uint8_t> mp2_stereo_44k1(313, 0);
  mp2_stereo_44k1[0] = 0xFF;
  mp2_stereo_44k1[1] = 0xFD;
  mp2_stereo_44k1[2] = 0x60;
  mp2_stereo_44k1[3] = 0x10;
  auto buf2 = DecoderBuffer::CopyFrom(mp2_stereo_44k1);
  buf2->set_timestamp(base::Microseconds(24000));

  base::test::TestFuture<DecoderStatus> decode_future2;
  decoder->Decode(std::move(buf2), decode_future2.GetCallback());
  EXPECT_TRUE(decode_future2.Get().is_ok());
  scoped_refptr<AudioBuffer> out2 = output_future.Take();
  ASSERT_TRUE(out2);
  EXPECT_EQ(out2->sample_rate(), 44100);
  EXPECT_EQ(out2->channel_count(), 2);
  EXPECT_EQ(out2->channel_layout(), CHANNEL_LAYOUT_STEREO);
}

TEST(SymphoniaAudioDecoderTest, RejectsInvalidDecodedParameters) {
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

  // Corrupted frame with invalid header.
  const uint8_t corrupted_data[] = {0xFF, 0x00, 0x00, 0x00, 0x00, 0x00};
  auto buf = DecoderBuffer::CopyFrom(corrupted_data);
  buf->set_timestamp(base::Microseconds(0));

  base::test::TestFuture<DecoderStatus> decode_future;
  decoder->Decode(std::move(buf), decode_future.GetCallback());
  // First corrupted buffer is dropped gracefully.
  EXPECT_TRUE(decode_future.Get().is_ok());
  EXPECT_FALSE(output_future.IsReady());
}

TEST(SymphoniaAudioDecoderTest, IsValidDecodedParametersValidation) {
  constexpr size_t kChannels = 2;
  constexpr uint32_t kSampleRate = 48000;
  constexpr size_t kNumFrames = 1024;
  constexpr SampleFormat kSampleFormat = SampleFormat::kSampleFormatF32;

  // Valid parameters.
  EXPECT_TRUE(SymphoniaAudioDecoder::IsValidDecodedParametersForTesting(
      kChannels, kSampleRate, kNumFrames, kSampleFormat));

  // Overflowing sample rates (> INT_MAX).
  EXPECT_FALSE(SymphoniaAudioDecoder::IsValidDecodedParametersForTesting(
      kChannels, 4294934084u, kNumFrames, kSampleFormat));
  EXPECT_FALSE(SymphoniaAudioDecoder::IsValidDecodedParametersForTesting(
      kChannels, UINT32_MAX, kNumFrames, kSampleFormat));

  // Sample rates outside [kMinSampleRate, kMaxSampleRate].
  EXPECT_FALSE(SymphoniaAudioDecoder::IsValidDecodedParametersForTesting(
      kChannels, 0, kNumFrames, kSampleFormat));
  EXPECT_FALSE(SymphoniaAudioDecoder::IsValidDecodedParametersForTesting(
      kChannels, static_cast<uint32_t>(limits::kMinSampleRate - 1), kNumFrames,
      kSampleFormat));
  EXPECT_FALSE(SymphoniaAudioDecoder::IsValidDecodedParametersForTesting(
      kChannels, static_cast<uint32_t>(limits::kMaxSampleRate + 1), kNumFrames,
      kSampleFormat));

  // Invalid channel counts.
  EXPECT_FALSE(SymphoniaAudioDecoder::IsValidDecodedParametersForTesting(
      0, kSampleRate, kNumFrames, kSampleFormat));
  EXPECT_FALSE(SymphoniaAudioDecoder::IsValidDecodedParametersForTesting(
      static_cast<size_t>(limits::kMaxChannels + 1), kSampleRate, kNumFrames,
      kSampleFormat));
  EXPECT_FALSE(SymphoniaAudioDecoder::IsValidDecodedParametersForTesting(
      SIZE_MAX, kSampleRate, kNumFrames, kSampleFormat));

  // Invalid num_frames.
  EXPECT_FALSE(SymphoniaAudioDecoder::IsValidDecodedParametersForTesting(
      kChannels, kSampleRate, 0, kSampleFormat));
  EXPECT_FALSE(SymphoniaAudioDecoder::IsValidDecodedParametersForTesting(
      kChannels, kSampleRate,
      static_cast<size_t>(limits::kMaxSamplesPerPacket + 1), kSampleFormat));
  EXPECT_FALSE(SymphoniaAudioDecoder::IsValidDecodedParametersForTesting(
      kChannels, kSampleRate, SIZE_MAX, kSampleFormat));

  // Unknown sample format.
  EXPECT_FALSE(SymphoniaAudioDecoder::IsValidDecodedParametersForTesting(
      kChannels, kSampleRate, kNumFrames, kUnknownSampleFormat));
}

}  // namespace media
