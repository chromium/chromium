// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <optional>

#include "base/numerics/safe_conversions.h"
#include "media/parsers/h264_parser.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

static volatile size_t volatile_sink;

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (!size)
    return 0;

  media::H264Parser parser;
  parser.SetStream(data, base::checked_cast<off_t>(size));

  // Parse until the end of stream/unsupported stream/error in stream is
  // found.
  while (true) {
    media::H264NALU nalu;
    media::H264Parser::Result res = parser.AdvanceToNextNALU(&nalu);
    if (res != media::H264Parser::kOk)
      break;

    switch (nalu.nal_unit_type) {
      case media::H264NALU::kIDRSlice:
      case media::H264NALU::kNonIDRSlice: {
        media::H264SliceHeader shdr;
        res = parser.ParseSliceHeader(nalu, &shdr);
        break;
      }

      case media::H264NALU::kSPS: {
        int id;
        res = parser.ParseSPS(&id);
        if (res != media::H264Parser::kOk)
          break;
        const media::H264SPS* sps = parser.GetSPS(id);
        if (!sps)
          break;
        // Also test the SPS helper methods. We make sure that the results are
        // used so that the calls are not optimized away.
        std::optional<gfx::Size> coded_size = sps->GetCodedSize();
        volatile_sink = coded_size.value_or(gfx::Size()).ToString().length();
        std::optional<gfx::Rect> visible_rect = sps->GetVisibleRect();
        volatile_sink = visible_rect.value_or(gfx::Rect()).ToString().length();
        break;
      }

      case media::H264NALU::kPPS: {
        int id;
        res = parser.ParsePPS(&id);
        break;
      }

      case media::H264NALU::kSEIMessage: {
        media::H264SEI sei;
        res = parser.ParseSEI(&sei);
        break;
      }

      default:
        // Skip any other NALU.
        break;
    }
    if (res != media::H264Parser::kOk)
      break;
  }

  return 0;
}
