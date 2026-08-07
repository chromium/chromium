// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/mac/vt_hdr_metadata.h"

#include <CoreFoundation/CoreFoundation.h>
#include <stdint.h>

#include <array>
#include <utility>
#include <variant>
#include <vector>

#include "base/apple/scoped_cftyperef.h"
#include "base/check_op.h"
#include "media/media_buildflags.h"
#include "media/parsers/h264_parser.h"
#include "media/parsers/h26x_parser.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
#include "media/parsers/h265_parser.h"
#endif

namespace media {

namespace {

// mastering_display_colour_volume as VideoToolbox reports it: Rec.2020
// primaries in GBR order and the D65 white point in 0.00002 units, then the
// mastering luminance range of 1000 to 0.005 nits in 0.0001 units.
constexpr auto kMdcvPayload = std::to_array<uint8_t>({
    0x21, 0x34,              // display_primaries_x[0] = 8500
    0x9b, 0xaa,              // display_primaries_y[0] = 39850
    0x19, 0x96,              // display_primaries_x[1] = 6550
    0x08, 0xfc,              // display_primaries_y[1] = 2300
    0x8a, 0x48,              // display_primaries_x[2] = 35400
    0x39, 0x08,              // display_primaries_y[2] = 14600
    0x3d, 0x13,              // white_point_x = 15635
    0x40, 0x42,              // white_point_y = 16450
    0x00, 0x98, 0x96, 0x80,  // max_display_mastering_luminance = 10000000
    0x00, 0x00, 0x00, 0x32,  // min_display_mastering_luminance = 50
});

// content_light_level_info with maxCLL 1000 and maxFALL 400.
constexpr auto kClliPayload = std::to_array<uint8_t>({0x03, 0xe8, 0x01, 0x90});

base::apple::ScopedCFTypeRef<CFDataRef> MakeCFData(
    base::span<const uint8_t> bytes) {
  return base::apple::ScopedCFTypeRef<CFDataRef>(
      CFDataCreate(kCFAllocatorDefault, bytes.data(), bytes.size()));
}

base::apple::ScopedCFTypeRef<CMSampleBufferRef> MakeSampleBuffer(
    base::span<const uint8_t> mdcv,
    base::span<const uint8_t> clli,
    CMVideoCodecType codec_type = kCMVideoCodecType_HEVC) {
  base::apple::ScopedCFTypeRef<CFMutableDictionaryRef> extensions(
      CFDictionaryCreateMutable(kCFAllocatorDefault, 2,
                                &kCFTypeDictionaryKeyCallBacks,
                                &kCFTypeDictionaryValueCallBacks));
  auto mdcv_data = MakeCFData(mdcv);
  if (!mdcv.empty()) {
    CFDictionarySetValue(
        extensions.get(),
        kCMFormatDescriptionExtension_MasteringDisplayColorVolume,
        mdcv_data.get());
  }
  auto clli_data = MakeCFData(clli);
  if (!clli.empty()) {
    CFDictionarySetValue(extensions.get(),
                         kCMFormatDescriptionExtension_ContentLightLevelInfo,
                         clli_data.get());
  }

  base::apple::ScopedCFTypeRef<CMFormatDescriptionRef> format;
  CHECK_EQ(CMFormatDescriptionCreate(kCFAllocatorDefault, kCMMediaType_Video,
                                     codec_type, extensions.get(),
                                     format.InitializeInto()),
           noErr);

  base::apple::ScopedCFTypeRef<CMSampleBufferRef> sample_buffer;
  CHECK_EQ(CMSampleBufferCreate(kCFAllocatorDefault, nullptr, true, nullptr,
                                nullptr, format.get(), 0, 0, nullptr, 0,
                                nullptr, sample_buffer.InitializeInto()),
           noErr);
  return sample_buffer;
}

#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
struct ParsedH265AccessUnit {
  std::vector<int> nal_unit_types;
  std::vector<H265SEIMessage> sei_msgs;
};

ParsedH265AccessUnit ParseH265(base::span<const uint8_t> access_unit) {
  ParsedH265AccessUnit parsed;
  H265Parser parser;
  parser.SetStream(access_unit);

  H265NALU nalu;
  while (parser.AdvanceToNextNALU(&nalu) == H265Parser::kOk) {
    parsed.nal_unit_types.push_back(nalu.nal_unit_type);
    if (nalu.nal_unit_type == H265NALU::PREFIX_SEI_NUT) {
      H265SEI sei;
      EXPECT_EQ(parser.ParseSEI(&sei), H265Parser::kOk);
      for (auto& msg : sei.msgs) {
        parsed.sei_msgs.push_back(std::move(msg));
      }
    }
  }
  return parsed;
}
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)

struct ParsedH264AccessUnit {
  std::vector<int> nal_unit_types;
  std::vector<H264SEIMessage> sei_msgs;
};

ParsedH264AccessUnit ParseH264(base::span<const uint8_t> access_unit) {
  ParsedH264AccessUnit parsed;
  H264Parser parser;
  parser.SetStream(access_unit);

  H264NALU nalu;
  while (parser.AdvanceToNextNALU(&nalu) == H264Parser::kOk) {
    parsed.nal_unit_types.push_back(nalu.nal_unit_type);
    if (nalu.nal_unit_type == H264NALU::kSEIMessage) {
      H264SEI sei;
      EXPECT_EQ(parser.ParseSEI(&sei), H264Parser::kOk);
      for (auto& msg : sei.msgs) {
        parsed.sei_msgs.push_back(std::move(msg));
      }
    }
  }
  return parsed;
}

}  // namespace

