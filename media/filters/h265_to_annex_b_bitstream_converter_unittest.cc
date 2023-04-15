// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/h265_to_annex_b_bitstream_converter.h"

#include <stdint.h>

#include <memory>

#include "media/formats/mp4/box_definitions.h"
#include "media/formats/mp4/hevc.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class H265ToAnnexBBitstreamConverterTest : public testing::Test {
 public:
  H265ToAnnexBBitstreamConverterTest(
      const H265ToAnnexBBitstreamConverterTest&) = delete;
  H265ToAnnexBBitstreamConverterTest& operator=(
      const H265ToAnnexBBitstreamConverterTest&) = delete;

 protected:
  H265ToAnnexBBitstreamConverterTest() = default;

  ~H265ToAnnexBBitstreamConverterTest() override = default;

 protected:
  mp4::HEVCDecoderConfigurationRecord hevc_config_;
};

static const uint8_t kHeaderDataOkWithFieldLen4[] = {
    0x01, 0x01, 0x60, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x96, 0xf0, 0x00, 0xfc, 0xfd, 0xf8, 0xf8, 0x00, 0x00, 0x0f,
    0x03, 0xa0, 0x00, 0x01, 0x00, 0x18, 0x40, 0x01, 0x0c, 0x01, 0xff,
    0xff, 0x01, 0x60, 0x00, 0x00, 0x03, 0x00, 0x80, 0x00, 0x00, 0x03,
    0x00, 0x00, 0x03, 0x00, 0x96, 0x9d, 0xc0, 0x90, 0xa1, 0x00, 0x01,
    0x00, 0x29, 0x42, 0x01, 0x01, 0x01, 0x60, 0x00, 0x00, 0x03, 0x00,
    0x80, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03, 0x00, 0x96, 0xa0, 0x03,
    0xc0, 0x80, 0x10, 0xe5, 0x96, 0x77, 0x92, 0x46, 0xda, 0xf0, 0x10,
    0x10, 0x00, 0x00, 0x3e, 0x80, 0x00, 0x06, 0x1a, 0x80, 0x80, 0xa2,
    0x00, 0x01, 0x00, 0x06, 0x44, 0x01, 0xc1, 0x73, 0xd1, 0x89};

static const uint8_t kPacketDataOkWithFieldLen4[] = {
    0x00, 0x00, 0x00, 0x2d, 0x00, 0x01, 0xe0, 0xa6, 0xf5, 0xd7,
    0xd2, 0x24, 0x0a, 0x19, 0x1a, 0xa0, 0xdc, 0x8c, 0x68, 0x5e,
    0x35, 0x20, 0x40, 0x64, 0x1c, 0x86, 0x81, 0x8a, 0x25, 0x5d,
    0x65, 0x6c, 0xfe, 0x80, 0x7a, 0xe3, 0xf4, 0x63, 0xe1, 0xcf,
    0xf2, 0x6e, 0x92, 0x1e, 0xff, 0xd3, 0x65, 0xd9, 0x60};

TEST_F(H265ToAnnexBBitstreamConverterTest, Success) {
  // Initialize converter.
  std::unique_ptr<uint8_t[]> output;
  H265ToAnnexBBitstreamConverter converter;

  // Parse the headers.
  EXPECT_TRUE(converter.ParseConfiguration(kHeaderDataOkWithFieldLen4,
                                           sizeof(kHeaderDataOkWithFieldLen4),
                                           &hevc_config_));
  uint32_t config_size = converter.GetConfigSize(hevc_config_);
  EXPECT_GT(config_size, 0U);

  // Go on with converting the headers.
  output = std::make_unique<uint8_t[]>(config_size);
  EXPECT_TRUE(output.get() != nullptr);
  EXPECT_TRUE(converter.ConvertHEVCDecoderConfigToByteStream(
      hevc_config_, output.get(), &config_size));

  // Calculate buffer size for actual NAL unit.
  uint32_t output_size = converter.CalculateNeededOutputBufferSize(
      kPacketDataOkWithFieldLen4, sizeof(kPacketDataOkWithFieldLen4),
      &hevc_config_);
  EXPECT_GT(output_size, 0U);
  output = std::make_unique<uint8_t[]>(output_size);
  EXPECT_TRUE(output.get() != nullptr);

  uint32_t output_size_left_for_nal_unit = output_size;
  // Do the conversion for actual NAL unit.
  EXPECT_TRUE(converter.ConvertNalUnitStreamToByteStream(
      kPacketDataOkWithFieldLen4, sizeof(kPacketDataOkWithFieldLen4),
      &hevc_config_, output.get(), &output_size_left_for_nal_unit));
}

