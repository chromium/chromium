// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/numerics/safe_conversions.h"
#include "base/optional.h"
#include "media/video/h265_parser.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (!size)
    return 0;

  media::H265Parser parser;
  parser.SetStream(data, base::checked_cast<off_t>(size));

  // Parse until the end of stream/unsupported stream/error in stream is
  // found.
  while (true) {
    media::H265NALU nalu;
    media::H265Parser::Result res = parser.AdvanceToNextNALU(&nalu);
    if (res != media::H265Parser::kOk)
      break;

    switch (nalu.nal_unit_type) {
      case media::H265NALU::SPS_NUT:
        int sps_id;
        res = parser.ParseSPS(&sps_id);
        break;
      default:
        // Skip any other NALU.
        break;
    }
    if (res != media::H265Parser::kOk)
      break;
  }

  return 0;
}
