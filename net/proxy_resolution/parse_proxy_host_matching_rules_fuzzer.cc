// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "base/compiler_specific.h"
#include "net/proxy_resolution/proxy_host_matching_rules.h"

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  // Don't waste time parsing if the input is too large
  // (https://crbug.com/813619). According to
  // //testing/libfuzzer/efficient_fuzzer.md setting max_len in the build
  // target is insufficient since AFL doesn't respect it.
  if (size > 512) {
    return 0;
  }

  net::ProxyHostMatchingRules rules;
  std::string input(data, UNSAFE_TODO(data + size));
  rules.ParseFromString(input);

  return 0;
}
