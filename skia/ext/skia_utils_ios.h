// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_EXT_SKIA_UTILS_IOS_H_
#define SKIA_EXT_SKIA_UTILS_IOS_H_

#include <CoreGraphics/CoreGraphics.h>
#include <vector>

#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"

#ifdef __OBJC__
@class UIColor;
@class UIImage;
@class NSData;
#else
// TODO(crbug.com/40264240): Remove this.
class UIImage;
#endif

namespace skia {

// Draws a CGImage into an SkBitmap of the given size.
SK_API SkBitmap CGImageToSkBitmap(CGImageRef image,
                                  CGSize size,
                                  bool is_opaque);

// Given an SkBitmap and a color space, return an autoreleased UIImage.
// TODO(crbug.com/40264240): Restrict this to Objective-C callers.
SK_API UIImage* SkBitmapToUIImageWithColorSpace(const SkBitmap& skia_bitmap,
                                                CGFloat scale,
                                                CGColorSpaceRef color_space);

#ifdef __OBJC__

// Decodes all image representations inside the data into a vector of SkBitmaps.
// Returns a vector of all the successfully decoded representations or an empty
// vector if none can be decoded.
SK_API std::vector<SkBitmap> ImageDataToSkBitmaps(NSData* image_data);

// Decodes all image representations inside the data into a vector of SkBitmaps.
// If a representation is bigger than max_size (either width or height), it is
// ignored.
// Returns a vector of all the successfully decoded representations or an empty
// vector if none can be decoded.
SK_API std::vector<SkBitmap> ImageDataToSkBitmapsWithMaxSize(NSData* image_data,
                                                             CGFloat max_size);

// Returns a UIColor for an SKColor.
SK_API UIColor* UIColorFromSkColor(SkColor color);

#endif  // __OBJC__

}  // namespace skia

#endif  // SKIA_EXT_SKIA_UTILS_IOS_H_
