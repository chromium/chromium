// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_new_tab_button.h"

#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_configuration.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_constants.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/pointer_interaction_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kSpotlightHeight = 36.0f;
}  // namespace

@implementation ToolbarNewTabButton

+ (instancetype)toolbarButtonWithImage:(UIImage*)image {
  ToolbarNewTabButton* button = [super toolbarButtonWithImage:image];
  return button;
}

- (void)setDimmed:(BOOL)dimmed {
  [super setDimmed:dimmed];
  [self updateSpotlightViewHiddenState];
}

- (void)setSpotlighted:(BOOL)spotlighted {
  [super setSpotlighted:spotlighted];
  [self updateSpotlightViewHiddenState];
}

- (void)setIphHighlighted:(BOOL)iphHighlighted {
  [super setIphHighlighted:iphHighlighted];
  [self updateSpotlightViewHiddenState];
}

- (void)setToolbarConfiguration:(ToolbarConfiguration*)toolbarConfiguration {
  [super setToolbarConfiguration:toolbarConfiguration];
  [self updateSpotlightViewHiddenState];
}

- (void)updateSpotlightViewHiddenState {
  self.spotlightView.hidden =
      self.dimmed && !self.spotlighted && !self.iphHighlighted;
}

#pragma mark - Subclassing

- (void)configureSpotlightView {
  UIView* spotlightView = [[UIView alloc] init];
  spotlightView.translatesAutoresizingMaskIntoConstraints = NO;
  spotlightView.userInteractionEnabled = NO;
  spotlightView.layer.cornerRadius = kSpotlightHeight / 2;
  spotlightView.backgroundColor =
      self.toolbarConfiguration.buttonsSpotlightColor;
  // Make sure that the spotlightView is below the image to avoid changing the
  // color of the image.
  [self insertSubview:spotlightView belowSubview:self.imageView];

  AddSameCenterConstraints(self, spotlightView);
  [spotlightView.heightAnchor constraintEqualToConstant:kSpotlightHeight]
      .active = YES;
  [spotlightView.widthAnchor constraintEqualToConstant:kSpotlightHeight]
      .active = YES;
  self.spotlightView = spotlightView;

  // Customize the pointer highlight tomatch the spotlight view.
  self.pointerInteractionEnabled = YES;
  self.pointerStyleProvider = CreateLiftEffectCirclePointerStyleProvider();
}

@end
