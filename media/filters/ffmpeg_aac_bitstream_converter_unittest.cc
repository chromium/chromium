// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stddef.h>
#include <stdint.h>

#include "media/ffmpeg/ffmpeg_common.h"
#include "media/ffmpeg/scoped_av_packet.h"
#include "media/filters/ffmpeg_aac_bitstream_converter.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace {
const int kAacMainProfile = 0;
const int kAacLowComplexityProfile = 1;
} // namespace

// Class for testing the FFmpegAACBitstreamConverter.
class FFmpegAACBitstreamConverterTest : public testing::Test {
 public:
  FFmpegAACBitstreamConverterTest(const FFmpegAACBitstreamConverterTest&) =
      delete;
  FFmpegAACBitstreamConverterTest& operator=(
      const FFmpegAACBitstreamConverterTest&) = delete;

 protected:
  FFmpegAACBitstreamConverterTest() {
    // Minimal extra data header
    memset(extradata_header_, 0, sizeof(extradata_header_));

    // Set up reasonable aac parameters
    memset(&test_parameters_, 0, sizeof(AVCodecParameters));
    test_parameters_.codec_id = AV_CODEC_ID_AAC;
    test_parameters_.profile = FF_PROFILE_AAC_MAIN;
    test_parameters_.ch_layout.nb_channels = 2;
    test_parameters_.extradata = extradata_header_;
    test_parameters_.extradata_size = sizeof(extradata_header_);
  }

  void CreatePacket(AVPacket* packet, const uint8_t* data, uint32_t data_size) {
    // Create new packet sized of |data_size| from |data|.
    EXPECT_EQ(av_new_packet(packet, data_size), 0);
    memcpy(packet->data, data, data_size);
  }

  // Variable to hold valid dummy parameters for testing.
  AVCodecParameters test_parameters_;

 private:
  uint8_t extradata_header_[2];
};

TEST_F(FFmpegAACBitstreamConverterTest, Conversion_Success) {
  FFmpegAACBitstreamConverter converter(&test_parameters_);

  uint8_t dummy_packet[1000];
  // Fill dummy packet with junk data. aac converter doesn't look into packet
  // data, just header, so can fill with whatever we want for test.
  for(size_t i = 0; i < sizeof(dummy_packet); i++) {
    dummy_packet[i] = i & 0xFF; // Repeated sequences of 0-255
  }

  auto test_packet = ScopedAVPacket::Allocate();
  CreatePacket(test_packet.get(), dummy_packet,
               sizeof(dummy_packet));

  // Try out the actual conversion (should be successful and allocate new
  // packet and destroy the old one).
  EXPECT_TRUE(converter.ConvertPacket(test_packet.get()));

  // Check that a header was added and that packet data was preserved
  EXPECT_EQ(static_cast<long>(test_packet->size),
            static_cast<long>(sizeof(dummy_packet) +
                              FFmpegAACBitstreamConverter::kAdtsHeaderSize));
  EXPECT_EQ(memcmp(
      reinterpret_cast<void*>(test_packet->data +
                              FFmpegAACBitstreamConverter::kAdtsHeaderSize),
      reinterpret_cast<void*>(dummy_packet),
      sizeof(dummy_packet)), 0);
}

TEST_F(FFmpegAACBitstreamConverterTest, Conversion_FailureNullParams) {
  // Set up AVCConfigurationRecord to represent NULL data.
  AVCodecParameters dummy_parameters;
  dummy_parameters.extradata = nullptr;
  dummy_parameters.extradata_size = 0;
  FFmpegAACBitstreamConverter converter(&dummy_parameters);

  uint8_t dummy_packet[1000] = {0};

  // Try out the actual conversion with NULL parameter.
  EXPECT_FALSE(converter.ConvertPacket(NULL));

  // Create new packet to test actual conversion.
  auto test_packet = ScopedAVPacket::Allocate();
  CreatePacket(test_packet.get(), dummy_packet, sizeof(dummy_packet));

  // Try out the actual conversion. This should fail due to missing extradata.
  EXPECT_FALSE(converter.ConvertPacket(test_packet.get()));
}

TEST_F(FFmpegAACBitstreamConverterTest, Conversion_AudioProfileType) {
  FFmpegAACBitstreamConverter converter(&test_parameters_);

  uint8_t dummy_packet[1000] = {0};

  auto test_packet = ScopedAVPacket::Allocate();
  CreatePacket(test_packet.get(), dummy_packet,
               sizeof(dummy_packet));

  EXPECT_TRUE(converter.ConvertPacket(test_packet.get()));

  // Check that the ADTS header profile matches the parameters
  int profile = ((test_packet->data[2] & 0xC0) >> 6);

  EXPECT_EQ(profile, kAacMainProfile);

  test_parameters_.profile = FF_PROFILE_AAC_HE;
  FFmpegAACBitstreamConverter converter_he(&test_parameters_);

  test_packet = ScopedAVPacket::Allocate();
  CreatePacket(test_packet.get(), dummy_packet,
               sizeof(dummy_packet));

  EXPECT_TRUE(converter_he.ConvertPacket(test_packet.get()));

  profile = ((test_packet->data[2] & 0xC0) >> 6);

  EXPECT_EQ(profile, kAacLowComplexityProfile);

  test_parameters_.profile = FF_PROFILE_AAC_ELD;
  FFmpegAACBitstreamConverter converter_eld(&test_parameters_);

  test_packet = ScopedAVPacket::Allocate();
  CreatePacket(test_packet.get(), dummy_packet,
               sizeof(dummy_packet));

  EXPECT_FALSE(converter_eld.ConvertPacket(test_packet.get()));
}

TEST_F(FFmpegAACBitstreamConverterTest, Conversion_MultipleLength) {
  FFmpegAACBitstreamConverter converter(&test_parameters_);

  uint8_t dummy_packet[1000];

  auto test_packet = ScopedAVPacket::Allocate();
  CreatePacket(test_packet.get(), dummy_packet,
               sizeof(dummy_packet));

  // Try out the actual conversion (should be successful and allocate new
  // packet and destroy the old one).
  EXPECT_TRUE(converter.ConvertPacket(test_packet.get()));

  // Check that the ADTS header frame length matches the packet size
  int frame_length = ((test_packet->data[3] & 0x03) << 11) |
                     ((test_packet->data[4] & 0xFF) << 3) |
                     ((test_packet->data[5] & 0xE0) >> 5);

  EXPECT_EQ(frame_length, test_packet->size);

  // Create a second packet that is 1 byte smaller than the first one
  auto second_test_packet = ScopedAVPacket::Allocate();
  CreatePacket(second_test_packet.get(), dummy_packet,
               sizeof(dummy_packet) - 1);

  // Try out the actual conversion (should be successful and allocate new
  // packet and destroy the old one).
  EXPECT_TRUE(converter.ConvertPacket(second_test_packet.get()));

  // Check that the ADTS header frame length matches the packet size
  frame_length = ((second_test_packet->data[3] & 0x03) << 11) |
                 ((second_test_packet->data[4] & 0xFF) << 3) |
                 ((second_test_packet->data[5] & 0xE0) >> 5);

  EXPECT_EQ(frame_length, second_test_packet->size);
}

}  // namespace media
