// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "mojo/core/entrypoints.h"
#include "mojo/core/node_controller.h"

// Message deserialization may register handles in the global handle table. We
// need to initialize Core for that to be OK.
struct Environment {
  Environment() { mojo::core::InitializeCore(); }
};

extern "C" int LLVMFuzzerTestOneInput(const unsigned char* data_ptr,
                                      size_t size) {
  // SAFETY: libfuzzer provides a valid pointer and size pair.
  auto data = UNSAFE_BUFFERS(base::span<const uint8_t>(data_ptr, size));

  static Environment environment;

  // Try using the fuzz as the full contents of a port event.
  mojo::core::NodeController::DeserializeRawBytesAsEventForFuzzer(data);
  return 0;
}
