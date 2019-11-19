// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <MobileCoreServices/MobileCoreServices.h>
#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/image_util/image_util.h"
#include "ui/gfx/color_analysis.h"
#include "ui/gfx/image/image.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
NSString* kImageExtensionJPG = @"jpg";
NSString* kImageExtensionPNG = @"png";
NSString* kImageExtensionTIF = @"tif";
NSString* kImageExtensionBMP = @"bmp";
NSString* kImageExtensionGIF = @"gif";
NSString* kImageExtensionICO = @"ico";
NSString* kImageExtensionWebP = @"webp";

}

UIColor* DominantColorForImage(const gfx::Image& image, CGFloat opacity) {
  SkColor color = color_utils::CalculateKMeanColorOfBitmap(*image.ToSkBitmap());
  UIColor* result = [UIColor colorWithRed:SkColorGetR(color) / 255.0
                                    green:SkColorGetG(color) / 255.0
                                     blue:SkColorGetB(color) / 255.0
                                    alpha:opacity];
  return result;
}

UIImage* StretchableImageFromUIImage(UIImage* image,
                                     NSInteger left_cap_width,
                                     NSInteger top_cap_height) {
  UIEdgeInsets insets = UIEdgeInsetsMake(
      top_cap_height, left_cap_width, image.size.height - top_cap_height + 1.0,
      image.size.width - left_cap_width + 1.0);
  return [image resizableImageWithCapInsets:insets];
}

UIImage* StretchableImageNamed(NSString* name) {
  UIImage* image = [UIImage imageNamed:name];
  if (!image)
    return nil;
  // Returns a copy of |image| configured to stretch at the center pixel.
  return StretchableImageFromUIImage(image, floor(image.size.width / 2.0),
                                     floor(image.size.height / 2.0));
}

UIImage* StretchableImageNamed(NSString* name,
                               NSInteger left_cap_width,
                               NSInteger top_cap_height) {
  UIImage* image = [UIImage imageNamed:name];
  if (!image)
    return nil;

  return StretchableImageFromUIImage(image, left_cap_width, top_cap_height);
}

// https://en.wikipedia.org/wiki/List_of_file_signatures
NSString* GetImageExtensionFromData(NSData* data) {
  if (!data || data.length < 16)
    return nil;

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
    kImageExtensionJPG : (__bridge NSString*)kUTTypeJPEG,
    kImageExtensionPNG : (__bridge NSString*)kUTTypePNG,
    kImageExtensionGIF : (__bridge NSString*)kUTTypeGIF,
    kImageExtensionTIF : (__bridge NSString*)kUTTypeTIFF,
    kImageExtensionBMP : (__bridge NSString*)kUTTypeBMP,
    kImageExtensionICO : (__bridge NSString*)kUTTypeICO
  };
  return dict[GetImageExtensionFromData(data)];
}
