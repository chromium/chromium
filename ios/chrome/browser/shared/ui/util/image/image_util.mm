// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#import "ios/chrome/browser/shared/ui/util/image/image_util.h"
#import "ui/gfx/color_analysis.h"
#import "ui/gfx/image/image.h"

namespace {
NSString* kImageExtensionJPG = @"jpg";
NSString* kImageExtensionPNG = @"png";
NSString* kImageExtensionTIF = @"tif";
NSString* kImageExtensionBMP = @"bmp";
NSString* kImageExtensionGIF = @"gif";
NSString* kImageExtensionICO = @"ico";
NSString* kImageExtensionWebP = @"webp";

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

  const char* pdata = static_cast<const char*>(data.bytes);
  switch (pdata[0]) {
    case '\xFF':
      return strncmp(pdata, "\xFF\xD8\xFF", 3) ? nil : kImageExtensionJPG;
    case '\x89':
      return strncmp(pdata, "\x89\x50\x4E\x47\x0D\x0A\x1A\x0A", 8)
                 ? nil
                 : kImageExtensionPNG;
    case 'G':
      return (strncmp(pdata, "GIF87a", 6) && strncmp(pdata, "GIF89a", 6))
                 ? nil
                 : kImageExtensionGIF;
    case '\x49':
      return strncmp(pdata, "\x49\x49\x2A\x00", 4) ? nil : kImageExtensionTIF;
    case '\x4D':
      return strncmp(pdata, "\x4D\x4D\x00\x2A", 4) ? nil : kImageExtensionTIF;
    case 'B':
      return strncmp(pdata, "BM", 2) ? nil : kImageExtensionBMP;
    case 'R':
      return (strncmp(pdata, "RIFF", 4) || strncmp(pdata + 8, "WEBP", 4))
                 ? nil
                 : kImageExtensionWebP;
    case '\0':
      return strncmp(pdata, "\x00\x00\x01\x00", 4) ? nil : kImageExtensionICO;
    default:
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
