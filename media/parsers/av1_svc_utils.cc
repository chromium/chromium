// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/parsers/av1_svc_utils.h"

#include <algorithm>

namespace media {

namespace {
constexpr int kOperatingPoint = 0;
}  // namespace

bool IsAV1RtpWildcardOperatingPoint(
    const libgav1::ObuSequenceHeader& sequence_header) {
  constexpr int kRtpWildcardOperatingPointIdc = 0b1111'1111'1111;  // 0xfff
  return sequence_header.operating_points == 1 &&
         sequence_header.operating_point_idc[kOperatingPoint] ==
             kRtpWildcardOperatingPointIdc;
}

bool IsAV1SpatialLayerSequence(
    const libgav1::ObuSequenceHeader& sequence_header) {
  if (IsAV1RtpWildcardOperatingPoint(sequence_header)) {
    return false;
  }
  // Spec 6.4.1.
  constexpr int kTemporalLayerBitMaskBits = 8;
  const int operating_point_idc =
      sequence_header.operating_point_idc[kOperatingPoint];
  const int used_spatial_layer_bit_mask =
      (operating_point_idc >> kTemporalLayerBitMaskBits) & 0b1111;
  // In case of an only temporal layer encoding e.g. L1T3, spatial layer#0 bit
  // is 1. We allow this case.
  return used_spatial_layer_bit_mask > 1;
}

bool HasAV1SpatialLayerObu(
    const libgav1::Vector<libgav1::ObuHeader>& obu_headers) {
  return std::ranges::any_of(obu_headers, [](const libgav1::ObuHeader& header) {
    return header.spatial_id > 0;
  });
}

libgav1::StatusCode ParseOneAV1Frame(
    libgav1::ObuParser& parser,
    libgav1::RefCountedBufferPtr* current_frame) {
  libgav1::StatusCode status_code = parser.ParseOneFrame(current_frame);
  // ObuParser::ParseOneFrame() only populates `*current_frame` immediately
  // before running CheckLevelConformance() at the very end of the function.
  // When the RTP wildcard operating point is used with seq_level_idx[0] = 0
  // (Level 2.0), seq_level_idx[0] does not reflect the level of the operating
  // point and CheckLevelConformance() will fail for resolutions above Level
  // 2.0 (147,456 pixels).
  if (status_code == libgav1::kStatusBitstreamError && *current_frame &&
      IsAV1RtpWildcardOperatingPoint(parser.sequence_header()) &&
      parser.sequence_header().level[kOperatingPoint].major == 2 &&
      parser.sequence_header().level[kOperatingPoint].minor == 0) {
    return libgav1::kStatusOk;
  }
  return status_code;
}

}  // namespace media
