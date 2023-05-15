// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "base/logging.h"
#include "ui/base/resource/resource_scale_factor.h"
#include "ui/gfx/image/image.h"

struct Environment {
  Environment() {
    // Disable noisy logging.
    logging::SetMinLogLevel(logging::LOG_FATAL);
  }
};

Environment* env = new Environment();

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  ui::SetSupportedResourceScaleFactors({ui::k100Percent});
  gfx::Image image = gfx::Image::CreateFrom1xPNGBytes(data, size);
  if (image.IsEmpty()) {
    return 0;
  }

  image.ToSkBitmap();
  return 0;
}
