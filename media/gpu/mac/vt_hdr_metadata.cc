// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/mac/vt_hdr_metadata.h"

#include <optional>
#include <vector>

#include "base/apple/foundation_util.h"
#include "base/notreached.h"
#include "base/numerics/byte_conversions.h"
#include "media/filters/h26x_annex_b_bitstream_builder.h"
#include "media/gpu/h264_builder.h"
#include "media/media_buildflags.h"
#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
#include "media/gpu/h265_builder.h"
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)

namespace media {

namespace {

constexpr size_t kMdcvPayloadSize = 24;
constexpr size_t kClliPayloadSize = 4;

CFDataRef GetDataExtension(CMFormatDescriptionRef format_desc,
                           CFStringRef key) {
  CFTypeRef value = CMFormatDescriptionGetExtension(format_desc, key);
  if (!value || CFGetTypeID(value) != CFDataGetTypeID()) {
    return nullptr;
  }
  return static_cast<CFDataRef>(value);
}

base::span<const uint8_t> CFDataAsSpan(CFDataRef data) {
  return data ? base::apple::CFDataToSpan(data) : base::span<const uint8_t>();
}

std::optional<H26xSEIMasteringDisplayInfo> ParseMdcvPayload(
    base::span<const uint8_t> payload) {
  if (payload.size() != kMdcvPayloadSize) {
    return std::nullopt;
  }
  H26xSEIMasteringDisplayInfo info;
  size_t offset = 0;
  for (auto& primary : info.display_primaries) {
    primary[0] = base::U16FromBigEndian(payload.subspan(offset).first<2>());
    offset += 2;
    primary[1] = base::U16FromBigEndian(payload.subspan(offset).first<2>());
    offset += 2;
  }
  info.white_points[0] =
      base::U16FromBigEndian(payload.subspan(offset).first<2>());
  offset += 2;
  info.white_points[1] =
      base::U16FromBigEndian(payload.subspan(offset).first<2>());
  offset += 2;
  info.max_luminance =
      base::U32FromBigEndian(payload.subspan(offset).first<4>());
  offset += 4;
  info.min_luminance =
      base::U32FromBigEndian(payload.subspan(offset).first<4>());
  return info;
}

std::optional<H26xSEIContentLightLevelInfo> ParseClliPayload(
    base::span<const uint8_t> payload) {
  if (payload.size() != kClliPayloadSize) {
    return std::nullopt;
  }
  return H26xSEIContentLightLevelInfo{
      .max_content_light_level = base::U16FromBigEndian(payload.first<2>()),
      .max_picture_average_light_level =
          base::U16FromBigEndian(payload.subspan(2u).first<2>()),
  };
}

std::vector<uint8_t> BuildHdrSeiNalu(VideoCodec codec,
                                     CFDataRef mdcv_data,
                                     CFDataRef clli_data) {
  const auto mdcv = ParseMdcvPayload(CFDataAsSpan(mdcv_data));
  const auto clli = ParseClliPayload(CFDataAsSpan(clli_data));
  if (!mdcv && !clli) {
    return {};
  }

  H26xAnnexBBitstreamBuilder builder(
      /*insert_emulation_prevention_bytes=*/true);
  switch (codec) {
    case VideoCodec::kH264:
      BuildPackedH264SEI(builder, mdcv, clli);
      break;
#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
    case VideoCodec::kHEVC:
      BuildPackedH265SEI(builder, mdcv, clli);
      break;
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
    default:
      NOTREACHED();
  }
  auto data = builder.data();
  return std::vector<uint8_t>(data.begin(), data.end());
}

}  // namespace

std::vector<uint8_t> BuildHdrMetadataSeiNalu(VideoCodec codec,
                                             CMSampleBufferRef sample_buffer) {
  CMFormatDescriptionRef format_desc =
      CMSampleBufferGetFormatDescription(sample_buffer);
  if (!format_desc) {
    return {};
  }

  const CFDataRef mdcv_data = GetDataExtension(
      format_desc, kCMFormatDescriptionExtension_MasteringDisplayColorVolume);
  const CFDataRef clli_data = GetDataExtension(
      format_desc, kCMFormatDescriptionExtension_ContentLightLevelInfo);
  return BuildHdrSeiNalu(codec, mdcv_data, clli_data);
}

}  // namespace media
