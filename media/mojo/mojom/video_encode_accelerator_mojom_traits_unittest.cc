// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/mojom/video_encode_accelerator_mojom_traits.h"

#include "media/mojo/mojom/video_encode_accelerator.mojom.h"
#include "media/mojo/mojom/video_encoder_info_mojom_traits.h"
#include "media/video/video_encode_accelerator.h"
#include "media/video/video_encoder_info.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
TEST(VideoEncoderInfoStructTraitTest, RoundTrip) {
  ::media::VideoEncoderInfo input;
  input.implementation_name = "FakeVideoEncodeAccelerator";
  // Scaling settings.
  input.scaling_settings = ::media::ScalingSettings(12, 123);
  // FPS allocation.
  for (size_t i = 0; i < ::media::VideoEncoderInfo::kMaxSpatialLayers; ++i)
    input.fps_allocation[i] = {5, 5, 10};
  // Resolution bitrate limits.
  input.resolution_bitrate_limits.push_back(::media::ResolutionBitrateLimit(
      gfx::Size(123, 456), 123456, 123456, 789012));
  input.resolution_bitrate_limits.push_back(::media::ResolutionBitrateLimit(
      gfx::Size(789, 1234), 1234567, 1234567, 7890123));
  // Other bool values.
  input.supports_native_handle = true;
  input.has_trusted_rate_controller = true;
  input.is_hardware_accelerated = true;
  input.supports_simulcast = true;

  ::media::VideoEncoderInfo output = input;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::VideoEncoderInfo>(
      input, output));
  EXPECT_EQ(input, output);
}

TEST(SpatialLayerStructTraitTest, RoundTrip) {
  ::media::VideoEncodeAccelerator::Config::SpatialLayer input_spatial_layer;
  input_spatial_layer.width = 320;
  input_spatial_layer.width = 180;
  input_spatial_layer.bitrate_bps = 12345678u;
  input_spatial_layer.framerate = 24u;
  input_spatial_layer.max_qp = 30u;
  input_spatial_layer.num_of_temporal_layers = 3u;
  ::media::VideoEncodeAccelerator::Config::SpatialLayer output_spatial_layer;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::SpatialLayer>(
      input_spatial_layer, output_spatial_layer));
  EXPECT_EQ(input_spatial_layer, output_spatial_layer);
}

TEST(VideoEncodeAcceleratorConfigStructTraitTest, RoundTrip) {
  std::vector<::media::VideoEncodeAccelerator::Config::SpatialLayer>
      input_spatial_layers(3);
  gfx::Size kBaseSize(320, 180);
  uint32_t kBaseBitrateBps = 123456u;
  uint32_t kBaseFramerate = 24u;
  for (size_t i = 0; i < input_spatial_layers.size(); ++i) {
    input_spatial_layers[i].width =
        static_cast<int32_t>(kBaseSize.width() * (i + 1));
    input_spatial_layers[i].height =
        static_cast<int32_t>(kBaseSize.height() * (i + 1));
    input_spatial_layers[i].bitrate_bps = kBaseBitrateBps * (i + 1) / 2;
    input_spatial_layers[i].framerate = kBaseFramerate * 2 / (i + 1);
    input_spatial_layers[i].max_qp = 30 * (i + 1) / 2;
    input_spatial_layers[i].num_of_temporal_layers = 3 - i;
  }
  ::media::VideoEncodeAccelerator::Config input_config(
      ::media::PIXEL_FORMAT_NV12, kBaseSize, ::media::VP9PROFILE_PROFILE0,
      kBaseBitrateBps, kBaseFramerate, base::nullopt, base::nullopt, false,
      ::media::VideoEncodeAccelerator::Config::StorageType::kGpuMemoryBuffer,
      ::media::VideoEncodeAccelerator::Config::ContentType::kCamera,
      input_spatial_layers);
  DVLOG(4) << input_config.AsHumanReadableString();

  ::media::VideoEncodeAccelerator::Config output_config{};
  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::VideoEncodeAcceleratorConfig>(
          input_config, output_config));
  DVLOG(4) << output_config.AsHumanReadableString();
  EXPECT_EQ(input_config, output_config);
}

TEST(BitstreamBufferMetadataTraitTest, RoundTrip) {
  ::media::BitstreamBufferMetadata input_metadata;
  input_metadata.payload_size_bytes = 1234;
  input_metadata.key_frame = true;
  input_metadata.timestamp = base::TimeDelta::FromMilliseconds(123456);
  ::media::BitstreamBufferMetadata output_metadata;
  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::BitstreamBufferMetadata>(
          input_metadata, output_metadata));
  EXPECT_EQ(input_metadata, output_metadata);

  Vp8Metadata vp8;
  vp8.non_reference = true;
  vp8.temporal_idx = 1;
  vp8.layer_sync = false;
  input_metadata.vp8 = vp8;
  output_metadata = ::media::BitstreamBufferMetadata();
  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::BitstreamBufferMetadata>(
          input_metadata, output_metadata));
  EXPECT_EQ(input_metadata, output_metadata);
  input_metadata.vp8.reset();

  Vp9Metadata vp9;
  vp9.has_reference = true;
  vp9.temporal_up_switch = true;
  vp9.temporal_idx = 2;
  vp9.p_diffs = {0, 1};
  input_metadata.vp9 = vp9;
  output_metadata = ::media::BitstreamBufferMetadata();
  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::BitstreamBufferMetadata>(
          input_metadata, output_metadata));
  EXPECT_EQ(input_metadata, output_metadata);
}
}  // namespace media
