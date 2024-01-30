// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/image-decoders/bmp/bmp_image_decoder.h"

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/platform/graphics/color_behavior.h"
#include "third_party/blink/renderer/platform/image-decoders/image_decoder.h"
#include "third_party/blink/renderer/platform/wtf/allocator/partitions.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"

#include <memory>
#include <string_view>
#include <tuple>
#include <vector>

namespace blink {
namespace {

// This is a valid 1x1 BMP which displays a red pixel.
constexpr char kBitmapFile[] =
    "\x42\x4d\x1e\x00\x00\x00\x00\x00\x00\x00\x1a\x00"
    "\x00\x00\x0c\x00\x00\x00\x01\x00\x01\x00\x01\x00"
    "\x18\x00\x00\x00\xff\x00";

std::unique_ptr<ImageDecoder> CreateBMPDecoder() {
  return std::make_unique<BMPImageDecoder>(
      ImageDecoder::kAlphaNotPremultiplied, ColorBehavior::kTransformToSRGB,
      ImageDecoder::kNoDecodedImageByteLimit);
}

void Decode(std::string_view input) {
  scoped_refptr<SharedBuffer> data =
      SharedBuffer::Create(input.data(), input.size());

  std::unique_ptr<ImageDecoder> decoder = CreateBMPDecoder();
  decoder->SetData(data.get(), /*all_data_received=*/true);
  decoder->DecodeFrameBufferAtIndex(0);
}

FUZZ_TEST(BMPImageDecoderFuzzer, Decode)
    .WithSeeds([]() -> std::vector<std::tuple<std::string_view>> {
      WTF::Partitions::Initialize();

      // We need to explicitly pass a size to the std::string_view constructor,
      // because the bitmap data contains zero bytes in the middle; these would
      // terminate the string.
      return {{std::string_view(kBitmapFile, std::size(kBitmapFile))}};
    });

}  // namespace
}  // namespace blink
