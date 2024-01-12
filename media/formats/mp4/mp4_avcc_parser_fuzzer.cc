// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "base/logging.h"
#include "media/formats/mp4/box_definitions.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  media::mp4::AVCDecoderConfigurationRecord().Parse(data, size);
  return 0;
}

// For disabling noisy logging.
struct Environment {
  Environment() { logging::SetMinLogLevel(logging::LOGGING_FATAL); }
};

Environment* env = new Environment();
