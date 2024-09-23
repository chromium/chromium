// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_button.h"

#import "base/check.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/browser/ui/toolbar/buttons/buttons_constants.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_configuration.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {
const CGFloat kSpotlightSize = 38;
const CGFloat kSpotlightCornerRadius = 7;
const CGFloat kToolsMenuButtonImageSize = 35;
const CGFloat kBlueDotSize = 10;
const CGFloat kButtonImageInset = 3;
}  // namespace

@interface ToolbarButton () {
  // The image loader used to load `_image` when the button is updated to
  // visible.
  ToolbarButtonImageLoader _imageLoader;
  // The image loader used to load `_IPHHighlightedImage` when the button is
  // updated to visible and highlighted.
  ToolbarButtonImageLoader _IPHHighlightedImageLoader;
}

// The image used for the normal state.
@property(nonatomic, strong) UIImage* image;
// The image used for iphHighlighted state. If this property is not nil, the
// iphHighlighted effect will be replacing the default image with this one,
// instead of using tint color OR `self.spotlightView`.
@property(nonatomic, strong) UIImage* IPHHighlightedImage;
// View used to display the blue dot on the icon.
@property(nonatomic, strong) UIView* blueDotView;
@end

@implementation ToolbarButton

- (instancetype)initWithImageLoader:(ToolbarButtonImageLoader)imageLoader {
  return [self initWithImageLoader:imageLoader IPHHighlightedImageLoader:nil];
}

- (instancetype)initWithImageLoader:(ToolbarButtonImageLoader)imageLoader
          IPHHighlightedImageLoader:
              (ToolbarButtonImageLoader)IPHHighlightedImageLoader {
  self = [[super class] buttonWithType:UIButtonTypeSystem];
  if (self) {
    CHECK(imageLoader);
    _imageLoader = [imageLoader copy];
    _IPHHighlightedImageLoader = [IPHHighlightedImageLoader copy];

    [self initializeButton];
  }
  return self;
}

- (void)setImageLoader:(ToolbarButtonImageLoader)imageLoader {
  CHECK(imageLoader);
  _imageLoader = [imageLoader copy];
  if (_image != nil) {
    _image = nil;
    [self updateImage];
  }
}

#pragma mark - Public Methods

- (void)updateHiddenInCurrentSizeClass {
  BOOL newHiddenValue = YES;

  BOOL isCompactWidth = self.traitCollection.horizontalSizeClass ==
                        UIUserInterfaceSizeClassCompact;
  BOOL isCompactHeight =
      self.traitCollection.verticalSizeClass == UIUserInterfaceSizeClassCompact;
  BOOL isRegularWidth = self.traitCollection.horizontalSizeClass ==
                        UIUserInterfaceSizeClassRegular;
  BOOL isRegularHeight =
      self.traitCollection.verticalSizeClass == UIUserInterfaceSizeClassRegular;

  if (isCompactWidth && isCompactHeight) {
    newHiddenValue = !(self.visibilityMask &
                       ToolbarComponentVisibilityCompactWidthCompactHeight);
  } else if (isCompactWidth && isRegularHeight) {
    newHiddenValue = !(self.visibilityMask &
                       ToolbarComponentVisibilityCompactWidthRegularHeight);
  } else if (isRegularWidth && isCompactHeight) {
    newHiddenValue = !(self.visibilityMask &
                       ToolbarComponentVisibilityRegularWidthCompactHeight);
  } else if (isRegularWidth && isRegularHeight) {
    newHiddenValue = !(self.visibilityMask &
                       ToolbarComponentVisibilityRegularWidthRegularHeight);
  }

  if (self.hiddenInCurrentSizeClass != newHiddenValue) {
    self.hiddenInCurrentSizeClass = newHiddenValue;
    [self setHiddenForCurrentStateAndSizeClass];
  }

  [self checkNamedGuide];
  [self checkImageVisibility];
}

- (void)setHiddenInCurrentState:(BOOL)hiddenInCurrentState {
  _hiddenInCurrentState = hiddenInCurrentState;
  [self setHiddenForCurrentStateAndSizeClass];
}

- (void)setIphHighlighted:(BOOL)iphHighlighted {
  if (iphHighlighted == _iphHighlighted)
    return;

  _iphHighlighted = iphHighlighted;
  if ([self canUseIPHHighlightedImage]) {
    [self updateImage];
  } else {
    [self updateTintColor];
    [self updateSpotlightView];
  }
}

- (void)setToolbarConfiguration:(ToolbarConfiguration*)toolbarConfiguration {
  _toolbarConfiguration = toolbarConfiguration;
  if (!toolbarConfiguration)
    return;
  _spotlightView.backgroundColor =
      self.toolbarConfiguration.buttonsIPHHighlightColor;
  [self updateTintColor];
}

- (void)setHasBlueDot:(BOOL)hasBlueDot {
  if (_hasBlueDot == hasBlueDot) {
    return;
  }

  _hasBlueDot = hasBlueDot;

  if (hasBlueDot) {
    [self addBlueDotViewIfNeeded];
  } else {
    [self removeBlueDotViewIfNeeded];
  }
}

#pragma mark - Accessors

- (UIView*)spotlightView {
  // Lazy load spotlightView to improve startup latency.
  if (!_spotlightView) {
    [self createSpotlightViewIfNeeded];
  }
  return _spotlightView;
}

