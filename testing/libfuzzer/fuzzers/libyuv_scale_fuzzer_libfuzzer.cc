// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuzzer/FuzzedDataProvider.h>

#include "testing/libfuzzer/fuzzers/libyuv_scale_fuzzer.h"
#include "third_party/libyuv/include/libyuv.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  FuzzedDataProvider provider(data, size);

  // Limit width and height for performance.
  int src_width = provider.ConsumeIntegralInRange<int>(1, 256);
  int src_height = provider.ConsumeIntegralInRange<int>(1, 256);

  int filter_num =
      provider.ConsumeIntegralInRange<int>(0, libyuv::FilterMode::kFilterBox);

  int dst_width = provider.ConsumeIntegralInRange<int>(1, 256);
  int dst_height = provider.ConsumeIntegralInRange<int>(1, 256);

  std::string seed = provider.ConsumeRemainingBytesAsString();

  Scale(true, src_width, src_height, dst_width, dst_height, filter_num, seed);
  Scale(false, src_width, src_height, dst_width, dst_height, filter_num, seed);

  return 0;
}
