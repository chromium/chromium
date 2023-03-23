// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_button.h"

#import "base/check.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_configuration.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kSpotlightSize = 38;
const CGFloat kSpotlightCornerRadius = 7;
}  // namespace

@implementation ToolbarButton

+ (instancetype)toolbarButtonWithImage:(UIImage*)image {
  ToolbarButton* button = [[self class] buttonWithType:UIButtonTypeSystem];
  [button setImage:image forState:UIControlStateNormal];
  button.translatesAutoresizingMaskIntoConstraints = NO;

  UIView* spotlightView = [[UIView alloc] init];
  spotlightView.translatesAutoresizingMaskIntoConstraints = NO;
  spotlightView.hidden = YES;
  spotlightView.userInteractionEnabled = NO;
  spotlightView.layer.cornerRadius = kSpotlightCornerRadius;
  // Make sure that the spotlightView is below the image to avoid changing the
  // color of the image.
  [button insertSubview:spotlightView belowSubview:button.imageView];
  AddSameCenterConstraints(button, spotlightView);
  [spotlightView.widthAnchor constraintEqualToConstant:kSpotlightSize].active =
      YES;
  [spotlightView.heightAnchor constraintEqualToConstant:kSpotlightSize].active =
      YES;
  button.spotlightView = spotlightView;

  return button;
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
}

- (void)setHiddenInCurrentState:(BOOL)hiddenInCurrentState {
  _hiddenInCurrentState = hiddenInCurrentState;
  [self setHiddenForCurrentStateAndSizeClass];
}

- (void)setIphHighlighted:(BOOL)iphHighlighted {
  if (iphHighlighted == _iphHighlighted)
    return;

  _iphHighlighted = iphHighlighted;
  [self updateTintColor];
  [self updateSpotlightView];
}

- (void)setToolbarConfiguration:(ToolbarConfiguration*)toolbarConfiguration {
  _toolbarConfiguration = toolbarConfiguration;
  if (!toolbarConfiguration)
    return;
  self.spotlightView.backgroundColor =
      self.toolbarConfiguration.buttonsIPHHighlightColor;
  [self updateTintColor];
  [self updateSpotlightView];
}

#pragma mark - Private

// Checks if the button should be visible based on its hiddenInCurrentSizeClass
// and hiddenInCurrentState properties, then updates its visibility accordingly.
- (void)setHiddenForCurrentStateAndSizeClass {
  self.hidden = self.hiddenInCurrentState || self.hiddenInCurrentSizeClass;

  [self checkNamedGuide];
}

// Checks whether the named guide associated with this button, if there is one,
// should be updated.
- (void)checkNamedGuide {
  if (!self.hidden && self.guideName) {
    [self.layoutGuideCenter referenceView:self underName:self.guideName];
  }
}

// Updates the spotlight view's appearance according to the current state.
- (void)updateSpotlightView {
  self.spotlightView.hidden = !self.iphHighlighted;
}

// Updates the tint color according to the current state.
- (void)updateTintColor {
  self.tintColor =
      (self.iphHighlighted)
          ? self.toolbarConfiguration.buttonsTintColorIPHHighlighted
          : self.toolbarConfiguration.buttonsTintColor;
}

@end
