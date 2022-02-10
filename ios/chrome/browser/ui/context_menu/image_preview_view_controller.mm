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

- (instancetype)initWithPreferredContentSize:(CGSize)preferredContentSize {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    if (preferredContentSize.width <= 0.0 ||
        preferredContentSize.height <= 0.0) {
      self.preferredContentSize = CGSizeMake(kDefaultSize, kDefaultSize);
    } else {
      self.preferredContentSize = preferredContentSize;
    }
  }
  return self;
}

- (void)loadView {
  self.view = [[UIImageView alloc] init];
}

- (void)updateImage:(UIImage*)image {
  self.view.image = image;
}

- (void)updateImageData:(NSData*)data {
  UIImage* image = [UIImage imageWithData:data];
  self.view.image = image;

  // UIPreviewProvider cannot animate |preferredContentSize| changes.
  // Changing |preferredContentSize| during animation will make it glitch.
  // See crbug.com/1288017.
  // Here we set |preferredContentSize| as a last resort, if
  // the |preferredContentSize| provided during initialization is invalid.
  if (self.preferredContentSize.width == kDefaultSize &&
      self.preferredContentSize.height == kDefaultSize) {
    self.preferredContentSize = image.size;
  }
}

@end
