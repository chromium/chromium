// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/third_party/nspr/prtime.h"

#include <fuzzer/FuzzedDataProvider.h>
#include <stddef.h>
#include <stdint.h>

#include <string>

#include "base/containers/span.h"
#include "testing/libfuzzer/libfuzzer_base_wrappers.h"

PRTime parsed_time;

DEFINE_LLVM_FUZZER_TEST_ONE_INPUT_SPAN(base::span<const uint8_t> bytes) {
  FuzzedDataProvider provider(bytes.data(), bytes.size());
  if (provider.remaining_bytes() == 0) {
    return 0;
  }
  uint8_t selector = provider.ConsumeIntegral<uint8_t>();

  // Using std::string instead of a (potentially faster) fixed buffer to catch
  // accesses beyond the end of the string.
  std::string str = provider.ConsumeRemainingBytesAsString();
  PR_ParseTimeString(str.c_str(), selector & 1, &parsed_time);

  return 0;
}
