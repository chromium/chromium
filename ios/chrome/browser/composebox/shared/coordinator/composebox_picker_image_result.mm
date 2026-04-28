// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/shared/coordinator/composebox_picker_image_result.h"

@implementation ComposeboxPickerImageResult

- (instancetype)initWithImageProvider:(NSItemProvider*)imageProvider
                              assetID:(NSString*)assetID {
  self = [super init];
  if (self) {
    _imageProvider = imageProvider;
    _assetID = [assetID copy];
  }

  return self;
}
@end
