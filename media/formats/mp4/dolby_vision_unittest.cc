// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/mp4/dolby_vision.h"

#include "media/base/video_codecs.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace mp4 {

class DOVIDecoderConfigurationRecordTest : public testing::Test {};

TEST_F(DOVIDecoderConfigurationRecordTest, Profile0Level1ELTrackTest) {
  DOVIDecoderConfigurationRecord dv_config;
  uint8_t data[] = {0x00, 0x00, 0x00, 0x0E};

  dv_config.ParseForTesting(data, sizeof(data));

  EXPECT_EQ(dv_config.dv_version_major, 0);
  EXPECT_EQ(dv_config.dv_version_minor, 0);
  EXPECT_EQ(dv_config.codec_profile, DOLBYVISION_PROFILE0);
  EXPECT_EQ(dv_config.dv_profile, 0);
  EXPECT_EQ(dv_config.dv_level, 1);
  EXPECT_EQ(dv_config.rpu_present_flag, 1);
  EXPECT_EQ(dv_config.el_present_flag, 1);
  EXPECT_EQ(dv_config.bl_present_flag, 0);
  EXPECT_EQ(dv_config.dv_bl_signal_compatibility_id, 0);
}

TEST_F(DOVIDecoderConfigurationRecordTest, Profile4Test) {
  DOVIDecoderConfigurationRecord dv_config;
  uint8_t data[] = {0x00, 0x00, 0x08, 0x16, 0x20};

  EXPECT_FALSE(dv_config.ParseForTesting(data, sizeof(data)));
}

TEST_F(DOVIDecoderConfigurationRecordTest, Profile5Test) {
  DOVIDecoderConfigurationRecord dv_config;
  uint8_t data[] = {0x00, 0x00, 0x0A, 0x17, 0x00};

  dv_config.ParseForTesting(data, sizeof(data));

  EXPECT_EQ(dv_config.dv_version_major, 0);
  EXPECT_EQ(dv_config.dv_version_minor, 0);
  EXPECT_EQ(dv_config.codec_profile, DOLBYVISION_PROFILE5);
  EXPECT_EQ(dv_config.dv_profile, 5);
  EXPECT_EQ(dv_config.dv_level, 2);
  EXPECT_EQ(dv_config.rpu_present_flag, 1);
  EXPECT_EQ(dv_config.el_present_flag, 1);
  EXPECT_EQ(dv_config.bl_present_flag, 1);
  EXPECT_EQ(dv_config.dv_bl_signal_compatibility_id, 0);
}

TEST_F(DOVIDecoderConfigurationRecordTest, Profile8Point1Test) {
  DOVIDecoderConfigurationRecord dv_config;
  uint8_t data[] = {0x00, 0x00, 0x10, 0x17, 0x10};

  dv_config.ParseForTesting(data, sizeof(data));

  EXPECT_EQ(dv_config.dv_version_major, 0);
  EXPECT_EQ(dv_config.dv_version_minor, 0);
  EXPECT_EQ(dv_config.codec_profile, DOLBYVISION_PROFILE8);
  EXPECT_EQ(dv_config.dv_profile, 8);
  EXPECT_EQ(dv_config.dv_level, 2);
  EXPECT_EQ(dv_config.rpu_present_flag, 1);
  EXPECT_EQ(dv_config.el_present_flag, 1);
  EXPECT_EQ(dv_config.bl_present_flag, 1);
  EXPECT_EQ(dv_config.dv_bl_signal_compatibility_id, 1);
}

TEST_F(DOVIDecoderConfigurationRecordTest, Profile8Point4Test) {
  DOVIDecoderConfigurationRecord dv_config;
  uint8_t data[] = {0x01, 0x00, 0x10, 0x2d, 0x40};

  dv_config.ParseForTesting(data, sizeof(data));

  EXPECT_EQ(dv_config.dv_version_major, 1);
  EXPECT_EQ(dv_config.dv_version_minor, 0);
  EXPECT_EQ(dv_config.codec_profile, DOLBYVISION_PROFILE8);
  EXPECT_EQ(dv_config.dv_profile, 8);
  EXPECT_EQ(dv_config.dv_level, 5);
  EXPECT_EQ(dv_config.rpu_present_flag, 1);
  EXPECT_EQ(dv_config.el_present_flag, 0);
  EXPECT_EQ(dv_config.bl_present_flag, 1);
  EXPECT_EQ(dv_config.dv_bl_signal_compatibility_id, 4);
}

TEST_F(DOVIDecoderConfigurationRecordTest, Profile9Test) {
  DOVIDecoderConfigurationRecord dv_config;
  uint8_t data[] = {0x00, 0x00, 0x12, 0x17, 0x20};

  dv_config.ParseForTesting(data, sizeof(data));

  EXPECT_EQ(dv_config.dv_version_major, 0);
  EXPECT_EQ(dv_config.dv_version_minor, 0);
  EXPECT_EQ(dv_config.codec_profile, DOLBYVISION_PROFILE9);
  EXPECT_EQ(dv_config.dv_profile, 9);
  EXPECT_EQ(dv_config.dv_level, 2);
  EXPECT_EQ(dv_config.rpu_present_flag, 1);
  EXPECT_EQ(dv_config.el_present_flag, 1);
  EXPECT_EQ(dv_config.bl_present_flag, 1);
  EXPECT_EQ(dv_config.dv_bl_signal_compatibility_id, 2);
}

TEST_F(DOVIDecoderConfigurationRecordTest, ParseNotEnoughData) {
  DOVIDecoderConfigurationRecord dv_config;
  uint8_t data[] = {0x00, 0x00, 0x0C};

  EXPECT_FALSE(dv_config.ParseForTesting(data, sizeof(data)));
}

}  // namespace mp4
}  // namespace media
