// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stddef.h>
#include <stdint.h>

#include "base/check_op.h"
#include "base/logging.h"
#include "media/parsers/jpeg_parser.h"
#include "media/parsers/parse_jpeg_wrapper.h"

struct Environment {
  Environment() { logging::SetMinLogLevel(logging::LOGGING_FATAL); }
};

Environment* env = new Environment();

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  media::JpegParseResult cpp_result;
  const bool cpp_success =
      media::ParseJpegPictureLegacy(base::span(data, size), &cpp_result);

  media::JpegParseResult rust_result;
  const bool rust_success =
      media::ParseJpegPictureRust(base::span(data, size), &rust_result);

  CHECK_EQ(cpp_success, rust_success)
      << "Implementations disagree on validity!";
  if (cpp_success) {
    CHECK_EQ(cpp_result, rust_result);
  }

  return 0;
}
