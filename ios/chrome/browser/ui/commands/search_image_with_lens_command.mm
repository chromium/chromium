// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/commands/search_image_with_lens_command.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation SearchImageWithLensCommand

- (instancetype)initWithImage:(UIImage*)image
                   entryPoint:(LensEntrypoint)entryPoint {
  self = [super init];
  if (self) {
    _image = image;
    _entryPoint = entryPoint;
  }
  return self;
}

@end
