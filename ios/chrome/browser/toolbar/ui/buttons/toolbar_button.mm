// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/toolbar/ui/buttons/toolbar_button.h"

#import "ios/chrome/browser/toolbar/ui/buttons/toolbar_button_constants.h"
#import "ios/chrome/browser/toolbar/ui/buttons/toolbar_buttons_utils.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/ui_util.h"

namespace {
constexpr CGFloat kDisabledOpacity = 0.4;
}  // namespace

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

    [NSLayoutConstraint activateConstraints:@[
      [self.widthAnchor constraintEqualToConstant:kSize],
      [self.heightAnchor constraintEqualToAnchor:self.widthAnchor],
    ]];

    self.backgroundColor = ToolbarButtonColor();

    ConfigureCornerRadiusForToolbarButtonContainer(self);

    ConfigureShadowForToolbarButton(self);

    self.tintColor = [UIColor colorNamed:kSolidBlackColor];

    [self registerForTraitChanges:@[
      UITraitVerticalSizeClass.class, UITraitHorizontalSizeClass.class
    ]
                       withAction:@selector(updateAppearance)];
  }
  return self;
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
  [self updateAppearance];
}

- (void)setEnabled:(BOOL)enabled {
  [super setEnabled:enabled];
  if (enabled) {
    self.tintColor = [UIColor colorNamed:kSolidBlackColor];
  } else {
    self.tintColor = [[UIColor colorNamed:kSolidBlackColor]
        colorWithAlphaComponent:kDisabledOpacity];
  }
  [self updateAppearance];
}

- (void)setVisibilityMask:(ToolbarButtonVisibility)visibilityMask {
  _visibilityMask = visibilityMask;
  [self updateAppearance];
}

#pragma mark - Private

// Updates the image visibility based on the visibility of the button.
- (void)checkImageVisibility {
  if (!self.hidden && !self.currentImage) {
    [self setImage:self.image forState:UIControlStateNormal];
  }
}

// Updates the appearance of this ToolbarButton.
- (void)updateAppearance {
  [self updateVisibility];
  [self updateShape];
}

// Helper for `-updateAppearance`. Updates the visibility of this button based
// on the current state and the visibility mask.
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

// Helper for `-updateAppearance`. Updates the shape of this button based on the
// current size class of the UI. In windows with compact width, the
// ToolbarButton should be square. Otherwise, they should be circular.
- (void)updateShape {
  ConfigureCornerRadiusForToolbarButtonContainer(self);
}

@end
