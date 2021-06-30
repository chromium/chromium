// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/containers/span.h"
#include "base/no_destructor.h"
#include "mojo/core/entrypoints.h"
#include "mojo/core/node_controller.h"

// Message deserialization may register handles in the global handle table. We
// need to initialize Core for that to be OK.
struct Environment {
  Environment() { mojo::core::InitializeCore(); }
};

extern "C" int LLVMFuzzerTestOneInput(const unsigned char* data, size_t size) {
  static base::NoDestructor<Environment> environment;

  // Try using the fuzz as the full contents of a port event.
  mojo::core::NodeController::DeserializeRawBytesAsEventForFuzzer(
      base::make_span(data, size));
  return 0;
}
