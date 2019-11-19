// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_button.h"

#include "base/logging.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_configuration.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_constants.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"

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
  [button configureSpotlightView];
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

- (void)setSpotlighted:(BOOL)spotlighted {
  if (spotlighted == _spotlighted)
    return;

  _spotlighted = spotlighted;
  self.spotlightView.hidden = !spotlighted;
  [self setNeedsLayout];
  [self layoutIfNeeded];
}

- (void)setDimmed:(BOOL)dimmed {
  if (dimmed == _dimmed)
    return;
  _dimmed = dimmed;
  if (!self.configuration)
    return;

  if (dimmed) {
    self.alpha = kToolbarDimmedButtonAlpha;
    if (_spotlightView) {
      self.spotlightView.backgroundColor =
          self.configuration.dimmedButtonsSpotlightColor;
    }
  } else {
    self.alpha = 1;
    if (_spotlightView) {
      self.spotlightView.backgroundColor =
          self.configuration.buttonsSpotlightColor;
    }
  }
}

- (UIControlState)state {
  DCHECK(kControlStateSpotlighted & UIControlStateApplication);
  UIControlState state = [super state];
  if (self.spotlighted)
    state |= kControlStateSpotlighted;
  return state;
}

- (void)setConfiguration:(ToolbarConfiguration*)configuration {
  _configuration = configuration;
  if (!configuration)
    return;

  self.tintColor = configuration.buttonsTintColor;
  _spotlightView.backgroundColor = self.configuration.buttonsSpotlightColor;
}

#pragma mark - Subclassing

- (void)configureSpotlightView {
  UIView* spotlightView = [[UIView alloc] init];
  spotlightView.translatesAutoresizingMaskIntoConstraints = NO;
  spotlightView.hidden = YES;
  spotlightView.userInteractionEnabled = NO;
  spotlightView.layer.cornerRadius = kSpotlightCornerRadius;
  spotlightView.backgroundColor = self.configuration.buttonsSpotlightColor;
  // Make sure that the spotlightView is below the image to avoid changing the
  // color of the image.
  [self insertSubview:spotlightView belowSubview:self.imageView];
  AddSameCenterConstraints(self, spotlightView);
  [spotlightView.widthAnchor constraintEqualToConstant:kSpotlightSize].active =
      YES;
  [spotlightView.heightAnchor constraintEqualToConstant:kSpotlightSize].active =
      YES;
  self.spotlightView = spotlightView;
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
    NamedGuide* guide = [NamedGuide guideWithName:self.guideName view:self];
    if (guide.constrainedView != self)
      guide.constrainedView = self;
  }
}

@end
