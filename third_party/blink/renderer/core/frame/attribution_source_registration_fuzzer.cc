// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "third_party/blink/public/mojom/conversions/attribution_data_host.mojom-blink.h"
#include "third_party/blink/renderer/core/frame/attribution_response_parsing.h"
#include "third_party/blink/renderer/platform/testing/blink_fuzzer_test_support.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static BlinkFuzzerTestSupport test_support = BlinkFuzzerTestSupport();

  const AtomicString input(data, static_cast<unsigned>(size));
  mojom::blink::AttributionSourceData output;
  attribution_response_parsing::ParseSourceRegistrationHeader(input, output);

  return 0;
}

}  // namespace blink

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  return blink::LLVMFuzzerTestOneInput(data, size);
}
