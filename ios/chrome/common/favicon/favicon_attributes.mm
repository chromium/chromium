// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/favicon/favicon_attributes.h"

#include "base/logging.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation FaviconAttributes

- (instancetype)initWithImage:(UIImage*)image
                     monogram:(NSString*)monogram
                    textColor:(UIColor*)textColor
              backgroundColor:(UIColor*)backgroundColor
       defaultBackgroundColor:(BOOL)defaultBackgroundColor
             usesDefaultImage:(BOOL)defaultImage {
  DCHECK(image || (monogram && textColor && backgroundColor));
  self = [super init];
  if (self) {
    _faviconImage = image;
    _monogramString = [monogram copy];
    _textColor = textColor;
    _backgroundColor = backgroundColor;
    _defaultBackgroundColor = defaultBackgroundColor;
    _usesDefaultImage = defaultImage;
  }

  return self;
}

+ (instancetype)attributesWithImage:(UIImage*)image {
  DCHECK(image);
  return [[self alloc] initWithImage:image
                            monogram:nil
                           textColor:nil
                     backgroundColor:nil
              defaultBackgroundColor:NO
                    usesDefaultImage:NO];
}

+ (instancetype)attributesWithMonogram:(NSString*)monogram
                             textColor:(UIColor*)textColor
                       backgroundColor:(UIColor*)backgroundColor
                defaultBackgroundColor:(BOOL)defaultBackgroundColor {
  return [[self alloc] initWithImage:nil
                            monogram:monogram
                           textColor:textColor
                     backgroundColor:backgroundColor
              defaultBackgroundColor:defaultBackgroundColor
                    usesDefaultImage:NO];
}

+ (instancetype)attributesWithDefaultImage {
  return
      [[self alloc] initWithImage:[UIImage imageNamed:@"default_world_favicon"]
                         monogram:nil
                        textColor:nil
                  backgroundColor:nil
           defaultBackgroundColor:NO
                 usesDefaultImage:YES];
}

@end
