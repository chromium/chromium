// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/lens/lens_image_source.h"

@implementation LensImageSource

- (instancetype)initWithSnapshot:(UIImage*)snapshot {
  self = [super init];
  if (self) {
    _snapshot = snapshot;
  }

  return self;
}

- (instancetype)initWithImageMetadata:(id<LensImageMetadata>)imageMetadata {
  self = [super init];
  if (self) {
    _imageMetadata = imageMetadata;
  }

  return self;
}

- (BOOL)isValid {
  return _snapshot != nil || _imageMetadata != nil;
}

@end
