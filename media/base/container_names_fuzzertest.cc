// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/numerics/safe_conversions.h"
#include "media/base/container_names.h"

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  media::container_names::DetermineContainer(data,
                                             base::checked_cast<int>(size));
  return 0;
}
