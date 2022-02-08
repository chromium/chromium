// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/mojom/audio_processing_mojom_traits.h"

#include "media/base/audio_processing.h"
#include "media/mojo/mojom/traits_test_service.mojom.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace {

class AudioProcessingMojomTraitsTest : public testing::Test {
 public:
  AudioProcessingMojomTraitsTest() = default;

  AudioProcessingMojomTraitsTest(const AudioProcessingMojomTraitsTest&) =
      delete;
  AudioProcessingMojomTraitsTest& operator=(
      const AudioProcessingMojomTraitsTest&) = delete;
};

}  // namespace

TEST_F(AudioProcessingMojomTraitsTest, AudioProcessingSettings) {
  AudioProcessingSettings settings_in;
  AudioProcessingSettings settings_out;

  mojo::test::SerializeAndDeserialize<media::mojom::AudioProcessingSettings>(
      settings_in, settings_out);

  EXPECT_EQ(settings_in, settings_out);

  // Flip all fields.
  settings_in.echo_cancellation = !settings_in.echo_cancellation;
  settings_in.noise_suppression = !settings_in.noise_suppression;
  settings_in.transient_noise_suppression =
      !settings_in.transient_noise_suppression;
  settings_in.automatic_gain_control = !settings_in.automatic_gain_control;
  settings_in.experimental_automatic_gain_control =
      !settings_in.experimental_automatic_gain_control;
  settings_in.high_pass_filter = !settings_in.high_pass_filter;
  settings_in.multi_channel_capture_processing =
      !settings_in.multi_channel_capture_processing;
  settings_in.stereo_mirroring = !settings_in.stereo_mirroring;
  settings_in.force_apm_creation = !settings_in.force_apm_creation;

  mojo::test::SerializeAndDeserialize<media::mojom::AudioProcessingSettings>(
      settings_in, settings_out);

  EXPECT_EQ(settings_in, settings_out);
}

}  // namespace media
