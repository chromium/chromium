// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/containers/span.h"
#include "base/logging.h"
#include "media/cdm/cenc_utils.h"
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
DEFINE_LLVM_FUZZER_TEST_ONE_INPUT_SPAN(base::span<const uint8_t> data) {
  media::ValidatePsshInput(data);
  return 0;
}
