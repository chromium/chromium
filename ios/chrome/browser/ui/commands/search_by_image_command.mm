// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/commands/search_by_image_command.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SearchByImageCommand () {
  GURL _URL;
}
@end

@implementation SearchByImageCommand

- (instancetype)initWithImage:(UIImage*)image {
  return [self initWithImage:image URL:GURL() inNewTab:NO];
}

- (instancetype)initWithImage:(UIImage*)image
                          URL:(const GURL&)URL
                     inNewTab:(BOOL)inNewTab {
  self = [super init];
  if (self) {
    _URL = URL;
    _image = image;
    _inNewTab = inNewTab;
  }
  return self;
}

- (const GURL&)URL {
  return _URL;
}

@end
