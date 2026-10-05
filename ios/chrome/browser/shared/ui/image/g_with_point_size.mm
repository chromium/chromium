// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/image/g_with_point_size.h"

#import "base/check.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/shared/ui/buildflags.h"

UIImage* GWithPointSize(SuperGSize size) {
#if BUILDFLAG(IOS_USE_BRANDED_ASSETS)
  NSString* image_name = nil;
  switch (size) {
    case SuperGSize::k16pt:
      image_name = @"google_icon_16pt";
      break;
    case SuperGSize::k18pt:
      image_name = @"google_icon_18pt";
      break;
    case SuperGSize::k24pt:
      image_name = @"google_icon_24pt";
      break;
  }
  UIImage* image = [UIImage imageNamed:image_name];
  DCHECK(image) << " image_name: " << base::SysNSStringToUTF8(image_name);
  return image;
#else
  return nil;
#endif  // BUILDFLAG(IOS_USE_BRANDED_ASSETS)
}
