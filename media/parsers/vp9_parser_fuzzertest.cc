// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_view_util.h"
#include "media/parsers/ivf_parser.h"
#include "media/parsers/vp9_parser.h"
#include "testing/libfuzzer/libfuzzer_base_wrappers.h"

struct Environment {
  Environment() {
    // Disable noisy logging as per "libFuzzer in Chrome" documentation:
    // testing/libfuzzer/getting_started.md#Disable-noisy-error-message-logging.
    logging::SetMinLogLevel(logging::LOGGING_FATAL);
  }
};

Environment* env = new Environment();

// Entry point for LibFuzzer.
DEFINE_LLVM_FUZZER_TEST_ONE_INPUT_SPAN(const base::span<const uint8_t> data) {
  std::string str(base::as_string_view(data));

  media::IvfParser ivf_parser;
  media::IvfFileHeader ivf_file_header;
  media::IvfFrameHeader ivf_frame_header;

  if (!ivf_parser.Initialize(data, &ivf_file_header)) {
    return 0;
  }

  media::Vp9Parser vp9_parser;
  // Parse until the end of stream/unsupported stream/error in stream is found.
  for (auto ivf_bytes = ivf_parser.ParseNextFrame(&ivf_frame_header);
       !ivf_bytes.empty();
       ivf_bytes = ivf_parser.ParseNextFrame(&ivf_frame_header)) {
    media::Vp9FrameHeader vp9_frame_header;
    vp9_parser.SetStream(ivf_bytes, nullptr);
    std::unique_ptr<media::DecryptConfig> null_config;
    gfx::Size allocate_size;
    while (vp9_parser.ParseNextFrame(&vp9_frame_header, &allocate_size,
                                     &null_config) == media::Vp9Parser::kOk) {
      // Repeat until all frames processed.
    }
  }

  return 0;
}
