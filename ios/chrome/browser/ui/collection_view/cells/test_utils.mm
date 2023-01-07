// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/collection_view/cells/test_utils.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

UIImage* CollectionViewTestImage() {
  CGRect rect = CGRectMake(0.0, 0.0, 1.0, 1.0);
  UIGraphicsBeginImageContext(rect.size);
  CGContextRef context = UIGraphicsGetCurrentContext();

  CGContextSetFillColorWithColor(context, [UIColor blueColor].CGColor);
  CGContextFillRect(context, rect);

  UIImage* image = UIGraphicsGetImageFromCurrentImageContext();
  UIGraphicsEndImageContext();

  return image;
}
