// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/internal/verify_name_match.h"

#include <stddef.h>
#include <stdint.h>

#include <fuzzer/FuzzedDataProvider.h>

#include <vector>

#include "net/der/input.h"

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  FuzzedDataProvider fuzzed_data(data, size);

  // Intentionally using uint16_t here to avoid empty |second_part|.
  size_t first_part_size = fuzzed_data.ConsumeIntegral<uint16_t>();
  std::vector<uint8_t> first_part =
      fuzzed_data.ConsumeBytes<uint8_t>(first_part_size);
  std::vector<uint8_t> second_part =
      fuzzed_data.ConsumeRemainingBytes<uint8_t>();

  net::der::Input in1(first_part.data(), first_part.size());
  net::der::Input in2(second_part.data(), second_part.size());
  bool match = net::VerifyNameInSubtree(in1, in2);
  bool reverse_order_match = net::VerifyNameInSubtree(in2, in1);
  // If both InSubtree matches are true, then in1 == in2 (modulo normalization).
  if (match && reverse_order_match)
    CHECK(net::VerifyNameMatch(in1, in2));
  return 0;
}
