// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Compile with:
// gn gen out/Fuzz '--args=use_libfuzzer=true is_asan=true
// is_debug=false is_ubsan_security=true' --check
// ninja -C out/Fuzz blink_png_decoder_fuzzer
//
// Run with:
// ./out/Fuzz/blink_png_decoder_fuzzer
// third_party/blink/web_tests/images/resources/pngfuzz
//
// Alternatively, it can be run with:
// ./out/Fuzz/blink_png_decoder_fuzzer ~/another_dir_to_store_corpus
// third_party/blink/web_tests/images/resources/pngfuzz
//
// so the fuzzer will read both directories passed, but all new generated
// testcases will go into ~/another_dir_to_store_corpus

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "third_party/blink/renderer/platform/graphics/color_behavior.h"
#include "third_party/blink/renderer/platform/image-decoders/image_decoder.h"
#include "third_party/blink/renderer/platform/image-decoders/png/png_image_decoder.h"
#include "third_party/blink/renderer/platform/testing/blink_fuzzer_test_support.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace blink {

std::unique_ptr<ImageDecoder> CreatePNGDecoder() {
  // TODO(crbug.com/323934468): Initialize decoder settings dynamically using
  // fuzzer input.
  return std::make_unique<PNGImageDecoder>(
      ImageDecoder::kAlphaPremultiplied, ImageDecoder::kDefaultBitDepth,
      ColorBehavior::kTransformToSRGB, ImageDecoder::kNoDecodedImageByteLimit);
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static BlinkFuzzerTestSupport test_support;
  auto buffer = SharedBuffer::Create(data, size);
  auto decoder = CreatePNGDecoder();
  static constexpr bool kAllDataReceived = true;
  decoder->SetData(buffer.get(), kAllDataReceived);
  for (wtf_size_t frame = 0; frame < decoder->FrameCount(); ++frame) {
    decoder->DecodeFrameBufferAtIndex(frame);
    if (decoder->Failed()) {
      return 0;
    }
  }
  return 0;
}

}  // namespace blink