TEST_F(H265ToAnnexBBitstreamConverterTest, FailureHeaderBufferOverflow) {
  // Initialize converter
  H265ToAnnexBBitstreamConverter converter;

  // Simulate 10 nalu_array HEVCDecoderConfigurationRecord,
  // which would extend beyond the buffer.
  uint8_t corrupted_header[sizeof(kHeaderDataOkWithFieldLen4)];
  memcpy(corrupted_header, kHeaderDataOkWithFieldLen4,
         sizeof(kHeaderDataOkWithFieldLen4));
  // 23th byte contain the number of nalu arrays
  corrupted_header[22] = corrupted_header[22] | 0xA;

  // Parse the headers
  EXPECT_FALSE(converter.ParseConfiguration(
      corrupted_header, sizeof(corrupted_header), &hevc_config_));
}

TEST_F(H265ToAnnexBBitstreamConverterTest, FailureNalUnitBreakage) {
  // Initialize converter.
  std::unique_ptr<uint8_t[]> output;
  H265ToAnnexBBitstreamConverter converter;

  // Parse the headers.
  EXPECT_TRUE(converter.ParseConfiguration(kHeaderDataOkWithFieldLen4,
                                           sizeof(kHeaderDataOkWithFieldLen4),
                                           &hevc_config_));
  uint32_t config_size = converter.GetConfigSize(hevc_config_);
  EXPECT_GT(config_size, 0U);

  // Go on with converting the headers.
  output = std::make_unique<uint8_t[]>(config_size);
  EXPECT_TRUE(output.get() != nullptr);
  EXPECT_TRUE(converter.ConvertHEVCDecoderConfigToByteStream(
      hevc_config_, output.get(), &config_size));

  // Simulate NAL unit broken in middle by writing only some of the data.
  uint8_t corrupted_nal_unit[sizeof(kPacketDataOkWithFieldLen4) - 30];
  memcpy(corrupted_nal_unit, kPacketDataOkWithFieldLen4,
         sizeof(kPacketDataOkWithFieldLen4) - 30);

  // Calculate buffer size for actual NAL unit, should return 0 because of
  // incomplete input buffer.
  uint32_t output_size = converter.CalculateNeededOutputBufferSize(
      corrupted_nal_unit, sizeof(corrupted_nal_unit), &hevc_config_);
  EXPECT_EQ(output_size, 0U);

  // Ignore the error and try to go on with conversion simulating wrong usage.
  output_size = sizeof(kPacketDataOkWithFieldLen4);
  output = std::make_unique<uint8_t[]>(output_size);
  EXPECT_TRUE(output.get() != nullptr);

  uint32_t output_size_left_for_nal_unit = output_size;
  // Do the conversion for actual NAL unit, expecting failure.
  EXPECT_FALSE(converter.ConvertNalUnitStreamToByteStream(
      corrupted_nal_unit, sizeof(corrupted_nal_unit), &hevc_config_,
      output.get(), &output_size_left_for_nal_unit));
  EXPECT_EQ(output_size_left_for_nal_unit, 0U);
}

