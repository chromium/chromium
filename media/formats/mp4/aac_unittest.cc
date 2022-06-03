// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <string>

#include "media/base/mock_media_log.h"
#include "media/formats/mp4/aac.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::AllOf;
using ::testing::HasSubstr;
using ::testing::InSequence;
using ::testing::StrictMock;

namespace media {

namespace mp4 {

MATCHER_P(UnsupportedFrequencyIndexLog, frequency_index, "") {
  return CONTAINS_STRING(
      arg,
      "Sampling Frequency Index(0x" +
          std::string(frequency_index) + ") is not supported.");
}

MATCHER_P(UnsupportedExtensionFrequencyIndexLog, frequency_index, "") {
  return CONTAINS_STRING(
      arg,
      "Extension Sampling Frequency Index(0x" +
          std::string(frequency_index) + ") is not supported.");
}

MATCHER_P(UnsupportedChannelConfigLog, channel_index, "") {
  return CONTAINS_STRING(
      arg,
      "Channel Configuration(" + std::string(channel_index) +
          ") is not supported");
}

MATCHER_P(UnsupportedAudioProfileLog, profile_string, "") {
  return CONTAINS_STRING(
      arg,
      "Audio codec(" + std::string(profile_string) + ") is not supported");
}

class AACTest : public testing::Test {
 public:
  AACTest() = default;

  bool Parse(const std::vector<uint8_t>& data) {
    return aac_.Parse(data, &media_log_);
  }