- (UIImage*)image {
  // Lazy load image to improve startup latency.
  if (!_image) {
    _image = _imageLoader();
  }
  return _image;
}

- (UIImage*)IPHHighlightedImage {
  // Lazy load IPHHighlightedImage to improve startup latency.
  if (!_IPHHighlightedImage && _IPHHighlightedImageLoader) {
    _IPHHighlightedImage = _IPHHighlightedImageLoader();
  }
  return _IPHHighlightedImage;
}

#pragma mark - Private

- (void)initializeButton {
  self.translatesAutoresizingMaskIntoConstraints = NO;

  __weak __typeof(self) weakSelf = self;
  CustomHighlightableButtonHighlightHandler handler = ^(BOOL highlighted) {
    [weakSelf setIphHighlighted:highlighted];
  };
  [self setCustomHighlightHandler:handler];
}

// Creates spotlightView if not done yet.
- (void)createSpotlightViewIfNeeded {
  if (_spotlightView) {
    return;
  }
  UIView* spotlightView = [[UIView alloc] init];
  spotlightView.translatesAutoresizingMaskIntoConstraints = NO;
  spotlightView.hidden = YES;
  spotlightView.userInteractionEnabled = NO;
  spotlightView.layer.cornerRadius = kSpotlightCornerRadius;
  spotlightView.backgroundColor =
      self.toolbarConfiguration.buttonsIPHHighlightColor;
  // Make sure that the spotlightView is below the image to avoid changing the
  // color of the image.
  [self insertSubview:spotlightView belowSubview:self.imageView];
  AddSameCenterConstraints(self, spotlightView);
  [spotlightView.widthAnchor constraintEqualToConstant:kSpotlightSize].active =
      YES;
  [spotlightView.heightAnchor constraintEqualToConstant:kSpotlightSize].active =
      YES;
  _spotlightView = spotlightView;
}

// Checks if the button should be visible based on its hiddenInCurrentSizeClass
// and hiddenInCurrentState properties, then updates its visibility accordingly.
- (void)setHiddenForCurrentStateAndSizeClass {
  self.hidden = self.hiddenInCurrentState || self.hiddenInCurrentSizeClass;

  [self checkNamedGuide];
  [self checkImageVisibility];
}

// Checks whether the named guide associated with this button, if there is one,
// should be updated.
- (void)checkNamedGuide {
  if (!self.hidden && self.guideName) {
    [self.layoutGuideCenter referenceView:self underName:self.guideName];
  }
}

// Checks whether the image is set when the button visibility is changed, if the
// button is visible and the image is not set, update the image.
- (void)checkImageVisibility {
  // Use `self.currentImage` to check whether the image is set,
  // `self.imageView.image` is a costly call from the Instruments measurement.
  if (!self.hidden && !self.currentImage) {
    [self updateImage];
  }
}

// Updates the spotlight view's appearance according to the current state.
- (void)updateSpotlightView {
  self.spotlightView.hidden = !self.iphHighlighted;
}

- (void)updateImage {
  if (_iphHighlighted && [self canUseIPHHighlightedImage]) {
    [self setImage:self.IPHHighlightedImage forState:UIControlStateNormal];
  } else {
    [self setImage:self.image forState:UIControlStateNormal];
  }
}

// Updates the tint color according to the current state.
- (void)updateTintColor {
  self.tintColor =
      (self.iphHighlighted)
          ? self.toolbarConfiguration.buttonsTintColorIPHHighlighted
          : self.toolbarConfiguration.buttonsTintColor;
}

// Whether there is an IPH highlighted image can be used.
- (BOOL)canUseIPHHighlightedImage {
  return _IPHHighlightedImageLoader != nil;
}

// Adds blue dot view to the button if there is none yet.
- (void)addBlueDotViewIfNeeded {
  if (self.blueDotView) {
    return;
  }

  self.blueDotView = [[UIView alloc] init];
  self.blueDotView.translatesAutoresizingMaskIntoConstraints = NO;
  self.blueDotView.accessibilityIdentifier = kToolbarButtonBlueDotViewID;
  self.blueDotView.layer.cornerRadius = kBlueDotSize / 2;
  self.blueDotView.backgroundColor = [UIColor colorNamed:kBlue600Color];
  [self addSubview:self.blueDotView];

  [NSLayoutConstraint activateConstraints:@[
    [self.blueDotView.widthAnchor constraintEqualToConstant:kBlueDotSize],
    [self.blueDotView.heightAnchor constraintEqualToConstant:kBlueDotSize],
    // Position the blue dot at right top corner of the button image.
    [self.blueDotView.centerXAnchor
        constraintEqualToAnchor:self.centerXAnchor
                       constant:kToolsMenuButtonImageSize / 2 -
                                kButtonImageInset],
    [self.blueDotView.centerYAnchor
        constraintEqualToAnchor:self.centerYAnchor
                       constant:-kToolsMenuButtonImageSize / 2 +
                                kButtonImageInset],
  ]];
}

// Removes blue dot view from the button if there is one.
- (void)removeBlueDotViewIfNeeded {
  if (!self.blueDotView) {
    return;
  }

  [self.blueDotView removeFromSuperview];
  self.blueDotView = nil;
}

@end
