// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/containers/span.h"
#include "mojo/core/entrypoints.h"
#include "mojo/core/node_controller.h"
#include "testing/libfuzzer/libfuzzer_base_wrappers.h"

// Message deserialization may register handles in the global handle table. We
// need to initialize Core for that to be OK.
struct Environment {
  Environment() { mojo::core::InitializeCore(); }
};

DEFINE_LLVM_FUZZER_TEST_ONE_INPUT_SPAN(const base::span<const uint8_t> data) {
  static Environment environment;

  // Try using the fuzz as the full contents of a port event.
  mojo::core::NodeController::DeserializeRawBytesAsEventForFuzzer(data);
  return 0;
}
