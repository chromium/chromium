// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_grid/tab_grid_top_toolbar.h"

#include "base/i18n/rtl.h"
#import "ios/chrome/browser/ui/tab_grid/tab_grid_constants.h"
#import "ios/chrome/browser/ui/tab_grid/tab_grid_page_control.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation TabGridTopToolbar
@synthesize leadingButton = _leadingButton;
@synthesize trailingButton = _trailingButton;
@synthesize pageControl = _pageControl;

#pragma mark - UIView

- (CGSize)intrinsicContentSize {
  return CGSizeMake(UIViewNoIntrinsicMetric, kTabGridTopToolbarHeight);
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

  // The segmented control has an intrinsic size.
  TabGridPageControl* pageControl = [[TabGridPageControl alloc] init];
  pageControl.translatesAutoresizingMaskIntoConstraints = NO;

  UIButton* trailingButton = [UIButton buttonWithType:UIButtonTypeSystem];
  trailingButton.translatesAutoresizingMaskIntoConstraints = NO;
  trailingButton.tintColor = UIColorFromRGB(kTabGridToolbarTextButtonColor);

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

  [toolbar.contentView addSubview:leadingButton];
  [toolbar.contentView addSubview:trailingButton];
  [toolbar.contentView addSubview:pageControl];
  _leadingButton = leadingButton;
  _trailingButton = trailingButton;
  _pageControl = pageControl;

  NSArray* constraints = @[
    [toolbar.topAnchor constraintEqualToAnchor:self.topAnchor],
    [toolbar.bottomAnchor constraintEqualToAnchor:self.bottomAnchor],
    [toolbar.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
    [toolbar.trailingAnchor constraintEqualToAnchor:self.trailingAnchor],
    [leadingButton.heightAnchor
        constraintEqualToConstant:kTabGridTopToolbarHeight],
    [leadingButton.leadingAnchor
        constraintEqualToAnchor:self.layoutMarginsGuide.leadingAnchor],
    [leadingButton.bottomAnchor constraintEqualToAnchor:toolbar.bottomAnchor],
    [pageControl.centerXAnchor constraintEqualToAnchor:toolbar.centerXAnchor],
    [pageControl.centerYAnchor
        constraintEqualToAnchor:toolbar.bottomAnchor
                       constant:-kTabGridTopToolbarHeight / 2.0f],
    [trailingButton.heightAnchor
        constraintEqualToConstant:kTabGridTopToolbarHeight],
    [trailingButton.leadingAnchor
        constraintEqualToAnchor:pageControl.trailingAnchor],
    [trailingButton.trailingAnchor
        constraintEqualToAnchor:self.layoutMarginsGuide.trailingAnchor],
    [trailingButton.bottomAnchor constraintEqualToAnchor:toolbar.bottomAnchor],
  ];
  [NSLayoutConstraint activateConstraints:constraints];
}

@end
