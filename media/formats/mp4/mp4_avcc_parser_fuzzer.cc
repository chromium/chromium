// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "base/containers/span.h"
#include "base/logging.h"
#include "media/formats/mp4/box_definitions.h"
#include "testing/libfuzzer/libfuzzer_base_wrappers.h"

DEFINE_LLVM_FUZZER_TEST_ONE_INPUT_SPAN(const base::span<const uint8_t> data) {
  media::mp4::AVCDecoderConfigurationRecord().Parse(data);
  return 0;
}

// For disabling noisy logging.
struct Environment {
  Environment() { logging::SetMinLogLevel(logging::LOGGING_FATAL); }
};

Environment* env = new Environment();
