// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/uri_template/uri_template.h"

#include <fuzzer/FuzzedDataProvider.h>

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  FuzzedDataProvider fuzzed_data(data, size);
  std::string uri_template = fuzzed_data.ConsumeRandomLengthString(256);
  // Construct a map containing variable names and corresponding values.
  std::unordered_map<std::string, std::string> parameters;
  uint8_t num_vars(fuzzed_data.ConsumeIntegral<uint8_t>());
  for (uint8_t i = 0; i < num_vars; i++) {
    parameters.emplace(fuzzed_data.ConsumeRandomLengthString(10),
                       fuzzed_data.ConsumeRandomLengthString(10));
  }
  std::string target;
  uri_template::Expand(uri_template, parameters, &target);
  return 0;
}
