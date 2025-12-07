// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/ffmpeg_aac_bitstream_converter.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <array>

#include "base/containers/span.h"
#include "media/ffmpeg/ffmpeg_common.h"
#include "media/ffmpeg/scoped_av_packet.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace {
const int kAacMainProfile = 0;
const int kAacLowComplexityProfile = 1;
}  // namespace

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
    std::ranges::fill(extradata_header_, 0);

    // Set up reasonable aac parameters
    std::ranges::fill(
        base::byte_span_from_ref(base::allow_nonunique_obj, test_parameters_),
        0);
    test_parameters_.codec_id = AV_CODEC_ID_AAC;
    test_parameters_.profile = AV_PROFILE_AAC_MAIN;
    test_parameters_.ch_layout.nb_channels = 2;
    test_parameters_.extradata = extradata_header_;
    test_parameters_.extradata_size = sizeof(extradata_header_);
  }

  void CreatePacket(AVPacket* packet, base::span<const uint8_t> data) {
    // Create new packet sized of |data_size| from |data|.
    EXPECT_EQ(av_new_packet(packet, data.size()), 0);
    AVPacketData(*packet).copy_from_nonoverlapping(data);
  }

  // Variable to hold valid dummy parameters for testing.
  AVCodecParameters test_parameters_;

 private:
  uint8_t extradata_header_[2];
};

TEST_F(FFmpegAACBitstreamConverterTest, Conversion_Success) {
  FFmpegAACBitstreamConverter converter(&test_parameters_);

  std::array<uint8_t, 1000> dummy_packet;
  // Fill dummy packet with junk data. aac converter doesn't look into packet
  // data, just header, so can fill with whatever we want for test.
  for (size_t i = 0;
       i < (dummy_packet.size() * sizeof(decltype(dummy_packet)::value_type));
       i++) {
    dummy_packet[i] = i & 0xFF;  // Repeated sequences of 0-255
  }

  auto test_packet = ScopedAVPacket::Allocate();
  CreatePacket(test_packet.get(), dummy_packet);

  // Try out the actual conversion (should be successful and allocate new
  // packet and destroy the old one).
  EXPECT_TRUE(converter.ConvertPacket(test_packet.get()));

  // Check that a header was added and that packet data was preserved
  EXPECT_EQ(static_cast<long>(test_packet->size),
            static_cast<long>((dummy_packet.size() *
                               sizeof(decltype(dummy_packet)::value_type)) +
                              FFmpegAACBitstreamConverter::kAdtsHeaderSize));
  EXPECT_EQ(AVPacketData(*test_packet)
                .subspan(static_cast<size_t>(
                    FFmpegAACBitstreamConverter::kAdtsHeaderSize)),
            base::span(dummy_packet));
}

TEST_F(FFmpegAACBitstreamConverterTest, Conversion_FailureNullParams) {
  // Set up AVCConfigurationRecord to represent NULL data.
  AVCodecParameters dummy_parameters;
  dummy_parameters.extradata = nullptr;
  dummy_parameters.extradata_size = 0;
  FFmpegAACBitstreamConverter converter(&dummy_parameters);

  uint8_t dummy_packet[1000] = {};

  // Try out the actual conversion with NULL parameter.
  EXPECT_FALSE(converter.ConvertPacket(nullptr));

  // Create new packet to test actual conversion.
  auto test_packet = ScopedAVPacket::Allocate();
  CreatePacket(test_packet.get(), dummy_packet);

  // Try out the actual conversion. This should not fail - conversion is
  // necessary only when we have `extradata`.
  EXPECT_TRUE(converter.ConvertPacket(test_packet.get()));
}

TEST_F(FFmpegAACBitstreamConverterTest, Conversion_AudioProfileType) {
  FFmpegAACBitstreamConverter converter(&test_parameters_);

  uint8_t dummy_packet[1000] = {};

  auto test_packet = ScopedAVPacket::Allocate();
  CreatePacket(test_packet.get(), dummy_packet);

  EXPECT_TRUE(converter.ConvertPacket(test_packet.get()));

  // Check that the ADTS header profile matches the parameters
  auto test_packet_span = AVPacketData(*test_packet);
  int profile = ((test_packet_span[2] & 0xC0) >> 6);

  EXPECT_EQ(profile, kAacMainProfile);

  test_parameters_.profile = AV_PROFILE_AAC_HE;
  FFmpegAACBitstreamConverter converter_he(&test_parameters_);

  test_packet = ScopedAVPacket::Allocate();
  CreatePacket(test_packet.get(), dummy_packet);

  EXPECT_TRUE(converter_he.ConvertPacket(test_packet.get()));

  test_packet_span = AVPacketData(*test_packet);
  profile = ((test_packet_span[2] & 0xC0) >> 6);

  EXPECT_EQ(profile, kAacLowComplexityProfile);

  test_parameters_.profile = AV_PROFILE_AAC_ELD;
  FFmpegAACBitstreamConverter converter_eld(&test_parameters_);

  test_packet = ScopedAVPacket::Allocate();
  CreatePacket(test_packet.get(), dummy_packet);

  EXPECT_FALSE(converter_eld.ConvertPacket(test_packet.get()));
}

TEST_F(FFmpegAACBitstreamConverterTest, Conversion_MultipleLength) {
  FFmpegAACBitstreamConverter converter(&test_parameters_);

  std::array<uint8_t, 1000> dummy_packet;

  auto test_packet = ScopedAVPacket::Allocate();
  CreatePacket(test_packet.get(), dummy_packet);

  // Try out the actual conversion (should be successful and allocate new
  // packet and destroy the old one).
  EXPECT_TRUE(converter.ConvertPacket(test_packet.get()));

  // Check that the ADTS header frame length matches the packet size
  auto test_packet_span = AVPacketData(*test_packet);
  int frame_length = ((test_packet_span[3] & 0x03) << 11) |
                     ((test_packet_span[4] & 0xFF) << 3) |
                     ((test_packet_span[5] & 0xE0) >> 5);

  EXPECT_EQ(frame_length, test_packet->size);

  // Create a second packet that is 1 byte smaller than the first one
  auto second_test_packet = ScopedAVPacket::Allocate();
  CreatePacket(second_test_packet.get(),
               base::span(dummy_packet).first<dummy_packet.size() - 1>());

  // Try out the actual conversion (should be successful and allocate new
  // packet and destroy the old one).
  EXPECT_TRUE(converter.ConvertPacket(second_test_packet.get()));

  // Check that the ADTS header frame length matches the packet size
  auto second_test_packet_span = AVPacketData(*second_test_packet);
  frame_length = ((second_test_packet_span[3] & 0x03) << 11) |
                 ((second_test_packet_span[4] & 0xFF) << 3) |
                 ((second_test_packet_span[5] & 0xE0) >> 5);

  EXPECT_EQ(frame_length, second_test_packet->size);
}

}  // namespace media
