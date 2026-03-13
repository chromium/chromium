// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "media/parsers/jpeg_parser.h"
#include "media/parsers/parse_jpeg_wrapper.h"
#include "testing/libfuzzer/libfuzzer_base_wrappers.h"

struct Environment {
  Environment() { logging::SetMinLogLevel(logging::LOGGING_FATAL); }
};

Environment* env = new Environment();

// Entry point for LibFuzzer.
DEFINE_LLVM_FUZZER_TEST_ONE_INPUT_SPAN(const base::span<const uint8_t> data) {
  media::JpegParseResult cpp_result;
  const bool cpp_success = media::ParseJpegPictureLegacy(data, &cpp_result);

  media::JpegParseResult rust_result;
  const bool rust_success = media::ParseJpegPictureRust(data, &rust_result);

  CHECK_EQ(cpp_success, rust_success)
      << "Implementations disagree on validity!";
  if (cpp_success) {
    CHECK_EQ(cpp_result, rust_result);
  }

  return 0;
}
