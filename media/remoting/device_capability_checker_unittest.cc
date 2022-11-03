// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/remoting/device_capability_checker.h"

#include "media/base/audio_codecs.h"
#include "media/base/video_codecs.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media::remoting {

namespace {
constexpr char cc_model_name[] = "Chromecast";
constexpr char cc_ultra_model_name[] = "Chromecast Ultra";
constexpr char cc_dongle_model_name[] = "Eureka Dongle";
}  // namespace

TEST(DeviceCapabilityCheckerTest, CheckDeviceCapability) {
  EXPECT_TRUE(IsKnownToSupportRemoting(cc_model_name));
  EXPECT_TRUE(IsKnownToSupportRemoting(cc_ultra_model_name));
  EXPECT_TRUE(IsKnownToSupportRemoting(cc_dongle_model_name));
  EXPECT_FALSE(IsKnownToSupportRemoting("Nest"));
}

TEST(DeviceCapabilityCheckerTest, CheckVideoCapability) {
  EXPECT_FALSE(IsVideoCodecCompatible(cc_model_name, VideoCodec::kHEVC));
  EXPECT_TRUE(IsVideoCodecCompatible(cc_ultra_model_name, VideoCodec::kHEVC));
  EXPECT_TRUE(IsVideoCodecCompatible(cc_ultra_model_name, VideoCodec::kVP9));
  EXPECT_TRUE(IsVideoCodecCompatible(cc_dongle_model_name, VideoCodec::kVP8));
}

TEST(DeviceCapabilityCheckerTest, CheckAudioCapability) {
  EXPECT_FALSE(IsAudioCodecCompatible(cc_ultra_model_name, AudioCodec::kMP3));
  EXPECT_TRUE(IsAudioCodecCompatible(cc_model_name, AudioCodec::kAAC));
  EXPECT_TRUE(IsAudioCodecCompatible(cc_dongle_model_name, AudioCodec::kOpus));
}
}  // namespace media::remoting
