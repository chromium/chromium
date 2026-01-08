// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuzzer/FuzzedDataProvider.h>
#include <stddef.h>
#include <stdint.h>

#include "third_party/blink/renderer/platform/image-decoders/image_decoder_fuzzer_utils.h"
#include "third_party/blink/renderer/platform/testing/blink_fuzzer_test_support.h"

namespace blink {

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static BlinkFuzzerTestSupport test_support;
  FuzzedDataProvider fdp(data, size);
  FuzzDecoder(DecoderType::kJxlDecoder, fdp);
  return 0;
}

}  // namespace blink