TEST(VtHdrMetadataTest, BuildsH264SeiNalu) {
  auto sample_buffer =
      MakeSampleBuffer(kMdcvPayload, kClliPayload, kCMVideoCodecType_H264);

  const std::vector<uint8_t> sei_nalu =
      BuildHdrMetadataSeiNalu(VideoCodec::kH264, sample_buffer.get());
  ASSERT_FALSE(sei_nalu.empty());

  const ParsedH264AccessUnit parsed = ParseH264(sei_nalu);
  EXPECT_EQ(parsed.nal_unit_types, std::vector<int>({H264NALU::kSEIMessage}));
  ASSERT_EQ(parsed.sei_msgs.size(), 2u);
  const auto* mdcv =
      std::get_if<H26xSEIMasteringDisplayInfo>(&parsed.sei_msgs[0]);
  const auto* clli =
      std::get_if<H26xSEIContentLightLevelInfo>(&parsed.sei_msgs[1]);
  ASSERT_TRUE(mdcv);
  ASSERT_TRUE(clli);
  EXPECT_EQ(mdcv->max_luminance, 10000000u);
  EXPECT_EQ(mdcv->min_luminance, 50u);
  EXPECT_EQ(clli->max_content_light_level, 1000u);
  EXPECT_EQ(clli->max_picture_average_light_level, 400u);
}

#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
TEST(VtHdrMetadataTest, BuildsH265SeiNalu) {
  auto sample_buffer = MakeSampleBuffer(kMdcvPayload, kClliPayload);

  const std::vector<uint8_t> sei_nalu =
      BuildHdrMetadataSeiNalu(VideoCodec::kHEVC, sample_buffer.get());
  ASSERT_FALSE(sei_nalu.empty());

  const ParsedH265AccessUnit parsed = ParseH265(sei_nalu);
  EXPECT_EQ(parsed.nal_unit_types,
            std::vector<int>({H265NALU::PREFIX_SEI_NUT}));

  ASSERT_EQ(parsed.sei_msgs.size(), 2u);
  const auto* mdcv =
      std::get_if<H26xSEIMasteringDisplayInfo>(&parsed.sei_msgs[0]);
  const auto* clli =
      std::get_if<H26xSEIContentLightLevelInfo>(&parsed.sei_msgs[1]);
  ASSERT_TRUE(mdcv);
  ASSERT_TRUE(clli);
  EXPECT_EQ(mdcv->display_primaries[0][0], 8500u);
  EXPECT_EQ(mdcv->display_primaries[0][1], 39850u);
  EXPECT_EQ(mdcv->max_luminance, 10000000u);
  EXPECT_EQ(mdcv->min_luminance, 50u);
  EXPECT_EQ(clli->max_content_light_level, 1000u);
  EXPECT_EQ(clli->max_picture_average_light_level, 400u);
}

TEST(VtHdrMetadataTest, BuildsClliWhenMdcvIsUnavailable) {
  auto sample_buffer = MakeSampleBuffer({}, kClliPayload);

  const std::vector<uint8_t> sei_nalu =
      BuildHdrMetadataSeiNalu(VideoCodec::kHEVC, sample_buffer.get());
  ASSERT_FALSE(sei_nalu.empty());

  const ParsedH265AccessUnit parsed = ParseH265(sei_nalu);
  EXPECT_EQ(parsed.nal_unit_types,
            std::vector<int>({H265NALU::PREFIX_SEI_NUT}));
  ASSERT_EQ(parsed.sei_msgs.size(), 1u);
  EXPECT_TRUE(
      std::holds_alternative<H26xSEIContentLightLevelInfo>(parsed.sei_msgs[0]));
}

TEST(VtHdrMetadataTest, ReturnsEmptyWithoutHdrMetadata) {
  auto sample_buffer = MakeSampleBuffer({}, {});

  EXPECT_TRUE(
      BuildHdrMetadataSeiNalu(VideoCodec::kHEVC, sample_buffer.get()).empty());
}
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)

}  // namespace media
