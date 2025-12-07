// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/mojom/supported_video_decoder_config_mojom_traits.h"

#include "media/base/supported_video_decoder_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace {

SupportedVideoDecoderConfig ConstructTestConfig() {
  SupportedVideoDecoderConfig input;
  input.profile_min = H264PROFILE_MIN;
  input.profile_max = H264PROFILE_MAX;
  input.coded_size_min = gfx::Size(10, 20);
  input.coded_size_max = gfx::Size(10000, 20000);
  input.allow_encrypted = true;
  input.require_encrypted = false;
  return input;
}

}  // namespace

TEST(SupportedVideoDecoderConfigTraitsTest, Normal) {
  auto input = ConstructTestConfig();

  auto data = media::mojom::SupportedVideoDecoderConfig::Serialize(&input);
  SupportedVideoDecoderConfig output;
  EXPECT_TRUE(media::mojom::SupportedVideoDecoderConfig::Deserialize(
      std::move(data), &output));
  EXPECT_EQ(input, output);
}

TEST(SupportedVideoDecoderConfigTraitsTest, MinMaxEqual) {
  auto input = ConstructTestConfig();
  input.coded_size_min = input.coded_size_max = gfx::Size(96, 96);

  auto data = media::mojom::SupportedVideoDecoderConfig::Serialize(&input);
  SupportedVideoDecoderConfig output;
  EXPECT_TRUE(media::mojom::SupportedVideoDecoderConfig::Deserialize(
      std::move(data), &output));
  EXPECT_EQ(input, output);
}

TEST(SupportedVideoDecoderConfigTraitsTest, InvalidMin) {
  auto input = ConstructTestConfig();
  input.coded_size_min = gfx::Size();

  auto data = media::mojom::SupportedVideoDecoderConfig::Serialize(&input);
  SupportedVideoDecoderConfig output;
  EXPECT_FALSE(media::mojom::SupportedVideoDecoderConfig::Deserialize(
      std::move(data), &output));
}

TEST(SupportedVideoDecoderConfigTraitsTest, InvalidMax) {
  auto input = ConstructTestConfig();
  input.coded_size_max = gfx::Size();

  auto data = media::mojom::SupportedVideoDecoderConfig::Serialize(&input);
  SupportedVideoDecoderConfig output;
  EXPECT_FALSE(media::mojom::SupportedVideoDecoderConfig::Deserialize(
      std::move(data), &output));
}

TEST(SupportedVideoDecoderConfigTraitsTest, InvertedMinMaxProfile) {
  auto input = ConstructTestConfig();
  std::swap(input.profile_min, input.profile_max);

  auto data = media::mojom::SupportedVideoDecoderConfig::Serialize(&input);
  SupportedVideoDecoderConfig output;
  EXPECT_FALSE(media::mojom::SupportedVideoDecoderConfig::Deserialize(
      std::move(data), &output));
}

TEST(SupportedVideoDecoderConfigTraitsTest, InconsistentEncryptedSupport) {
  auto input = ConstructTestConfig();
  input.allow_encrypted = false;
  input.require_encrypted = true;

  auto data = media::mojom::SupportedVideoDecoderConfig::Serialize(&input);
  SupportedVideoDecoderConfig output;
  EXPECT_FALSE(media::mojom::SupportedVideoDecoderConfig::Deserialize(
      std::move(data), &output));
}

}  // namespace media
