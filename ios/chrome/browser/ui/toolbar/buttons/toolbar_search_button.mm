// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_search_button.h"

#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_configuration.h"
#import "ios/chrome/browser/ui/toolbar/public/features.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_constants.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kSpotlightHeight = 38.0f;
}  // namespace

@implementation ToolbarSearchButton

+ (instancetype)toolbarButtonWithImage:(UIImage*)image {
  ToolbarSearchButton* button = [super toolbarButtonWithImage:image];
  return button;
}

- (void)setDimmed:(BOOL)dimmed {
  [super setDimmed:dimmed];

  self.spotlightView.hidden = dimmed && !self.spotlighted;
}

- (void)setSpotlighted:(BOOL)spotlighted {
  [super setSpotlighted:spotlighted];
  self.spotlightView.hidden = !spotlighted && self.dimmed;
}

#pragma mark - Subclassing

- (void)configureSpotlightView {
  UIView* spotlightView = [[UIView alloc] init];
  spotlightView.translatesAutoresizingMaskIntoConstraints = NO;
  spotlightView.userInteractionEnabled = NO;
  spotlightView.layer.cornerRadius = kSpotlightHeight / 2;
  spotlightView.backgroundColor = self.configuration.buttonsSpotlightColor;
  // Make sure that the spotlightView is below the image to avoid changing the
  // color of the image.
  [self insertSubview:spotlightView belowSubview:self.imageView];

  AddSameCenterConstraints(self, spotlightView);
  [spotlightView.heightAnchor constraintEqualToConstant:kSpotlightHeight]
      .active = YES;
  if (base::FeatureList::IsEnabled(kToolbarNewTabButton)) {
    [spotlightView.widthAnchor constraintEqualToConstant:kSpotlightHeight]
        .active = YES;
  } else {
    [self.widthAnchor constraintEqualToAnchor:spotlightView.widthAnchor]
        .active = YES;
  }
  self.spotlightView = spotlightView;
}

@end
