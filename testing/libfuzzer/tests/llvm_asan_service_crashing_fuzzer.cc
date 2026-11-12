// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "base/compiler_specific.h"
#include "base/debug/asan_service.h"

// Crashes on every input, after initializing `base::debug::AsanService` the
// same way MojoLPM and in-process browser fuzzers do. `AsanService` hooks into
// AddressSanitizer's error reporting, and must not stop the fuzzing engine from
// saving the input that caused the crash.

// Triggers a memory fault / crash across all configurations (ASan, UBSan,
// etc.). Uses a volatile pointer so the compiler cannot optimize away the
// memory read.
NOINLINE int TriggerCrash() {
  volatile int* p = nullptr;
  return *p;
}

extern "C" int LLVMFuzzerInitialize(int* argc, char*** argv) {
#if defined(ADDRESS_SANITIZER)
  base::debug::AsanService::GetInstance()->Initialize();
#endif
  return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  return TriggerCrash();
}
