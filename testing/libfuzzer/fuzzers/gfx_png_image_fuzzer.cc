// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/logging.h"
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
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  ui::SetSupportedResourceScaleFactors({ui::k100Percent});
  // SAFETY: `data` has length `size`, as guaranteed by the fuzzer API.
  gfx::Image image =
      gfx::Image::CreateFrom1xPNGBytes(UNSAFE_BUFFERS(base::span(data, size)));
  if (image.IsEmpty()) {
    return 0;
  }

  image.ToSkBitmap();
  return 0;
}
