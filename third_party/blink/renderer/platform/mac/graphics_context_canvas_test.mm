// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/mac/graphics_context_canvas.h"

#include "skia/ext/skia_utils_mac.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_canvas.h"

namespace blink {

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
  const unsigned kOriginal[] = {0xFF333333, 0xFF666666, 0xFF999999, 0xFFCCCCCC};
  EXPECT_EQ(kStorageSize, sizeof(kOriginal) / sizeof(kOriginal[0]));
  unsigned bits[kStorageSize];
  memcpy(bits, kOriginal, sizeof(kOriginal));
  SkImageInfo info = SkImageInfo::MakeN32Premul(kWidth, kHeight);
  SkBitmap bitmap;
  bitmap.installPixels(info, bits, info.minRowBytes());

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
  const unsigned kResults[][kStorageSize] = {
      {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF},  // identity
      {0xFF333333, 0xFFFFFFFF, 0xFF999999, 0xFFFFFFFF},  // translate
      {0xFF333333, 0xFF666666, 0xFFFFFFFF, 0xFFFFFFFF},  // clip
      {0xFF333333, 0xFF666666, 0xFF999999, 0xFFFFFFFF}   // translate | clip
  };
  for (unsigned index = 0; index < kStorageSize; index++)
    EXPECT_EQ(kResults[test][index], bits[index]) << "Index: " << index;
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
