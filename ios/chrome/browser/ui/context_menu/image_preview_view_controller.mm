// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/context_menu/image_preview_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kDefaultSize = 50;
}  // namespace

@interface ImagePreviewViewController ()

// The image view containing the image.
@property(nonatomic, strong) UIImageView* view;

@end

@implementation ImagePreviewViewController

@dynamic view;

#pragma mark - Public

- (void)loadView {
  self.view = [[UIImageView alloc] init];

  self.preferredContentSize = CGSizeMake(kDefaultSize, kDefaultSize);
}

- (void)updateImageData:(NSData*)data {
  UIImage* image = [UIImage imageWithData:data];
  self.view.image = image;

  self.preferredContentSize = image.size;
}

@end
