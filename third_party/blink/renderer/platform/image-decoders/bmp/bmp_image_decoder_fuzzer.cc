// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Compile with:
// $ gn gen out/Fuzz '--args=use_libfuzzer=true is_asan=true is_debug=false \
//       is_ubsan_security=true use_remoteexec=true' --check
// $ ninja -C out/Fuzz blink_bmp_image_decoder_fuzzer
//
// Run with:
// $ out/Fuzz/blink_bmp_image_decoder_fuzzer \
//       third_party/blink/web_tests/images/bmp-suite/good/
//
// Alternatively, it can be run with:
// $ out/Fuzz/blink_bmp_image_decoder_fuzzer \
//       ~/another_dir_to_store_corpus \
//       third_party/blink/web_tests/images/bmp-suite/good/
//
// In this case, the fuzzer will read both passed-in directories, but all newly-
// generated testcases will go into ~/another_dir_to_store_corpus.

#include "third_party/blink/renderer/platform/image-decoders/bmp/bmp_image_decoder.h"

#include <fuzzer/FuzzedDataProvider.h>

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/platform/graphics/color_behavior.h"
#include "third_party/blink/renderer/platform/image-decoders/image_decoder.h"
#include "third_party/blink/renderer/platform/image-decoders/image_decoder_fuzzer_utils.h"
#include "third_party/blink/renderer/platform/testing/blink_fuzzer_test_support.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"

namespace blink {

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static blink::BlinkFuzzerTestSupport test_support;
  blink::test::TaskEnvironment task_environment;

  FuzzedDataProvider fdp(data, size);
  FuzzDecoder(DecoderType::kBmpDecoder, fdp);
  return 0;
}
}  // namespace blink
