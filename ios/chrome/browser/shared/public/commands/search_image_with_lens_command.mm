// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/public/commands/search_image_with_lens_command.h"

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
