// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/mojom/video_decode_accelerator_config_mojom_traits.h"

#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace {

TEST(VideoDecodeAcceleratorConfig, RoundTrip) {
  VideoDecodeAccelerator::Config input;

  input.profile = VP9PROFILE_PROFILE0;
  input.encryption_scheme = EncryptionScheme::kCbcs;
  input.cdm_id = base::UnguessableToken::Create();
  input.is_deferred_initialization_allowed = true;
  input.overlay_info.is_fullscreen = true;
  input.initial_expected_coded_size = gfx::Size(123, 456);
  input.output_mode = VideoDecodeAccelerator::Config::OutputMode::IMPORT;
  input.supported_output_formats.emplace_back(PIXEL_FORMAT_I420A);
  input.sps.emplace_back(0xA);
  input.pps.emplace_back(0xB);
  input.container_color_space = VideoColorSpace::JPEG();
  input.target_color_space = gfx::ColorSpace::CreateDisplayP3D65();
  input.hdr_metadata.emplace(gfx::HdrMetadataCta861_3(123, 456));

  VideoDecodeAccelerator::Config output;
  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::VideoDecodeAcceleratorConfig>(
          input, output));

  EXPECT_EQ(input.profile, output.profile);
  EXPECT_EQ(input.encryption_scheme, output.encryption_scheme);
  EXPECT_EQ(input.cdm_id, output.cdm_id);
  EXPECT_EQ(input.is_deferred_initialization_allowed,
            output.is_deferred_initialization_allowed);
  EXPECT_EQ(input.overlay_info.is_fullscreen,
            output.overlay_info.is_fullscreen);
  EXPECT_EQ(input.supported_output_formats, output.supported_output_formats);
  EXPECT_EQ(input.sps, output.sps);
  EXPECT_EQ(input.pps, output.pps);
  EXPECT_EQ(input.container_color_space, output.container_color_space);
  EXPECT_EQ(input.target_color_space, output.target_color_space);
  EXPECT_EQ(input.hdr_metadata, output.hdr_metadata);

  // TODO(https://crbug.com/1446302): OutputMode was not included in the IPC
  // version of this serialization. Leave it out for now (to avoid any behavior
  // changes, but consider reintroducing it).
  EXPECT_NE(input.output_mode, output.output_mode);
}

}  // namespace

}  // namespace media
