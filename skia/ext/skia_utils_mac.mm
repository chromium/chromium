// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "skia/ext/skia_utils_mac.h"

#import <AppKit/AppKit.h>
#include <stdint.h>

#include <memory>

#include "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/check.h"
#include "base/mac/mac_util.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/utils/mac/SkCGUtils.h"

namespace {

// Draws an NSImage or an NSImageRep with a given size into a SkBitmap.
SkBitmap NSImageOrNSImageRepToSkBitmap(NSImage* image,
                                       NSImageRep* image_rep,
                                       NSSize size,
                                       bool is_opaque) {
  // Only image or image_rep should be provided, not both.
  DCHECK((image != nullptr) ^ (image_rep != nullptr));

  // Determine the color space for the SkBitmap. Any color space is acceptable,
  // but if we can match the color space of `image` or `image_rep` then the
  // result will be higher fidelity.
  sk_sp<SkColorSpace> sk_color_space;
  {
    NSBitmapImageRep* bitmap_image_rep = nil;
    if (image_rep) {
      // If `image_rep` is an NSBitmapImageRep, then use its color space.
      bitmap_image_rep = base::apple::ObjCCast<NSBitmapImageRep>(image_rep);
    } else {
      // If `image` has an NSBitmapImageRep, then use the color space of the
      // first encountered NSBitmapImageRep.
      for (NSImageRep* rep in [image representations]) {
        bitmap_image_rep = base::apple::ObjCCast<NSBitmapImageRep>(rep);
        if (bitmap_image_rep) {
          break;
        }
      }
    }
    sk_color_space = SkMakeColorSpaceFromCGColorSpace(
        [[bitmap_image_rep colorSpace] CGColorSpace]);

    // If we did not extract a color space that matches the input, default to
    // using sRGB.
    if (!sk_color_space) {
      sk_color_space = SkColorSpace::MakeSRGB();
    }
  }

  // Set the CGColorSpace of the CGContext to match the SkColorSpace.
  base::apple::ScopedCFTypeRef<CGColorSpaceRef> cg_color_space(
      SkCreateCGColorSpace(sk_color_space.get()));

  SkImageInfo info = SkImageInfo::MakeN32(
      size.width, size.height,
      is_opaque ? kOpaque_SkAlphaType : kPremul_SkAlphaType, sk_color_space);
  SkBitmap bitmap;
  if (!bitmap.tryAllocPixels(info)) {
    return bitmap;  // Return |bitmap| which should respond true to isNull().
  }

  void* data = bitmap.getPixels();

  // Allocate a bitmap context with 4 components per pixel (BGRA). Apple
  // recommends these flags for improved CG performance.
#define HAS_ARGB_SHIFTS(a, r, g, b) \
            (SK_A32_SHIFT == (a) && SK_R32_SHIFT == (r) \
             && SK_G32_SHIFT == (g) && SK_B32_SHIFT == (b))
#if defined(SK_CPU_LENDIAN) && HAS_ARGB_SHIFTS(24, 16, 8, 0)
  base::apple::ScopedCFTypeRef<CGContextRef> context(CGBitmapContextCreate(
      data, size.width, size.height, 8, size.width * 4, cg_color_space.get(),
      uint32_t{kCGImageAlphaPremultipliedFirst} | kCGBitmapByteOrder32Host));
#else
#error We require that Skia's and CoreGraphics's recommended \
       image memory layout match.
#endif
#undef HAS_ARGB_SHIFTS

  // Something went really wrong. Best guess is that the bitmap data is invalid.
  DCHECK(context);

  [NSGraphicsContext saveGraphicsState];

  NSGraphicsContext* context_cocoa =
      [NSGraphicsContext graphicsContextWithCGContext:context.get() flipped:NO];
  [NSGraphicsContext setCurrentContext:context_cocoa];

  NSRect drawRect = NSMakeRect(0, 0, size.width, size.height);
  if (image) {
    [image drawInRect:drawRect
             fromRect:NSZeroRect
            operation:NSCompositingOperationCopy
             fraction:1.0];
  } else {
    [image_rep drawInRect:drawRect
                 fromRect:NSZeroRect
                operation:NSCompositingOperationCopy
                 fraction:1.0
           respectFlipped:NO
                    hints:nil];
  }

  [NSGraphicsContext restoreGraphicsState];

  return bitmap;
}

} // namespace

