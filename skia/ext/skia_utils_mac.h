// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_EXT_SKIA_UTILS_MAC_H_
#define SKIA_EXT_SKIA_UTILS_MAC_H_

#include <ApplicationServices/ApplicationServices.h>

#include "base/apple/scoped_cftyperef.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPixmap.h"

struct SkIRect;
struct SkRect;
class SkMatrix;
using NSSize = CGSize;

#ifdef __OBJC__
@class NSBitmapImageRep;
@class NSImage;
@class NSImageRep;
@class NSColor;
#else
// TODO(crbug.com/40264240): Remove this.
class NSImage;
#endif

namespace skia {

// Matrix converters.
SK_API CGAffineTransform SkMatrixToCGAffineTransform(const SkMatrix& matrix);

// Rectangle converters.
SK_API SkRect CGRectToSkRect(const CGRect& rect);

// Converts a Skia rect to a CoreGraphics CGRect.
CGRect SkIRectToCGRect(const SkIRect& rect);
CGRect SkRectToCGRect(const SkRect& rect);

#ifdef __OBJC__

// Converts NSColor to an SKColor.
// NSColor has a number of methods that return system colors (i.e. controlled by
// user preferences). This function converts the color given by an NSColor class
// method to an SkColor. Official documentation suggests developers only rely on
// +[NSColor selectedTextBackgroundColor] and +[NSColor selectedControlColor],
// but other colors give a good baseline. For many, a gradient is involved; the
// palette chosen based on the enum value given by +[NSColor currentColorTint].
// Apple's documentation also suggests to use NSColorList, but the system color
// list is just populated with class methods on NSColor.
SK_API SkColor NSSystemColorToSkColor(NSColor* color);

#endif  // __OBJC__

// Converts CGColorRef to the ARGB layout Skia expects. The given CGColorRef
// should be in the sRGB color space and include alpha.
SK_API SkColor CGColorRefToSkColor(CGColorRef color);

// Converts a Skia ARGB color to CGColorRef. Assumes sRGB color space.
SK_API base::apple::ScopedCFTypeRef<CGColorRef> CGColorCreateFromSkColor(
    SkColor color);

#ifdef __OBJC__

// Converts NSColor to ARGB. Returns raw rgb values and does no colorspace
// conversion. Only valid for colors in calibrated and device color spaces.
SK_API SkColor NSDeviceColorToSkColor(NSColor* color);

// Converts ARGB in the specified color space to NSColor.
// Prefer sRGB over calibrated colors.
SK_API NSColor* SkColorToCalibratedNSColor(SkColor color);
SK_API NSColor* SkColorToDeviceNSColor(SkColor color);
SK_API NSColor* SkColorToSRGBNSColor(SkColor color);

#endif  // __OBJC__

// Converts a CGImage to a SkBitmap.
SK_API SkBitmap CGImageToSkBitmap(CGImageRef image);

#ifdef __OBJC__

// Draws an NSImage with a given size into a SkBitmap.
SK_API SkBitmap NSImageToSkBitmap(NSImage* image, bool is_opaque);

// Draws an NSImageRep with a given size into a SkBitmap.
SK_API SkBitmap NSImageRepToSkBitmap(NSImageRep* image,
                                     NSSize size,
                                     bool is_opaque);

// Given an SkBitmap, return an autoreleased NSBitmapImageRep.
SK_API NSBitmapImageRep* SkBitmapToNSBitmapImageRep(const SkBitmap& skiaBitmap);

#endif  // __OBJC__

// Given an SkBitmap and a color space, return an autoreleased NSImage.
// TODO(crbug.com/40264240): Restrict this to Objective-C callers.
SK_API NSImage* SkBitmapToNSImage(const SkBitmap& icon);

}  // namespace skia

#endif  // SKIA_EXT_SKIA_UTILS_MAC_H_
