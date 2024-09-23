// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/mojom/video_encode_accelerator_mojom_traits.h"

#include "media/base/video_bitrate_allocation.h"
#include "media/mojo/mojom/video_encode_accelerator.mojom.h"
#include "media/mojo/mojom/video_encoder_info_mojom_traits.h"
#include "media/video/video_encode_accelerator.h"
#include "media/video/video_encoder_info.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

TEST(SVCScalabilityModeTest, RoundTrip) {
  auto hw_supported_svc_modes =
      ::media::GetSupportedScalabilityModesByHWEncoderForTesting();
  for (::media::SVCScalabilityMode input_svc_mode : hw_supported_svc_modes) {
    SVCScalabilityMode output_svc_mode;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::SVCScalabilityMode>(
        input_svc_mode, output_svc_mode));
    EXPECT_EQ(input_svc_mode, output_svc_mode);
  }
}

TEST(VideoEncodeAcceleratorSupportedProfile, RoundTrip) {
  ::media::VideoEncodeAccelerator::SupportedProfile input;
  input.profile = VP9PROFILE_PROFILE0;
  input.min_resolution = gfx::Size(64, 64);
  input.max_resolution = gfx::Size(4096, 4096);
  input.max_framerate_numerator = 30;
  input.max_framerate_denominator = 1;
  input.rate_control_modes = VideoEncodeAccelerator::kConstantMode |
                             VideoEncodeAccelerator::kVariableMode;
  input.scalability_modes.push_back(::media::SVCScalabilityMode::kL1T3);
  input.scalability_modes.push_back(::media::SVCScalabilityMode::kL3T3Key);
  input.scalability_modes.push_back(::media::SVCScalabilityMode::kS2T3);
  input.scalability_modes.push_back(::media::SVCScalabilityMode::kS3T1);

  ::media::VideoEncodeAccelerator::SupportedProfile output;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<
              mojom::VideoEncodeAcceleratorSupportedProfile>(input, output));
  EXPECT_EQ(input, output);
}

TEST(VideoEncoderInfoStructTraitTest, RoundTrip) {
  ::media::VideoEncoderInfo input;
  input.implementation_name = "FakeVideoEncodeAccelerator";
  // Set `frame_delay` but leave `input_capacity` empty.
  input.frame_delay = 3;
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

TEST(VideoBitrateAllocationStructTraitTest, ConstantBitrate_RoundTrip) {
  ::media::VideoBitrateAllocation input_allocation;
  ASSERT_TRUE(input_allocation.SetBitrate(0, 0, 1000u));
  ASSERT_TRUE(input_allocation.SetBitrate(1, 0, 3500u));
  ASSERT_TRUE(input_allocation.SetBitrate(0, 1, 2000u));
  ASSERT_TRUE(input_allocation.SetBitrate(1, 1, 5000u));
  ::media::VideoBitrateAllocation output_allocation;

  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::VideoBitrateAllocation>(
          input_allocation, output_allocation));

  EXPECT_EQ(input_allocation, output_allocation);
}

TEST(VideoBitrateAllocationStructTraitTest,
     VariableBitrate_PeakGreaterThanSum_RoundTrip) {
  ::media::VideoBitrateAllocation input_allocation(Bitrate::Mode::kVariable);
  input_allocation.SetBitrate(0, 0, 5000u);
  input_allocation.SetPeakBps(654321u);
  ::media::VideoBitrateAllocation output_allocation;

  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::VideoBitrateAllocation>(
          input_allocation, output_allocation));

  EXPECT_EQ(input_allocation, output_allocation);
}

TEST(VideoBitrateAllocationStructTraitTest,
     VariableBitrate_PeakEqualsSum_RoundTrip) {
  ::media::VideoBitrateAllocation input_allocation(Bitrate::Mode::kVariable);
  input_allocation.SetBitrate(0, 0, 5000u);
  input_allocation.SetPeakBps(5000u);
  ::media::VideoBitrateAllocation output_allocation;

  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::VideoBitrateAllocation>(
          input_allocation, output_allocation));

  EXPECT_EQ(input_allocation, output_allocation);
}

TEST(VideoBitrateAllocationStructTraitTest,
     VariableBitrate_InvalidTooLowPeak_Fails) {
  mojom::VideoBitrateAllocationPtr mojom_allocation =
      mojom::VideoBitrateAllocation::New();
  std::vector<uint32_t> bitrates = {1000u, 2500u, 3000u};
  mojom_allocation->bitrates = bitrates;
  mojom::VariableBitratePeakPtr mojom_peak = mojom::VariableBitratePeak::New();
  mojom_peak->bps = 6499u;  // invalid: must be >=6500u
  mojom_allocation->variable_bitrate_peak = std::move(mojom_peak);
  VideoBitrateAllocation output;

  EXPECT_FALSE(
      mojo::test::SerializeAndDeserialize<mojom::VideoBitrateAllocation>(
          mojom_allocation, output));
}

