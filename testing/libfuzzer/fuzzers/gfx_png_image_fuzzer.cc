// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/containers/span.h"
#include "base/logging.h"
#include "testing/libfuzzer/libfuzzer_base_wrappers.h"
#include "ui/base/resource/resource_scale_factor.h"
#include "ui/gfx/image/image.h"

struct Environment {
  Environment() {
    // Disable noisy logging.
    logging::SetMinLogLevel(logging::LOGGING_FATAL);
  }
};

Environment* env = new Environment();

// Entry point for LibFuzzer.
DEFINE_LLVM_FUZZER_TEST_ONE_INPUT_SPAN(base::span<const uint8_t> data) {
  ui::SetSupportedResourceScaleFactors({ui::k100Percent});
  gfx::Image image = gfx::Image::CreateFrom1xPNGBytes(data);
  if (image.IsEmpty()) {
    return 0;
  }

  image.ToSkBitmap();
  return 0;
}
