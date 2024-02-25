// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/skia_utils_ios.h"

#import <ImageIO/ImageIO.h>
#import <UIKit/UIKit.h>
#include <stddef.h>
#include <stdint.h>

#include "base/apple/scoped_cftyperef.h"
#include "base/ios/ios_util.h"
#include "base/logging.h"
#include "third_party/skia/include/utils/mac/SkCGUtils.h"

namespace {

const uint8_t kICOHeaderMagic[4] = {0x00, 0x00, 0x01, 0x00};

// Returns whether the data encodes an ico image.
bool EncodesIcoImage(NSData* image_data) {
  if (image_data.length < std::size(kICOHeaderMagic))
    return false;
  return memcmp(kICOHeaderMagic, image_data.bytes,
                std::size(kICOHeaderMagic)) == 0;
}

}  // namespace

namespace skia {

SkBitmap CGImageToSkBitmap(CGImageRef image, CGSize size, bool is_opaque) {
  SkBitmap bitmap;
  if (!image)
    return bitmap;

  if (!bitmap.tryAllocN32Pixels(size.width, size.height, is_opaque))
    return bitmap;

  void* data = bitmap.getPixels();

  // Allocate a bitmap context with 4 components per pixel (BGRA). Apple
  // recommends these flags for improved CG performance.
#define HAS_ARGB_SHIFTS(a, r, g, b) \
            (SK_A32_SHIFT == (a) && SK_R32_SHIFT == (r) \
             && SK_G32_SHIFT == (g) && SK_B32_SHIFT == (b))
#if defined(SK_CPU_LENDIAN) && HAS_ARGB_SHIFTS(24, 16, 8, 0)
  base::apple::ScopedCFTypeRef<CGColorSpaceRef> color_space(
      CGColorSpaceCreateDeviceRGB());
  base::apple::ScopedCFTypeRef<CGContextRef> context(CGBitmapContextCreate(
      data, size.width, size.height, 8, size.width * 4, color_space.get(),
      uint32_t{kCGImageAlphaPremultipliedFirst} | kCGBitmapByteOrder32Host));
#else
#error We require that Skia's and CoreGraphics's recommended \
       image memory layout match.
#endif
#undef HAS_ARGB_SHIFTS

  DCHECK(context);
  if (!context)
    return bitmap;

  CGRect imageRect = CGRectMake(0.0, 0.0, size.width, size.height);
  CGContextSetBlendMode(context.get(), kCGBlendModeCopy);
  CGContextDrawImage(context.get(), imageRect, image);

  return bitmap;
}

UIImage* SkBitmapToUIImageWithColorSpace(const SkBitmap& skia_bitmap,
                                         CGFloat scale,
                                         CGColorSpaceRef color_space) {
  if (skia_bitmap.isNull())
    return nil;

  // First convert SkBitmap to CGImageRef.
  base::apple::ScopedCFTypeRef<CGImageRef> cg_image(
      SkCreateCGImageRefWithColorspace(skia_bitmap, color_space));

  // Now convert to UIImage.
  return [UIImage imageWithCGImage:cg_image.get()
                             scale:scale
                       orientation:UIImageOrientationUp];
}

std::vector<SkBitmap> ImageDataToSkBitmaps(NSData* image_data) {
  return ImageDataToSkBitmapsWithMaxSize(image_data, CGFLOAT_MAX);
}

std::vector<SkBitmap> ImageDataToSkBitmapsWithMaxSize(NSData* image_data,
                                                      CGFloat max_size) {
  DCHECK(image_data);

  // On iOS 8.1.1 |CGContextDrawImage| crashes when processing images included
  // in .ico files that are 88x88 pixels or larger (http://crbug.com/435068).
  bool skip_images_88x88_or_larger =
      base::ios::IsRunningOnOrLater(8, 1, 1) && EncodesIcoImage(image_data);

  base::apple::ScopedCFTypeRef<CFDictionaryRef> empty_dictionary(
      CFDictionaryCreate(NULL, NULL, NULL, 0, NULL, NULL));
  std::vector<SkBitmap> frames;

  base::apple::ScopedCFTypeRef<CGImageSourceRef> source(
      CGImageSourceCreateWithData((CFDataRef)image_data,
                                  empty_dictionary.get()));

  size_t count = CGImageSourceGetCount(source.get());
  for (size_t index = 0; index < count; ++index) {
    base::apple::ScopedCFTypeRef<CGImageRef> cg_image(
        CGImageSourceCreateImageAtIndex(source.get(), index,
                                        empty_dictionary.get()));

    CGSize size = CGSizeMake(CGImageGetWidth(cg_image.get()),
                             CGImageGetHeight(cg_image.get()));
    if (size.width > max_size || size.height > max_size)
      continue;
    if (size.width >= 88 && size.height >= 88 && skip_images_88x88_or_larger)
      continue;

    const SkBitmap bitmap = CGImageToSkBitmap(cg_image.get(), size, false);
    if (!bitmap.empty())
      frames.push_back(bitmap);
  }

  DLOG_IF(WARNING, frames.size() != count) << "Only decoded " << frames.size()
      << " frames for " << count << " expected.";
  return frames;
}

UIColor* UIColorFromSkColor(SkColor color) {
  return [UIColor colorWithRed:SkColorGetR(color) / 255.0f
                         green:SkColorGetG(color) / 255.0f
                          blue:SkColorGetB(color) / 255.0f
                         alpha:SkColorGetA(color) / 255.0f];
}

}  // namespace skia
