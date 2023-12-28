// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/skia_utils_mac.h"

#import <AppKit/AppKit.h>

#include "base/apple/foundation_util.h"
#include "base/mac/mac_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "ui/gfx/scoped_ns_graphics_context_save_gstate_mac.h"

namespace {

class SkiaUtilsMacTest : public testing::Test {
 public:
  enum class TestColor {
    kRed,
    kBlue,
  };

  enum class ColorType {
    k24Bit,  // kN32_SkColorType
    k16Bit,  // kARGB_4444_SkColorType
  };

  // Creates a test bitmap of the specified color and color type.
  SkBitmap CreateSkBitmap(int width,
                          int height,
                          TestColor test_color,
                          ColorType color_type);

  // Creates a red image.
  NSImage* CreateNSImage(int width, int height);

  // Checks that the given bitmap rep is actually the correct color.
  void TestImageRep(NSBitmapImageRep* image_rep, TestColor test_color);

  // Checks that the given bitmap is red.
  void TestSkBitmap(const SkBitmap& bitmap);

  // Tests `SkBitmapToNSImage` for a specific combination of color and color
  // type. Creates a bitmap with `CreateSkBitmap`, converts it into an
  // `NSImage`, then tests it with `TestImageRep`.
  void ShapeHelper(int width,
                   int height,
                   TestColor test_color,
                   ColorType color_type);
};

SkBitmap SkiaUtilsMacTest::CreateSkBitmap(int width,
                                          int height,
                                          TestColor test_color,
                                          ColorType color_type) {
  SkColorType sk_color_type = color_type == ColorType::k24Bit
                                  ? kN32_SkColorType
                                  : kARGB_4444_SkColorType;
  SkImageInfo info =
      SkImageInfo::Make(width, height, sk_color_type, kPremul_SkAlphaType,
                        SkColorSpace::MakeSRGB());

  SkBitmap bitmap;
  bitmap.allocPixels(info);

  if (test_color == TestColor::kRed)
    bitmap.eraseARGB(0xff, 0xff, 0, 0);
  else
    bitmap.eraseARGB(0xff, 0, 0, 0xff);

  return bitmap;
}

NSImage* SkiaUtilsMacTest::CreateNSImage(int width, int height) {
  // An `NSBitmapImageRep` can only be created with a handful of named color
  // spaces, and sRGB isn't one. Do a retagging after creation to switch it.
  NSBitmapImageRep* initial_bitmap = [[NSBitmapImageRep alloc]
      initWithBitmapDataPlanes:nil
                    pixelsWide:width
                    pixelsHigh:height
                 bitsPerSample:8
               samplesPerPixel:4
                      hasAlpha:YES
                      isPlanar:NO
                colorSpaceName:NSCalibratedRGBColorSpace
                  bitmapFormat:0
                   bytesPerRow:4 * width
                  bitsPerPixel:32];
  NSBitmapImageRep* bitmap = [initial_bitmap
      bitmapImageRepByRetaggingWithColorSpace:NSColorSpace.sRGBColorSpace];

  {
    gfx::ScopedNSGraphicsContextSaveGState scopedGState;
    NSGraphicsContext.currentContext =
        [NSGraphicsContext graphicsContextWithBitmapImageRep:bitmap];

    CGFloat comps[] = {1.0, 0.0, 0.0, 1.0};
    NSColor* color = [NSColor colorWithColorSpace:NSColorSpace.sRGBColorSpace
                                       components:comps
                                            count:4];
    [color set];
    NSRectFill(NSMakeRect(0, 0, width, height));
  }

  NSImage* image = [[NSImage alloc] initWithSize:NSMakeSize(width, height)];
  [image addRepresentation:bitmap];

  return image;
}

void SkiaUtilsMacTest::TestImageRep(NSBitmapImageRep* image_rep,
                                    TestColor test_color) {
  // Get the color of a pixel and make sure it looks fine.
  int x = image_rep.size.width > 17 ? 17 : 0;
  int y = image_rep.size.height > 17 ? 17 : 0;
  NSColor* color = [image_rep colorAtX:x y:y];

  ASSERT_EQ(4, color.numberOfComponents);
  CGFloat color_components[4];
  [color getComponents:color_components];
  const CGFloat& red = color_components[0];
  const CGFloat& green = color_components[1];
  const CGFloat& blue = color_components[2];
  const CGFloat& alpha = color_components[3];

  // Be a little tolerant of floating point rounding, maybe, but everything is
  // done in SRGB so there should be no color space conversion affecting things.
  if (test_color == TestColor::kRed) {
    EXPECT_GT(red, 0.9995);
    EXPECT_LT(blue, 0.0005);
  } else {
    EXPECT_LT(red, 0.0005);
    EXPECT_GT(blue, 0.9995);
  }
  EXPECT_LT(green, 0.0005);
  EXPECT_GT(alpha, 0.9995);
}

void SkiaUtilsMacTest::TestSkBitmap(const SkBitmap& bitmap) {
  int x = bitmap.width() > 17 ? 17 : 0;
  int y = bitmap.height() > 17 ? 17 : 0;
  SkColor color = bitmap.getColor(x, y);

  EXPECT_EQ(255u, SkColorGetR(color));
  EXPECT_EQ(0u, SkColorGetB(color));
  EXPECT_EQ(0u, SkColorGetG(color));
  EXPECT_EQ(255u, SkColorGetA(color));
}

void SkiaUtilsMacTest::ShapeHelper(int width,
                                   int height,
                                   TestColor test_color,
                                   ColorType color_type) {
  SkBitmap bitmap(CreateSkBitmap(width, height, test_color, color_type));

  // Confirm size
  NSImage* image = skia::SkBitmapToNSImage(bitmap);
  EXPECT_DOUBLE_EQ(image.size.width, (CGFloat)width);
  EXPECT_DOUBLE_EQ(image.size.height, (CGFloat)height);

  EXPECT_TRUE(image.representations.count == 1);
  EXPECT_TRUE([image.representations.lastObject
      isKindOfClass:[NSBitmapImageRep class]]);
  TestImageRep(base::apple::ObjCCastStrict<NSBitmapImageRep>(
                   image.representations.lastObject),
               test_color);
}

TEST_F(SkiaUtilsMacTest, BitmapToNSImage_RedSquare64x64) {
  ShapeHelper(64, 64, TestColor::kRed, ColorType::k24Bit);
}

TEST_F(SkiaUtilsMacTest, BitmapToNSImage_BlueRectangle199x19) {
  ShapeHelper(199, 19, TestColor::kBlue, ColorType::k24Bit);
}

TEST_F(SkiaUtilsMacTest, BitmapToNSImage_BlueRectangle444) {
  ShapeHelper(200, 200, TestColor::kBlue, ColorType::k16Bit);
}

TEST_F(SkiaUtilsMacTest, BitmapToNSBitmapImageRep_BlueRectangle20x30) {
  int width = 20;
  int height = 30;

  SkBitmap bitmap(
      CreateSkBitmap(width, height, TestColor::kBlue, ColorType::k24Bit));
  NSBitmapImageRep* imageRep = skia::SkBitmapToNSBitmapImageRep(bitmap);

  EXPECT_DOUBLE_EQ(width, imageRep.size.width);
  EXPECT_DOUBLE_EQ(height, imageRep.size.height);
  TestImageRep(imageRep, TestColor::kBlue);
}

TEST_F(SkiaUtilsMacTest, NSImageRepToSkBitmap) {
  int width = 10;
  int height = 15;

  NSImage* image = CreateNSImage(width, height);
  EXPECT_EQ(1u, image.representations.count);
  NSBitmapImageRep* imageRep = base::apple::ObjCCastStrict<NSBitmapImageRep>(
      image.representations.lastObject);
  SkBitmap bitmap(skia::NSImageRepToSkBitmap(imageRep, image.size, false));
  TestSkBitmap(bitmap);
}

}  // namespace
