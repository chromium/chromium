// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/image-decoders/ico/ico_image_decoder.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/containers/span.h"
#include "testing/libfuzzer/libfuzzer_base_wrappers.h"
#include "third_party/blink/renderer/platform/graphics/color_behavior.h"
#include "third_party/blink/renderer/platform/image-decoders/image_decoder.h"
#include "third_party/blink/renderer/platform/testing/blink_fuzzer_test_support.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace blink {

std::unique_ptr<ImageDecoder> CreateICODecoder() {
  // TODO(crbug.com/323934468): Initialize decoder settings dynamically using
  // fuzzer input.
  return std::make_unique<ICOImageDecoder>(
      ImageDecoder::kAlphaPremultiplied, ColorBehavior::kTransformToSRGB,
      ImageDecoder::kNoDecodedImageByteLimit);
}

DEFINE_LLVM_FUZZER_TEST_ONE_INPUT_SPAN(const base::span<const uint8_t> data) {
  static BlinkFuzzerTestSupport test_support;
  test::TaskEnvironment task_environment;

  auto buffer = SharedBuffer::Create(data);
  auto decoder = CreateICODecoder();
  const bool kAllDataReceived = true;
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
