// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO (scroggo): Move this to
// third_party/blink/renderer/platform/image-decoders ?

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
//
// For more details, see
// https://chromium.googlesource.com/chromium/src/+/main/testing/libfuzzer/README.md

#include <memory>

#include "third_party/blink/renderer/platform/image-decoders/png/png_image_decoder.h"
#include "third_party/blink/renderer/platform/testing/blink_fuzzer_test_support.h"

namespace blink {

std::unique_ptr<ImageDecoder> CreateDecoder(
    ImageDecoder::AlphaOption alpha_option) {
  return std::make_unique<PNGImageDecoder>(
      alpha_option, ImageDecoder::kDefaultBitDepth,
      ColorBehavior::TransformToSRGB(), ImageDecoder::kNoDecodedImageByteLimit);
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static BlinkFuzzerTestSupport test_support = BlinkFuzzerTestSupport();
  auto buffer = SharedBuffer::Create(data, size);
  // TODO (scroggo): Also test ImageDecoder::AlphaNotPremultiplied?
  auto decoder = CreateDecoder(ImageDecoder::kAlphaPremultiplied);
  const bool kAllDataReceived = true;
  decoder->SetData(buffer.get(), kAllDataReceived);
  decoder->FrameCount();
  if (decoder->Failed())
    return 0;
  for (size_t frame = 0; frame < decoder->FrameCount(); frame++) {
    decoder->DecodeFrameBufferAtIndex(frame);
    if (decoder->Failed())
      return 0;
  }
  return 0;
}

}  // namespace blink

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  return blink::LLVMFuzzerTestOneInput(data, size);
}