TEST(VideoBitrateAllocationStructTraitTest,
     VariableBitrate_InvalidZeroPeak_Fails) {
  mojom::VideoBitrateAllocationPtr mojom_allocation =
      mojom::VideoBitrateAllocation::New();
  std::vector<uint32_t> bitrates = {0u};
  mojom_allocation->bitrates = bitrates;
  mojom::VariableBitratePeakPtr mojom_peak = mojom::VariableBitratePeak::New();
  mojom_peak->bps = 0u;
  mojom_allocation->variable_bitrate_peak = std::move(mojom_peak);
  VideoBitrateAllocation output;

  EXPECT_FALSE(
      mojo::test::SerializeAndDeserialize<mojom::VideoBitrateAllocation>(
          mojom_allocation, output));
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
  constexpr gfx::Size kBaseSize(320, 180);
  constexpr uint32_t kBaseBitrateBps = 123456u;
  constexpr uint32_t kBaseFramerate = 24u;
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
  constexpr ::media::Bitrate kBitrate =
      ::media::Bitrate::ConstantBitrate(kBaseBitrateBps);

  ::media::VideoEncodeAccelerator::Config input_config(
      ::media::PIXEL_FORMAT_NV12, kBaseSize, ::media::VP9PROFILE_PROFILE0,
      kBitrate, kBaseFramerate,
      ::media::VideoEncodeAccelerator::Config::StorageType::kGpuMemoryBuffer,
      ::media::VideoEncodeAccelerator::Config::ContentType::kCamera);
  input_config.drop_frame_thresh_percentage = 30;
  input_config.spatial_layers = input_spatial_layers;
  input_config.inter_layer_pred = ::media::SVCInterLayerPredMode::kOnKeyPic;

  ::media::VideoEncodeAccelerator::Config output_config{};
  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::VideoEncodeAcceleratorConfig>(
          input_config, output_config));
  EXPECT_EQ(input_config, output_config);
}

TEST(VideoEncodeAcceleratorConfigStructTraitTest, RoundTripVariableBitrate) {
  constexpr gfx::Size kBaseSize(320, 180);
  constexpr uint32_t kBaseBitrateBps = 123456u;
  constexpr uint32_t kMaximumBitrate = 999999u;
  const ::media::Bitrate kBitrate =
      ::media::Bitrate::VariableBitrate(kBaseBitrateBps, kMaximumBitrate);
  ::media::VideoEncodeAccelerator::Config input_config(
      ::media::PIXEL_FORMAT_NV12, kBaseSize, ::media::VP9PROFILE_PROFILE0,
      kBitrate, 30,
      ::media::VideoEncodeAccelerator::Config::StorageType::kGpuMemoryBuffer,
      ::media::VideoEncodeAccelerator::Config::ContentType::kCamera);

  ::media::VideoEncodeAccelerator::Config output_config{};
  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::VideoEncodeAcceleratorConfig>(
          input_config, output_config));
  EXPECT_EQ(input_config, output_config);
}

TEST(VariableBitrateStructTraitTest, PeakZeroBps_Rejected) {
  mojom::VariableBitratePtr mojom_variable_bitrate =
      mojom::VariableBitrate::New();
  mojom_variable_bitrate->target_bps = 0u;
  mojom_variable_bitrate->peak_bps = 0u;
  Bitrate output;

  bool result = mojo::test::SerializeAndDeserialize<mojom::VariableBitrate>(
      mojom_variable_bitrate, output);
  EXPECT_FALSE(result);
}

TEST(VariableBitrateStructTraitTest, PeakLessThanTarget_Rejected) {
  mojom::VariableBitratePtr mojom_variable_bitrate =
      mojom::VariableBitrate::New();
  mojom_variable_bitrate->target_bps = 6000u;
  mojom_variable_bitrate->peak_bps = 5999u;
  Bitrate output;

  bool result = mojo::test::SerializeAndDeserialize<mojom::VariableBitrate>(
      mojom_variable_bitrate, output);
  EXPECT_FALSE(result);
}

TEST(BitstreamBufferMetadataTraitTest, RoundTrip) {
  ::media::BitstreamBufferMetadata input_metadata;
  input_metadata.payload_size_bytes = 1234;
  input_metadata.key_frame = true;
  input_metadata.timestamp = base::Milliseconds(123456);
  ::media::BitstreamBufferMetadata output_metadata;
  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::BitstreamBufferMetadata>(
          input_metadata, output_metadata));
  EXPECT_EQ(input_metadata, output_metadata);

  H264Metadata h264;
  h264.temporal_idx = 1;
  h264.layer_sync = true;
  input_metadata.h264 = h264;
  output_metadata = ::media::BitstreamBufferMetadata();
  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::BitstreamBufferMetadata>(
          input_metadata, output_metadata));
  EXPECT_EQ(input_metadata, output_metadata);
  input_metadata.h264.reset();

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
  vp9.inter_pic_predicted = true;
  vp9.temporal_up_switch = true;
  vp9.referenced_by_upper_spatial_layers = true;
  vp9.reference_lower_spatial_layers = true;
  vp9.end_of_picture = false;
  vp9.temporal_idx = 2;
  vp9.spatial_idx = 0;
  vp9.spatial_layer_resolutions = {gfx::Size(320, 180), gfx::Size(640, 360)};
  vp9.p_diffs = {0, 1};
  input_metadata.vp9 = vp9;
  output_metadata = ::media::BitstreamBufferMetadata();
  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::BitstreamBufferMetadata>(
          input_metadata, output_metadata));
  EXPECT_EQ(input_metadata, output_metadata);

  input_metadata =
      BitstreamBufferMetadata::CreateForDropFrame(base::Milliseconds(123456),
                                                  /*spatial_idx=*/1u, false);
  CHECK(input_metadata.drop);
  output_metadata = ::media::BitstreamBufferMetadata();
  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::BitstreamBufferMetadata>(
          input_metadata, output_metadata));
  EXPECT_EQ(input_metadata, output_metadata);
}
}  // namespace media
