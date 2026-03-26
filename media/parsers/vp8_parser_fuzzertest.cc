// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "base/numerics/safe_conversions.h"
#include "media/parsers/ivf_parser.h"
#include "media/parsers/vp8_parser.h"

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  media::IvfParser ivf_parser;
  media::IvfFileHeader ivf_file_header;
  media::IvfFrameHeader ivf_frame_header;

  if (!ivf_parser.Initialize(data, size, &ivf_file_header))
    return 0;

  // Parse until the end of stream/unsupported stream/error in stream is found.
  for (auto ivf_bytes = ivf_parser.ParseNextFrame(&ivf_frame_header);
       !ivf_bytes.empty();
       ivf_bytes = ivf_parser.ParseNextFrame(&ivf_frame_header)) {
    media::Vp8Parser vp8_parser;
    media::Vp8FrameHeader vp8_frame_header;
    vp8_parser.ParseFrame(ivf_bytes, &vp8_frame_header);
  }

  return 0;
}
