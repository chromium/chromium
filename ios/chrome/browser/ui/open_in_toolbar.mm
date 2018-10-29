// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/open_in_toolbar.h"

#include <cmath>

#include "base/logging.h"
#import "ios/chrome/browser/ui/colors/MDCPalette+CrAdditions.h"
#import "ios/chrome/browser/ui/util/named_guide.h"
#include "ios/chrome/browser/ui/util/rtl_geometry.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/third_party/material_components_ios/src/components/Buttons/src/MaterialButtons.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// The toolbar's open button constants.
const CGFloat kOpenButtonVerticalPadding = 8.0f;
const CGFloat kOpenButtonTrailingPadding = 16.0f;

// The toolbar's border related constants.
const CGFloat kTopBorderHeight = 0.5f;
const CGFloat kTopBorderTransparency = 0.13f;
const int kTopBorderColor = 0x000000;

// The toolbar's background related constants.
const int kToolbarBackgroundColor = 0xFFFFFF;
const CGFloat kToolbarBackgroundTransparency = 0.97f;

}  // anonymous namespace

@interface OpenInToolbar () {
  // Backing object for |self.openButton|.
  MDCButton* _openButton;
  // Backing object for |self.topBorder|.
  UIView* _topBorder;
}

// The "Open in..." button that's hooked up with the target and action passed
// on initialization.
@property(nonatomic, retain, readonly) MDCButton* openButton;

// The line used as the border at the top of the toolbar.
@property(nonatomic, retain, readonly) UIView* topBorder;

// View used to have a bottom margin to prevent overlapping with the bottom
// toolbar. This view is used because the the CRWWebControllerContainerView is
// not using autolayout so the toolbar cannot use autolayout either. See
// https://crbug.com/857371.
@property(nonatomic, strong) UIView* bottomMargin;

// Relayout the frame of the Open In toolbar, based on its superview and the
// bottom margin.
- (void)relayout;

@end

// TODO(crbug.com/857371): Remove this once the OpenInToolbar is positioned with
// autolayout.
// This view is used to have a view the same height as the bottom toolbar as the
// OpenInToolbar cannot be positioned using autolayout. The height of this view
// should be constrained to the height of the bottom toolbar. When this view is
// re-layouted, its is asking the toolbar to relayout its frame.
@interface OpenInBottomMargin : UIView
// The OpenInToolbar that is notified when the bottom toolbar margin is changed.
@property(nonatomic, weak) OpenInToolbar* owner;
@end

@implementation OpenInBottomMargin
@synthesize owner = _owner;

- (void)layoutSubviews {
  [super layoutSubviews];
  // This call also happens when the height of this view is changed. And its
  // height is the same as the height of the bottom toolbar. So this allows to
  // monitor the height changes in the bottom toolbar. Calling relayout on the
  // OpenInToolbar ensures that it is correctly positioned.
  [self.owner relayout];
}

@end

@implementation OpenInToolbar

@synthesize bottomMargin = _bottomMargin;
@synthesize bottomMarginConstraint = _bottomMarginConstraint;

- (instancetype)initWithFrame:(CGRect)aRect {
  NOTREACHED();
  return nil;
}

- (instancetype)initWithTarget:(id)target action:(SEL)action {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    DCHECK([target respondsToSelector:action]);
    [self setBackgroundColor:UIColorFromRGB(kToolbarBackgroundColor,
                                            kToolbarBackgroundTransparency)];
    [self addSubview:self.openButton];
    [self.openButton addTarget:target
                        action:action
              forControlEvents:UIControlEventTouchUpInside];
    [self addSubview:self.topBorder];

    self.topBorder.translatesAutoresizingMaskIntoConstraints = NO;
    self.openButton.translatesAutoresizingMaskIntoConstraints = NO;

    [NSLayoutConstraint activateConstraints:@[
      [self.openButton.trailingAnchor
          constraintEqualToAnchor:self.trailingAnchor
                         constant:-kOpenButtonTrailingPadding],
      [self.openButton.leadingAnchor
          constraintGreaterThanOrEqualToAnchor:self.leadingAnchor
                                      constant:kOpenButtonTrailingPadding],
      [self.openButton.topAnchor
          constraintEqualToAnchor:self.topAnchor
                         constant:kOpenButtonVerticalPadding],
      [self.topBorder.heightAnchor constraintEqualToConstant:kTopBorderHeight],
    ]];

    AddSameConstraintsToSides(
        self.topBorder, self,
        LayoutSides::kTop | LayoutSides::kLeading | LayoutSides::kTrailing);

    OpenInBottomMargin* bottom = [[OpenInBottomMargin alloc] init];
    bottom.owner = self;
    bottom.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:bottom];
    self.bottomMargin = bottom;
  }
  return self;
}

#pragma mark Accessors

- (MDCButton*)openButton {
  if (!_openButton) {
    _openButton = [[MDCFlatButton alloc] init];
    [_openButton setTitleColor:[[MDCPalette cr_bluePalette] tint500]
                      forState:UIControlStateNormal];
    [_openButton setTitle:l10n_util::GetNSStringWithFixup(IDS_IOS_OPEN_IN)
                 forState:UIControlStateNormal];
    [_openButton sizeToFit];
  }
  return _openButton;
}

- (UIView*)topBorder {
  if (!_topBorder) {
    _topBorder = [[UIView alloc] initWithFrame:CGRectZero];
    [_topBorder setBackgroundColor:UIColorFromRGB(kTopBorderColor,
                                                  kTopBorderTransparency)];
  }
  return _topBorder;
}

#pragma mark Layout

- (CGSize)sizeThatFits:(CGSize)size {
  CGSize openButtonSize = [self.openButton sizeThatFits:size];
  CGFloat requiredHeight =
      openButtonSize.height + 2.0 * kOpenButtonVerticalPadding;
  return CGSizeMake(size.width, requiredHeight);
}

- (void)didMoveToSuperview {
  if (!self.superview) {
    self.bottomMarginConstraint = nil;
    return;
  }

  self.bottomMarginConstraint.active = NO;
  // If the bottom toolbar isn't present on screen, guide is nil and then
  // bottomMarginConstraint is also nil.
  NamedGuide* guide =
      [NamedGuide guideWithName:kSecondaryToolbarGuide view:self];
  self.bottomMarginConstraint = [guide.heightAnchor
      constraintEqualToAnchor:self.bottomMargin.heightAnchor];
  self.bottomMarginConstraint.active = YES;

  [self relayout];
}

- (void)relayout {
  CGRect frame = self.superview.frame;
  CGSize sizeThatFits = [self sizeThatFits:frame.size];
  CGFloat toolbarHeight =
      sizeThatFits.height + self.bottomMargin.frame.size.height;
  frame.origin.y = frame.size.height - toolbarHeight;
  frame.size.height = toolbarHeight;

  CGFloat xDiff = std::fabs(frame.origin.x - self.frame.origin.x);
  CGFloat yDiff = std::fabs(frame.origin.y - self.frame.origin.y);
  CGFloat widthDiff = std::fabs(frame.size.width - self.frame.size.width);
  CGFloat heightDiff = std::fabs(frame.size.height - self.frame.size.height);

  if (xDiff <= std::numeric_limits<CGFloat>::epsilon() &&
      yDiff <= std::numeric_limits<CGFloat>::epsilon() &&
      widthDiff <= std::numeric_limits<CGFloat>::epsilon() &&
      heightDiff <= std::numeric_limits<CGFloat>::epsilon()) {
    // Don't update the frame if it is close enough to prevent potential
    // infinite layout cycle.
    return;
  }

  self.frame = frame;
}

@end
