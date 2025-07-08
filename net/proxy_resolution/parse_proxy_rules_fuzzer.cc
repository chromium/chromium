// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "base/compiler_specific.h"
#include "net/proxy_resolution/proxy_config.h"

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  net::ProxyConfig::ProxyRules rules;
  std::string input(data, UNSAFE_TODO(data + size));
  rules.ParseFromString(input);
  return 0;
}