TEST_F(H265ToAnnexBBitstreamConverterTest, FailureTooSmallOutputBuffer) {
  // Initialize converter.
  std::unique_ptr<uint8_t[]> output;
  H265ToAnnexBBitstreamConverter converter;

  // Parse the headers.
  EXPECT_TRUE(converter.ParseConfiguration(kHeaderDataOkWithFieldLen4,
                                           sizeof(kHeaderDataOkWithFieldLen4),
                                           &hevc_config_));
  uint32_t config_size = converter.GetConfigSize(hevc_config_);
  EXPECT_GT(config_size, 0U);
  uint32_t real_config_size = config_size;

  // Go on with converting the headers with too small buffer.
  config_size -= 10;
  output = std::make_unique<uint8_t[]>(config_size);
  EXPECT_TRUE(output.get() != nullptr);
  EXPECT_FALSE(converter.ConvertHEVCDecoderConfigToByteStream(
      hevc_config_, output.get(), &config_size));
  EXPECT_EQ(config_size, 0U);

  // Still too small (but only 1 byte short).
  config_size = real_config_size - 1;
  output = std::make_unique<uint8_t[]>(config_size);
  EXPECT_TRUE(output.get() != nullptr);
  EXPECT_FALSE(converter.ConvertHEVCDecoderConfigToByteStream(
      hevc_config_, output.get(), &config_size));
  EXPECT_EQ(config_size, 0U);

  // Finally, retry with valid buffer.
  config_size = real_config_size;
  output = std::make_unique<uint8_t[]>(config_size);
  EXPECT_TRUE(output.get() != nullptr);
  EXPECT_TRUE(converter.ConvertHEVCDecoderConfigToByteStream(
      hevc_config_, output.get(), &config_size));

  // Calculate buffer size for actual NAL unit.
  uint32_t output_size = converter.CalculateNeededOutputBufferSize(
      kPacketDataOkWithFieldLen4, sizeof(kPacketDataOkWithFieldLen4),
      &hevc_config_);
  EXPECT_GT(output_size, 0U);
  // Simulate too small output buffer.
  output_size -= 1;
  output = std::make_unique<uint8_t[]>(output_size);
  EXPECT_TRUE(output.get() != nullptr);

  uint32_t output_size_left_for_nal_unit = output_size;
  // Do the conversion for actual NAL unit (expect failure).
  EXPECT_FALSE(converter.ConvertNalUnitStreamToByteStream(
      kPacketDataOkWithFieldLen4, sizeof(kPacketDataOkWithFieldLen4),
      &hevc_config_, output.get(), &output_size_left_for_nal_unit));
  EXPECT_EQ(output_size_left_for_nal_unit, 0U);
}

static const uint8_t kCorruptedPacketConfiguration[] = {
    0x01, 0x01, 0x60, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x96, 0xf0, 0x00, 0xfc, 0xfd, 0xf8, 0xf8, 0x00, 0x00, 0x0f,
    0x03, 0xa0, 0x00, 0x01, 0x00, 0x18, 0x40, 0x01, 0x0c, 0x01, 0xff,
    0xff, 0x01, 0x60, 0x00, 0x00, 0x03, 0x00, 0x80, 0x00, 0x00, 0x03,
    0x00, 0x00, 0x03, 0x00, 0x96, 0x9d, 0xc0, 0x90, 0xa1, 0x00, 0x01,
    0x00, 0x29, 0x42, 0x01, 0x01, 0x01, 0x60, 0x00, 0x00, 0x03, 0x00,
    0x80, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03, 0x00, 0x96, 0xa0, 0x03,
    0xc0, 0x80, 0x10, 0xe5, 0x96, 0x77, 0x92, 0x46, 0xda, 0xf0, 0x10,
    0x10, 0x00, 0x00, 0x3e, 0x80, 0x00, 0x06, 0x1a, 0x80, 0x80, 0xa2,
    0x00, 0x01, 0x00, 0x06, 0x44, 0x01, 0xc1, 0x73, 0xd1, 0x89};

static const uint8_t kCorruptedPacketData[] = {
    0x00, 0x00, 0x00, 0x15, 0x01, 0x9f, 0x6e, 0xbc, 0x85, 0x3f,
    0x0f, 0x87, 0x47, 0xa8, 0xd7, 0x5b, 0xfc, 0xb8, 0xfd, 0x3f,
    0x57, 0x0e, 0xac, 0xf5, 0x4c, 0x01, 0x2e, 0x57};

TEST_F(H265ToAnnexBBitstreamConverterTest, CorruptedPacket) {
  // Initialize converter.
  std::unique_ptr<uint8_t[]> output;
  H265ToAnnexBBitstreamConverter converter;

  // Parse the headers.
  EXPECT_TRUE(converter.ParseConfiguration(
      kCorruptedPacketConfiguration, sizeof(kCorruptedPacketConfiguration),
      &hevc_config_));
  uint32_t config_size = converter.GetConfigSize(hevc_config_);
  EXPECT_GT(config_size, 0U);

  // Go on with converting the headers.
  output = std::make_unique<uint8_t[]>(config_size);
  EXPECT_TRUE(converter.ConvertHEVCDecoderConfigToByteStream(
      hevc_config_, output.get(), &config_size));

  // Expect an error here.
  uint32_t output_size = converter.CalculateNeededOutputBufferSize(
      kCorruptedPacketData, sizeof(kCorruptedPacketData), &hevc_config_);
  EXPECT_EQ(output_size, 0U);
}

}  // namespace media
