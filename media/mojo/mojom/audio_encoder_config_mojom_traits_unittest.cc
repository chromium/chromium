// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/mojom/audio_encoder_config_mojom_traits.h"

#include <utility>

#include "media/base/audio_encoder.h"
#include "media/base/limits.h"
#include "media/mojo/mojom/audio_encoder.mojom.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace {

constexpr int kMinChannels = 1;

AudioEncoderConfig CreateValidConfig() {
  AudioEncoderConfig config;
  config.codec = AudioCodec::kAAC;
  config.channels = 2;
  config.sample_rate = 48000;
  config.bitrate = 128000;
  return config;
}

}  // namespace

TEST(AudioEncoderConfigStructTraitsTest, Normal) {
  AudioEncoderConfig input = CreateValidConfig();
  AudioEncoderConfig output;
  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::AudioEncoderConfig>(
      input, output));
  EXPECT_EQ(input.codec, output.codec);
  EXPECT_EQ(input.channels, output.channels);
  EXPECT_EQ(input.sample_rate, output.sample_rate);
  EXPECT_EQ(input.bitrate, output.bitrate);
}

TEST(AudioEncoderConfigStructTraitsTest, BoundaryChannels) {
  AudioEncoderConfig input = CreateValidConfig();
  AudioEncoderConfig output;

  // Min channels should succeed.
  input.channels = kMinChannels;
  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::AudioEncoderConfig>(
      input, output));
  EXPECT_EQ(input.channels, output.channels);

  // Max channels should succeed.
  input.channels = media::limits::kMaxChannels;
  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::AudioEncoderConfig>(
      input, output));
  EXPECT_EQ(input.channels, output.channels);
}

TEST(AudioEncoderConfigStructTraitsTest, InvalidChannels) {
  AudioEncoderConfig input = CreateValidConfig();
  AudioEncoderConfig output;

  // Min channels - 1 should fail.
  input.channels = kMinChannels - 1;
  EXPECT_FALSE(mojo::test::SerializeAndDeserialize<mojom::AudioEncoderConfig>(
      input, output));

  // Max channels + 1 should fail.
  input.channels = media::limits::kMaxChannels + 1;
  EXPECT_FALSE(mojo::test::SerializeAndDeserialize<mojom::AudioEncoderConfig>(
      input, output));
}

TEST(AudioEncoderConfigStructTraitsTest, InvalidSampleRate) {
  AudioEncoderConfig input = CreateValidConfig();

  // Too low should fail.
  input.sample_rate = media::limits::kMinSampleRate - 1;
  AudioEncoderConfig output;
  EXPECT_FALSE(mojo::test::SerializeAndDeserialize<mojom::AudioEncoderConfig>(
      input, output));

  // Too high should fail.
  input.sample_rate = media::limits::kMaxSampleRate + 1;
  EXPECT_FALSE(mojo::test::SerializeAndDeserialize<mojom::AudioEncoderConfig>(
      input, output));
}

TEST(AudioEncoderConfigStructTraitsTest, BoundarySampleRate) {
  AudioEncoderConfig input = CreateValidConfig();

  // Min sample rate should succeed.
  input.sample_rate = media::limits::kMinSampleRate;
  AudioEncoderConfig output;
  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::AudioEncoderConfig>(
      input, output));
  EXPECT_EQ(input.sample_rate, output.sample_rate);

  // Max sample rate should succeed.
  input.sample_rate = media::limits::kMaxSampleRate;
  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::AudioEncoderConfig>(
      input, output));
  EXPECT_EQ(input.sample_rate, output.sample_rate);
}

TEST(AudioEncoderConfigStructTraitsTest, Bitrate) {
  AudioEncoderConfig input = CreateValidConfig();

  input.bitrate = std::nullopt;
  AudioEncoderConfig output;
  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::AudioEncoderConfig>(
      input, output));
  EXPECT_FALSE(output.bitrate.has_value());

  input.bitrate = 0;
  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::AudioEncoderConfig>(
      input, output));
  // It should deserialize to nullopt because 0 is "no preference".
  EXPECT_FALSE(output.bitrate.has_value());

  input.bitrate = 128000;
  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::AudioEncoderConfig>(
      input, output));
  EXPECT_TRUE(output.bitrate.has_value());
  EXPECT_EQ(*input.bitrate, *output.bitrate);
}

}  // namespace media
