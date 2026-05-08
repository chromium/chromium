// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/v4l2/vp9/v4l2_vp9_compressed_header.h"

#include "base/check.h"
#include "base/logging.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "media/gpu/v4l2/vp9/vp9_compressed_header_parser.h"
#include "media/parsers/vp9_parser.h"

namespace media {

std::optional<Vp9V4L2CompressedParseResult> ParseVp9CompressedHeaderForV4L2(
    const Vp9FrameHeader& frame_hdr) {
  if (frame_hdr.header_size_in_bytes == 0) {
    Vp9V4L2CompressedParseResult result{};
    result.frame_context = frame_hdr.frame_context;
    return result;
  }
  base::CheckedNumeric<size_t> compressed_end =
      frame_hdr.uncompressed_header_size;
  compressed_end += frame_hdr.header_size_in_bytes;
  size_t compressed_end_size;
  if (!compressed_end.AssignIfValid(&compressed_end_size) ||
      compressed_end_size > frame_hdr.data.size()) {
    LOG(ERROR) << "Invalid compressed header size: compressed_end_size="
               << static_cast<size_t>(compressed_end.ValueOrDefault(0))
               << ", frame_data_size=" << frame_hdr.data.size();
    return std::nullopt;
  }

  Vp9V4L2CompressedParseResult result{};
  Vp9CompressedHeaderParser parser;
  auto compressed_header_data = frame_hdr.data.subspan(
      frame_hdr.uncompressed_header_size, frame_hdr.header_size_in_bytes);
  if (!parser.ParseNoContext(compressed_header_data, frame_hdr, &result)) {
    return std::nullopt;
  }

  return result;
}

}  // namespace media
