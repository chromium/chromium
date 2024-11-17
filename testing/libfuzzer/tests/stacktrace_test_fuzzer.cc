// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/compiler_specific.h"
#include "base/containers/span.h"

// Tries to use a dangling pointer, triggers a UaF crash under ASAN.
NOINLINE int TriggerUAF() {
  auto ptr = std::make_unique<int>(0);
  int* dangling = ptr.get();
  ptr = nullptr;
  return *dangling;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  // SAFETY: libFuzzer and compatible fuzzing engines pass valid data.
  auto bytes = UNSAFE_BUFFERS(base::make_span(data, size));
  auto str = base::as_string_view(bytes);

  if (str == "uaf") {
    return TriggerUAF();
  }
  return 0;
}
