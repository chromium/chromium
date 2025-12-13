// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/util/image/image_util.h"

#import <UIKit/UIKit.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#import "base/apple/foundation_util.h"
#import "ui/gfx/color_analysis.h"
#import "ui/gfx/image/image.h"

namespace {

NSString* const kImageExtensionJPG = @"jpg";
NSString* const kImageExtensionPNG = @"png";
NSString* const kImageExtensionTIF = @"tif";
NSString* const kImageExtensionBMP = @"bmp";
NSString* const kImageExtensionGIF = @"gif";
NSString* const kImageExtensionICO = @"ico";
NSString* const kImageExtensionWebP = @"webp";

// Returns whether `span` starts with `prefix`.
template <typename T, size_t E>
bool starts_with(base::span<T> span, base::span<T, E> prefix) {
  const auto head = span.first(E);
  return head == prefix;
}

}  // namespace

UIColor* DominantColorForImage(const gfx::Image& image, CGFloat opacity) {
  SkColor color = color_utils::CalculateKMeanColorOfBitmap(*image.ToSkBitmap());
  UIColor* result = [UIColor colorWithRed:SkColorGetR(color) / 255.0
                                    green:SkColorGetG(color) / 255.0
                                     blue:SkColorGetB(color) / 255.0
                                    alpha:opacity];
  return result;
}

UIImage* StretchableImageNamed(NSString* name) {
  UIImage* image = [UIImage imageNamed:name];
  if (!image) {
    return nil;
  }
  // Returns a copy of `image` configured to stretch at the center pixel.
  CGFloat half_width = floor(image.size.width / 2.0);
  CGFloat half_height = floor(image.size.height / 2.0);
  UIEdgeInsets insets =
      UIEdgeInsetsMake(half_height, half_width, half_height, half_width);
  return [image resizableImageWithCapInsets:insets];
}

// https://en.wikipedia.org/wiki/List_of_file_signatures
NSString* GetImageExtensionFromData(NSData* data) {
  if (!data || data.length < 16) {
    return nil;
  }

  const base::span<const uint8_t> span = base::apple::NSDataToSpan(data);
  switch (span[0]) {
    case uint8_t{0xff}:
      if (starts_with(span, base::byte_span_from_cstring("\xFF\xD8\xFF"))) {
        return kImageExtensionJPG;
      }
      return nil;

    case uint8_t{0x89}:
      if (starts_with(span, base::byte_span_from_cstring(
                                "\x89\x50\x4E\x47\x0D\x0A\x1A\x0A"))) {
        return kImageExtensionPNG;
      }
      return nil;

    case uint8_t{'G'}:
      if (starts_with(span, base::byte_span_from_cstring("GIF87a")) ||
          starts_with(span, base::byte_span_from_cstring("GIF89a"))) {
        return kImageExtensionGIF;
      }
      return nil;

    case uint8_t{0x49}:
      if (starts_with(span, base::byte_span_from_cstring("\x49\x49\x2A\x00"))) {
        return kImageExtensionTIF;
      }
      return nil;

    case uint8_t{0x4D}:
      if (starts_with(span, base::byte_span_from_cstring("\x4D\x4D\x00\x2A"))) {
        return kImageExtensionTIF;
      }
      return nil;

    case uint8_t{'B'}:
      if (starts_with(span, base::byte_span_from_cstring("BM"))) {
        return kImageExtensionBMP;
      }
      return nil;

    case uint8_t{'R'}:
      if (starts_with(span, base::byte_span_from_cstring("RIFF")) &&
          starts_with(span.subspan(8u), base::byte_span_from_cstring("WEBP"))) {
        return kImageExtensionWebP;
      }
      return nil;

    case uint8_t{0}:
      if (starts_with(span, base::byte_span_from_cstring("\x00\x00\x01\x00"))) {
        return kImageExtensionICO;
      }
      return nil;
  }

  return nil;
}

NSString* GetImageUTIFromData(NSData* data) {
  static NSDictionary* dict = @{
    kImageExtensionJPG : UTTypeJPEG.identifier,
    kImageExtensionPNG : UTTypePNG.identifier,
    kImageExtensionGIF : UTTypeGIF.identifier,
    kImageExtensionTIF : UTTypeTIFF.identifier,
    kImageExtensionBMP : UTTypeBMP.identifier,
    kImageExtensionICO : UTTypeICO.identifier
  };

  return dict[GetImageExtensionFromData(data)];
}
