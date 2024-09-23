// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/sharing/activity_services/data/share_image_data.h"

@implementation ShareImageData

- (instancetype)initWithImage:(UIImage*)image title:(NSString*)title {
  if ((self = [super init])) {
    _image = image;
    _title = title;
  }
  return self;
}

@end
