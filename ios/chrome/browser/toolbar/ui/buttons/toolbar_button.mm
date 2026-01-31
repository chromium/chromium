// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/toolbar/ui/buttons/toolbar_button.h"

#import "ios/chrome/common/ui/util/ui_util.h"

@interface ToolbarButton ()

// The image from the imageLoader, if it has been loaded.
@property(nonatomic, strong, readonly) UIImage* image;

@end

@implementation ToolbarButton {
  ToolbarButtonImageLoader _imageLoader;
}

@synthesize image = _image;

- (instancetype)initWithImageLoader:(ToolbarButtonImageLoader)imageLoader {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    _imageLoader = [imageLoader copy];
  }
  return self;
}

- (void)updateVisibility {
  if (self.forceHidden) {
    self.hidden = YES;
    return;
  }
  BOOL isCurrentRegularRegular = IsRegularXRegularSizeClass(self);
  BOOL isCurrentCompactHeight =
      self.traitCollection.verticalSizeClass == UIUserInterfaceSizeClassCompact;

  switch (self.visibilityMask) {
    case ToolbarButtonVisibility::kAlways:
      break;
    case ToolbarButtonVisibility::kRegularRegular:
      self.hidden = !isCurrentRegularRegular;
      break;
    case ToolbarButtonVisibility::kCompactHeight:
      self.hidden = !isCurrentCompactHeight;
      break;
    case ToolbarButtonVisibility::kWhenEnabled:
      self.hidden = !self.enabled;
      break;
  }
  [self checkImageVisibility];
}

#pragma mark - Properties

- (UIImage*)image {
  if (!_image) {
    _image = _imageLoader();
  }
  return _image;
}

- (void)setForceHidden:(BOOL)forceHidden {
  _forceHidden = forceHidden;
  [self updateVisibility];
}

- (void)setEnabled:(BOOL)enabled {
  [super setEnabled:enabled];
  [self updateVisibility];
}

#pragma mark - Private

// Updates the image visibility based on the visibility of the button.
- (void)checkImageVisibility {
  if (!self.hidden && !self.currentImage) {
    [self setImage:self.image forState:UIControlStateNormal];
  }
}

@end
