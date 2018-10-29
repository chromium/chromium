// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_button.h"

#include "base/logging.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_configuration.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_constants.h"
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
@synthesize visibilityMask = _visibilityMask;
@synthesize guideName = _guideName;
@synthesize hiddenInCurrentSizeClass = _hiddenInCurrentSizeClass;
@synthesize hiddenInCurrentState = _hiddenInCurrentState;
@synthesize spotlighted = _spotlighted;
@synthesize dimmed = _dimmed;
@synthesize configuration = _configuration;
@synthesize spotlightView = _spotlightView;

+ (instancetype)toolbarButtonWithImageForNormalState:(UIImage*)normalImage
                            imageForHighlightedState:(UIImage*)highlightedImage
                               imageForDisabledState:(UIImage*)disabledImage {
  ToolbarButton* button = [[self class] buttonWithType:UIButtonTypeCustom];
  [button setImage:normalImage forState:UIControlStateNormal];
  [button setImage:highlightedImage forState:UIControlStateHighlighted];
  [button setImage:disabledImage forState:UIControlStateDisabled];
  [button setImage:highlightedImage forState:UIControlStateSelected];
  button.translatesAutoresizingMaskIntoConstraints = NO;
  return button;
}

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
  DCHECK(ControlStateSpotlighted & UIControlStateApplication);
  UIControlState state = [super state];
  if (self.spotlighted)
    state |= ControlStateSpotlighted;
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
  [self addSubview:spotlightView];
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
  BOOL previouslyHidden = self.hidden;
  self.hidden = self.hiddenInCurrentState || self.hiddenInCurrentSizeClass;

  if (!self.hidden && previouslyHidden != self.hidden && self.guideName) {
    // The button is appearing. At this point, if it has a layout guide
    // associated, it should constraint it to itself.
    [NamedGuide guideWithName:self.guideName view:self].constrainedView = self;
  }
}

@end
