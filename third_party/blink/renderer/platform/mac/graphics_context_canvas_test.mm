// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/mac/graphics_context_canvas.h"

#include <array>

#include "base/containers/span.h"
#include "skia/ext/skia_utils_mac.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_canvas.h"

namespace blink {

using ::testing::ElementsAreArray;

enum TestType {
  kTestIdentity = 0,
  kTestTranslate = 1,
  kTestClip = 2,
  kTestXClip = kTestTranslate | kTestClip,
};

void RunTest(TestType test) {
  const unsigned kWidth = 2;
  const unsigned kHeight = 2;
  const unsigned kStorageSize = kWidth * kHeight;
  std::array<unsigned, kStorageSize> bits = {
      0xFF333333,
      0xFF666666,
      0xFF999999,
      0xFFCCCCCC,
  };

  SkImageInfo info = SkImageInfo::MakeN32Premul(kWidth, kHeight);
  SkBitmap bitmap;
  bitmap.installPixels(info, bits.data(), info.minRowBytes());

  SkiaPaintCanvas canvas(bitmap);
  if (test & kTestTranslate)
    canvas.translate(kWidth / 2, 0);
  if (test & kTestClip) {
    SkRect clip_rect = {0, kHeight / 2, kWidth, kHeight};
    canvas.clipRect(clip_rect);
  }
  {
    SkIRect clip = SkIRect::MakeWH(kWidth, kHeight);
    GraphicsContextCanvas bit_locker(&canvas, clip);
    CGContextRef cg_context = bit_locker.CgContext();
    CGColorRef test_color = CGColorGetConstantColor(kCGColorWhite);
    CGContextSetFillColorWithColor(cg_context, test_color);
    CGRect cg_rect = {{0, 0}, {kWidth, kHeight}};
    CGContextFillRect(cg_context, cg_rect);
  }
  const auto kResults = std::to_array<std::array<unsigned, kStorageSize>>({
      {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF},  // identity
      {0xFF333333, 0xFFFFFFFF, 0xFF999999, 0xFFFFFFFF},  // translate
      {0xFF333333, 0xFF666666, 0xFFFFFFFF, 0xFFFFFFFF},  // clip
      {0xFF333333, 0xFF666666, 0xFF999999, 0xFFFFFFFF},  // translate | clip
  });
  EXPECT_THAT(bits, ElementsAreArray(kResults[test]));
}

TEST(GraphicsContextCanvasTest, Identity) {
  RunTest(kTestIdentity);
}

TEST(GraphicsContextCanvasTest, Translate) {
  RunTest(kTestTranslate);
}

TEST(GraphicsContextCanvasTest, Clip) {
  RunTest(kTestClip);
}

TEST(GraphicsContextCanvasTest, XClip) {
  RunTest(kTestXClip);
}

}  // namespace
