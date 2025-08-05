// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/aim/prototype/coordinator/aim_prototype_mediator.h"

@implementation AIMPrototypeMediator {
  NSMutableArray<UIImage*>* _images;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _images = [NSMutableArray array];
  }
  return self;
}

- (void)processImage:(UIImage*)image {
  // TODO(crbug.com/40280872): Implement image processing.
  [_images addObject:image];
  [self.consumer setImages:_images];
}

@end