namespace skia {

CGAffineTransform SkMatrixToCGAffineTransform(const SkMatrix& matrix) {
  // CGAffineTransforms don't support perspective transforms, so make sure
  // we don't get those.
  DCHECK(matrix[SkMatrix::kMPersp0] == 0.0f);
  DCHECK(matrix[SkMatrix::kMPersp1] == 0.0f);
  DCHECK(matrix[SkMatrix::kMPersp2] == 1.0f);

  return CGAffineTransformMake(matrix[SkMatrix::kMScaleX],
                               matrix[SkMatrix::kMSkewY],
                               matrix[SkMatrix::kMSkewX],
                               matrix[SkMatrix::kMScaleY],
                               matrix[SkMatrix::kMTransX],
                               matrix[SkMatrix::kMTransY]);
}

SkRect CGRectToSkRect(const CGRect& rect) {
  SkRect sk_rect = {
    rect.origin.x, rect.origin.y, CGRectGetMaxX(rect), CGRectGetMaxY(rect)
  };
  return sk_rect;
}

CGRect SkIRectToCGRect(const SkIRect& rect) {
  CGRect cg_rect = {
    { rect.fLeft, rect.fTop },
    { rect.fRight - rect.fLeft, rect.fBottom - rect.fTop }
  };
  return cg_rect;
}

CGRect SkRectToCGRect(const SkRect& rect) {
  CGRect cg_rect = {
    { rect.fLeft, rect.fTop },
    { rect.fRight - rect.fLeft, rect.fBottom - rect.fTop }
  };
  return cg_rect;
}

SkColor NSSystemColorToSkColor(NSColor* color) {
  // System colors use the an NSNamedColorSpace called "System", so first step
  // is to convert the color into something that can be worked with.
  NSColor* device_color =
      [color colorUsingColorSpace:NSColorSpace.deviceRGBColorSpace];
  if (device_color)
    return NSDeviceColorToSkColor(device_color);

  // Sometimes the conversion is not possible, but we can get an approximation
  // by going through a CGColorRef. Note that simply using NSColor methods for
  // accessing components for system colors results in exceptions like
  // "-numberOfComponents not valid for the NSColor NSNamedColorSpace System
  // windowBackgroundColor; need to first convert colorspace." Hence the
  // conversion first to CGColor.
  CGColorRef cg_color = color.CGColor;
  const size_t component_count = CGColorGetNumberOfComponents(cg_color);
  if (component_count == 4)
    return CGColorRefToSkColor(cg_color);

  CHECK(component_count == 1 || component_count == 2);
  // 1-2 components means a grayscale channel and maybe an alpha channel, which
  // CGColorRefToSkColor will not like. But RGB is additive, so the conversion
  // is easy (RGB to grayscale is less easy).
  const CGFloat* components = CGColorGetComponents(cg_color);
  CGFloat alpha = component_count == 2 ? components[1] : 1.0f;
  return SkColor4f{components[0], components[0], components[0], alpha}
      .toSkColor();
}

SkColor CGColorRefToSkColor(CGColorRef color) {
  base::apple::ScopedCFTypeRef<CGColorRef> cg_color(
      CGColorCreateCopyByMatchingToColorSpace(base::mac::GetSRGBColorSpace(),
                                              kCGRenderingIntentDefault, color,
                                              nullptr));
  DCHECK(CGColorGetNumberOfComponents(color) == 4);
  const CGFloat* components = CGColorGetComponents(cg_color.get());
  return SkColor4f{components[0], components[1], components[2], components[3]}
      .toSkColor();
}

base::apple::ScopedCFTypeRef<CGColorRef> CGColorCreateFromSkColor(
    SkColor color) {
  CGFloat components[] = {
      SkColorGetR(color) / 255.0f, SkColorGetG(color) / 255.0f,
      SkColorGetB(color) / 255.0f, SkColorGetA(color) / 255.0f};
  return base::apple::ScopedCFTypeRef<CGColorRef>(
      CGColorCreate(base::mac::GetSRGBColorSpace(), components));
}

// Converts NSColor to ARGB
SkColor NSDeviceColorToSkColor(NSColor* color) {
  DCHECK(color.colorSpace == NSColorSpace.genericRGBColorSpace ||
         color.colorSpace == NSColorSpace.deviceRGBColorSpace);
  CGFloat red, green, blue, alpha;
  color = [color colorUsingColorSpace:NSColorSpace.deviceRGBColorSpace];
  [color getRed:&red green:&green blue:&blue alpha:&alpha];
  return SkColor4f{red, green, blue, alpha}.toSkColor();
}

// Converts ARGB to NSColor.
NSColor* SkColorToCalibratedNSColor(SkColor color) {
  return [NSColor colorWithCalibratedRed:SkColorGetR(color) / 255.0f
                                   green:SkColorGetG(color) / 255.0f
                                    blue:SkColorGetB(color) / 255.0f
                                   alpha:SkColorGetA(color) / 255.0f];
}

NSColor* SkColorToDeviceNSColor(SkColor color) {
  return [NSColor colorWithDeviceRed:SkColorGetR(color) / 255.0f
                               green:SkColorGetG(color) / 255.0f
                                blue:SkColorGetB(color) / 255.0f
                               alpha:SkColorGetA(color) / 255.0f];
}

NSColor* SkColorToSRGBNSColor(SkColor color) {
  const CGFloat components[] = {
      SkColorGetR(color) / 255.0f, SkColorGetG(color) / 255.0f,
      SkColorGetB(color) / 255.0f, SkColorGetA(color) / 255.0f};
  return [NSColor colorWithColorSpace:NSColorSpace.sRGBColorSpace
                           components:components
                                count:4];
}

SkBitmap CGImageToSkBitmap(CGImageRef image) {
  SkBitmap bitmap;
  if (image && SkCreateBitmapFromCGImage(&bitmap, image))
    return bitmap;
  return SkBitmap();
}

SkBitmap NSImageToSkBitmap(NSImage* image, bool is_opaque) {
  return NSImageOrNSImageRepToSkBitmap(image, /*image_rep=*/nil, image.size,
                                       is_opaque);
}

SkBitmap NSImageRepToSkBitmap(NSImageRep* image_rep,
                              NSSize size,
                              bool is_opaque) {
  return NSImageOrNSImageRepToSkBitmap(
      /*image=*/nil, image_rep, size, is_opaque);
}

NSBitmapImageRep* SkBitmapToNSBitmapImageRep(const SkBitmap& skiaBitmap) {
  // First convert SkBitmap to CGImageRef.
  base::apple::ScopedCFTypeRef<CGImageRef> cgimage(
      SkCreateCGImageRef(skiaBitmap));
  if (!cgimage)
    return nil;

  // Now convert to NSBitmapImageRep.
  return [[NSBitmapImageRep alloc] initWithCGImage:cgimage.get()];
}

NSImage* SkBitmapToNSImage(const SkBitmap& skiaBitmap) {
  if (skiaBitmap.isNull())
    return nil;

  NSImage* image = [[NSImage alloc] init];
  NSBitmapImageRep* imageRep = SkBitmapToNSBitmapImageRep(skiaBitmap);
  if (!imageRep)
    return nil;
  [image addRepresentation:imageRep];
  image.size = NSMakeSize(skiaBitmap.width(), skiaBitmap.height());
  return image;
}

}  // namespace skia
