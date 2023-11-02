// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuzzer/FuzzedDataProvider.h>

#include <cstdint>
#include <string>

#include "base/containers/span.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_token_builder.h"

// Similar to identifiable_token_builder_fuzzer except uses AddAtomic() instead
// of AddBytes().
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* fuzz_data,
                                      size_t fuzz_data_size) {
  FuzzedDataProvider fdp(fuzz_data, fuzz_data_size);
  auto partition_count = fdp.ConsumeIntegralInRange<size_t>(0, fuzz_data_size);
  blink::IdentifiableTokenBuilder token_builder;
  for (size_t i = 0; i < partition_count; ++i) {
    auto partition = fdp.ConsumeRandomLengthString(fuzz_data_size);
    token_builder.AddAtomic(base::as_bytes(base::make_span(partition)));
  }
  auto remainder = fdp.ConsumeRemainingBytes<uint8_t>();
  token_builder.AddAtomic(base::as_bytes(base::make_span(remainder)));
  return 0;
}