  StrictMock<MockMediaLog> media_log_;
  AAC aac_;
};

TEST_F(AACTest, BasicProfileTest) {
  uint8_t buffer[] = {0x12, 0x10};
  std::vector<uint8_t> data;

  data.assign(buffer, buffer + sizeof(buffer));

  EXPECT_TRUE(Parse(data));
  EXPECT_EQ(aac_.GetOutputSamplesPerSecond(false), 44100);
  EXPECT_EQ(aac_.GetChannelLayout(false), CHANNEL_LAYOUT_STEREO);
  EXPECT_EQ(aac_.GetProfile(), AudioCodecProfile::kUnknown);
}

TEST_F(AACTest, ExtensionTest) {
  uint8_t buffer[] = {0x13, 0x08, 0x56, 0xe5, 0x9d, 0x48, 0x80};
  std::vector<uint8_t> data;

  data.assign(buffer, buffer + sizeof(buffer));

  EXPECT_TRUE(Parse(data));
  EXPECT_EQ(aac_.GetOutputSamplesPerSecond(false), 48000);
  EXPECT_EQ(aac_.GetOutputSamplesPerSecond(true), 48000);
  EXPECT_EQ(aac_.GetChannelLayout(false), CHANNEL_LAYOUT_STEREO);
  EXPECT_EQ(aac_.GetProfile(), AudioCodecProfile::kUnknown);
}

// Test implicit SBR with mono channel config.
// Mono channel layout should only be reported if SBR is not
// specified. Otherwise stereo should be reported.
// See ISO 14496-3:2005 Section 1.6.5.3 for details about this special casing.
TEST_F(AACTest, ImplicitSBR_ChannelConfig0) {
  uint8_t buffer[] = {0x13, 0x08};
  std::vector<uint8_t> data;

  data.assign(buffer, buffer + sizeof(buffer));

  EXPECT_TRUE(Parse(data));

  // Test w/o implict SBR.
  EXPECT_EQ(aac_.GetOutputSamplesPerSecond(false), 24000);
  EXPECT_EQ(aac_.GetChannelLayout(false), CHANNEL_LAYOUT_MONO);
  EXPECT_EQ(aac_.GetProfile(), AudioCodecProfile::kUnknown);

  // Test implicit SBR.
  EXPECT_EQ(aac_.GetOutputSamplesPerSecond(true), 48000);
  EXPECT_EQ(aac_.GetChannelLayout(true), CHANNEL_LAYOUT_STEREO);
  EXPECT_EQ(aac_.GetProfile(), AudioCodecProfile::kUnknown);
}

// Tests implicit SBR with a stereo channel config.
TEST_F(AACTest, ImplicitSBR_ChannelConfig1) {
  uint8_t buffer[] = {0x13, 0x10};
  std::vector<uint8_t> data;

  data.assign(buffer, buffer + sizeof(buffer));

  EXPECT_TRUE(Parse(data));

  // Test w/o implict SBR.
  EXPECT_EQ(aac_.GetOutputSamplesPerSecond(false), 24000);
  EXPECT_EQ(aac_.GetChannelLayout(false), CHANNEL_LAYOUT_STEREO);
  EXPECT_EQ(aac_.GetProfile(), AudioCodecProfile::kUnknown);

  // Test implicit SBR.
  EXPECT_EQ(aac_.GetOutputSamplesPerSecond(true), 48000);
  EXPECT_EQ(aac_.GetChannelLayout(true), CHANNEL_LAYOUT_STEREO);
  EXPECT_EQ(aac_.GetProfile(), AudioCodecProfile::kUnknown);
}

TEST_F(AACTest, SixChannelTest) {
  uint8_t buffer[] = {0x11, 0xb0};
  std::vector<uint8_t> data;

  data.assign(buffer, buffer + sizeof(buffer));

  EXPECT_TRUE(Parse(data));
  EXPECT_EQ(aac_.GetOutputSamplesPerSecond(false), 48000);
  EXPECT_EQ(aac_.GetChannelLayout(false), CHANNEL_LAYOUT_5_1_BACK);
  EXPECT_EQ(aac_.GetProfile(), AudioCodecProfile::kUnknown);
}

TEST_F(AACTest, DataTooShortTest) {
  std::vector<uint8_t> data;

  EXPECT_FALSE(Parse(data));

  data.push_back(0x12);
  EXPECT_FALSE(Parse(data));
}

TEST_F(AACTest, IncorrectProfileTest) {
  InSequence s;
  uint8_t buffer[] = {0x0, 0x08};
  std::vector<uint8_t> data;

  data.assign(buffer, buffer + sizeof(buffer));
  EXPECT_MEDIA_LOG(UnsupportedAudioProfileLog("mp4a.40.0"));
  EXPECT_FALSE(Parse(data));

  data[0] = 0x08;

  EXPECT_TRUE(Parse(data));

  data[0] = 0x28;
  // No media log for this profile 5, since not enough bits are in |data| to
  // first parse profile 5's extension frequency index.
  EXPECT_FALSE(Parse(data));
}

TEST_F(AACTest, IncorrectFrequencyTest) {
  uint8_t buffer[] = {0x0f, 0x88};
  std::vector<uint8_t> data;

  data.assign(buffer, buffer + sizeof(buffer));
  EXPECT_FALSE(Parse(data));

  data[0] = 0x0e;
  data[1] = 0x08;
  EXPECT_TRUE(Parse(data));
}

TEST_F(AACTest, IncorrectChannelTest) {
  uint8_t buffer[] = {0x0e, 0x00};
  std::vector<uint8_t> data;

  data.assign(buffer, buffer + sizeof(buffer));
  EXPECT_FALSE(Parse(data));

  data[1] = 0x08;
  EXPECT_TRUE(Parse(data));
}

TEST_F(AACTest, UnsupportedProfileTest) {
  InSequence s;
  uint8_t buffer[] = {0x3a, 0x08};
  std::vector<uint8_t> data;

  data.assign(buffer, buffer + sizeof(buffer));
  EXPECT_MEDIA_LOG(UnsupportedAudioProfileLog("mp4a.40.7"));
  EXPECT_FALSE(Parse(data));

  data[0] = 0x12;
  data[1] = 0x18;
  EXPECT_TRUE(Parse(data));
}

TEST_F(AACTest, UnsupportedChannelLayoutTest) {
  InSequence s;
  uint8_t buffer[] = {0x12, 0x78};
  std::vector<uint8_t> data;

  data.assign(buffer, buffer + sizeof(buffer));
  EXPECT_MEDIA_LOG(UnsupportedChannelConfigLog("15"));
  EXPECT_FALSE(Parse(data));

  data[1] = 0x18;
  EXPECT_TRUE(Parse(data));
}

TEST_F(AACTest, UnsupportedFrequencyIndexTest) {
  InSequence s;
  uint8_t buffer[] = {0x17, 0x10};
  std::vector<uint8_t> data;

  data.assign(buffer, buffer + sizeof(buffer));
  EXPECT_MEDIA_LOG(UnsupportedFrequencyIndexLog("e"));
  EXPECT_FALSE(Parse(data));

  data[0] = 0x13;
  EXPECT_TRUE(Parse(data));
}

TEST_F(AACTest, UnsupportedExFrequencyIndexTest) {
  InSequence s;
  uint8_t buffer[] = {0x29, 0x17, 0x08, 0x0};
  std::vector<uint8_t> data;

  data.assign(buffer, buffer + sizeof(buffer));
  EXPECT_MEDIA_LOG(UnsupportedExtensionFrequencyIndexLog("e"));
  EXPECT_FALSE(Parse(data));

  data[1] = 0x11;
  EXPECT_TRUE(Parse(data));
}

TEST_F(AACTest, XHE_AAC) {
  InSequence s;
  uint8_t buffer[] = {0xf9, 0x46, 0x43, 0x22, 0x2c, 0xc0, 0x4c, 0x00,
                      0x85, 0xa0, 0x01, 0x13, 0x84, 0x00, 0x20, 0x00,
                      0x02, 0x50, 0x01, 0x19, 0x72, 0xc0, 0x00};
  std::vector<uint8_t> data;
  data.assign(buffer, buffer + sizeof(buffer));

  EXPECT_TRUE(Parse(data));
  EXPECT_EQ(aac_.GetOutputSamplesPerSecond(false), 48000);
  EXPECT_EQ(aac_.GetChannelLayout(false), CHANNEL_LAYOUT_STEREO);
  EXPECT_EQ(aac_.GetProfile(), AudioCodecProfile::kXHE_AAC);

  // ADTS conversion should do nothing since xHE-AAC can't be represented with
  // only two bits for the profile.
  EXPECT_TRUE(aac_.ConvertEsdsToADTS(&data));
  EXPECT_EQ(data.size(), sizeof(buffer));
}

}  // namespace mp4

}  // namespace media
