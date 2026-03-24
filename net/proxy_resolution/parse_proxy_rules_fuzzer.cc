// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/containers/span.h"
#include "base/strings/string_view_util.h"
#include "net/proxy_resolution/proxy_config.h"
#include "testing/libfuzzer/libfuzzer_base_wrappers.h"

// Entry point for LibFuzzer.
DEFINE_LLVM_FUZZER_TEST_ONE_INPUT_SPAN(base::span<const uint8_t> data) {
  net::ProxyConfig::ProxyRules rules;
  rules.ParseFromString(base::as_string_view(data));
  return 0;
}
