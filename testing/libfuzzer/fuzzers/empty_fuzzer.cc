// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Empty fuzzer that doesn't do anything. Used as test and documentation.

#include <stddef.h>
#include <stdint.h>

// Environment is optional.
struct Environment {
  Environment() {
    // Initialize your environment.
  }
};

// Fuzzer entry point.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  // Initialize environment once.
  static Environment env;
  // Run your code on data.
  return 0;
}
