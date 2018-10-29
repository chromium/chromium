// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_grid/tab_grid_bottom_toolbar.h"

#include "base/i18n/rtl.h"
#import "ios/chrome/browser/ui/tab_grid/tab_grid_constants.h"
#import "ios/chrome/browser/ui/tab_grid/tab_grid_new_tab_button.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation TabGridBottomToolbar
@synthesize leadingButton = _leadingButton;
@synthesize trailingButton = _trailingButton;
@synthesize centerButton = _centerButton;

#pragma mark - UIView

- (CGSize)intrinsicContentSize {
  return CGSizeMake(UIViewNoIntrinsicMetric, kTabGridBottomToolbarHeight);
}

- (void)willMoveToSuperview:(UIView*)newSuperview {
  // The first time this moves to a superview, perform the view setup.
  if (newSuperview && self.subviews.count == 0) {
    [self setupViews];
  }
}

#pragma mark - Private

- (void)setupViews {
  self.layoutMargins = UIEdgeInsetsMake(0, kTabGridToolbarHorizontalInset, 0,
                                        kTabGridToolbarHorizontalInset);

  UIVisualEffect* blurEffect =
      [UIBlurEffect effectWithStyle:UIBlurEffectStyleDark];
  UIVisualEffectView* toolbar =
      [[UIVisualEffectView alloc] initWithEffect:blurEffect];
  toolbar.translatesAutoresizingMaskIntoConstraints = NO;
  [self addSubview:toolbar];

  UIButton* leadingButton = [UIButton buttonWithType:UIButtonTypeSystem];
  leadingButton.translatesAutoresizingMaskIntoConstraints = NO;
  leadingButton.tintColor = UIColorFromRGB(kTabGridToolbarTextButtonColor);
  leadingButton.titleLabel.lineBreakMode = NSLineBreakByClipping;
  UIButton* trailingButton = [UIButton buttonWithType:UIButtonTypeSystem];
  trailingButton.translatesAutoresizingMaskIntoConstraints = NO;

  if (@available(iOS 11, *)) {
    leadingButton.contentHorizontalAlignment =
        UIControlContentHorizontalAlignmentLeading;
    trailingButton.contentHorizontalAlignment =
        UIControlContentHorizontalAlignmentTrailing;
  } else if (base::i18n::IsRTL()) {
    leadingButton.contentHorizontalAlignment =
        UIControlContentHorizontalAlignmentRight;
    trailingButton.contentHorizontalAlignment =
        UIControlContentHorizontalAlignmentLeft;
  } else {
    leadingButton.contentHorizontalAlignment =
        UIControlContentHorizontalAlignmentLeft;
    trailingButton.contentHorizontalAlignment =
        UIControlContentHorizontalAlignmentRight;
  }

  trailingButton.tintColor = UIColorFromRGB(kTabGridToolbarTextButtonColor);
  TabGridNewTabButton* centerButton = [TabGridNewTabButton
      buttonWithSizeClass:TabGridNewTabButtonSizeClassSmall];
  centerButton.translatesAutoresizingMaskIntoConstraints = NO;

  [toolbar.contentView addSubview:leadingButton];
  [toolbar.contentView addSubview:trailingButton];
  [toolbar.contentView addSubview:centerButton];
  _leadingButton = leadingButton;
  _trailingButton = trailingButton;
  _centerButton = centerButton;

  NSArray* constraints = @[
    [toolbar.topAnchor constraintEqualToAnchor:self.topAnchor],
    [toolbar.bottomAnchor constraintEqualToAnchor:self.bottomAnchor],
    [toolbar.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
    [toolbar.trailingAnchor constraintEqualToAnchor:self.trailingAnchor],
    [leadingButton.heightAnchor
        constraintEqualToConstant:kTabGridBottomToolbarHeight],
    [leadingButton.leadingAnchor
        constraintEqualToAnchor:self.layoutMarginsGuide.leadingAnchor],
    [leadingButton.topAnchor constraintEqualToAnchor:toolbar.topAnchor],
    [centerButton.centerXAnchor constraintEqualToAnchor:toolbar.centerXAnchor],
    [centerButton.centerYAnchor
        constraintEqualToAnchor:toolbar.topAnchor
                       constant:kTabGridBottomToolbarHeight / 2.0f],
    [trailingButton.heightAnchor
        constraintEqualToConstant:kTabGridBottomToolbarHeight],
    [trailingButton.leadingAnchor
        constraintEqualToAnchor:centerButton.trailingAnchor],
    [trailingButton.trailingAnchor
        constraintEqualToAnchor:self.layoutMarginsGuide.trailingAnchor],
    [trailingButton.topAnchor constraintEqualToAnchor:toolbar.topAnchor],
  ];
  [NSLayoutConstraint activateConstraints:constraints];
}

@end
