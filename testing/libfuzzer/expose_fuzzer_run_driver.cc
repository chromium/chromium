// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/libFuzzer/src/FuzzerDefs.h"

// Small shim to expose LLVMFuzzerRunDriver from libfuzzer.
// This is exposed as a visible symbol in the libfuzzer-API-compatible
// fuzzers framework centipede, but for some reason is not exposed in libfuzzer
// itself. Expose it so we can write a fuzzer the same way for each.

extern "C" __attribute__((visibility("default"))) int LLVMFuzzerRunDriver(
    int* argc,
    char*** argv,
    int (*UserCb)(const uint8_t* Data, size_t Size)) {
  return fuzzer::FuzzerDriver(argc, argv, UserCb);
}
