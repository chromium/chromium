// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/open_in/open_in_toolbar.h"

#import <cmath>

#import "base/check.h"
#import "base/notreached.h"
#import "ios/chrome/browser/ui/util/named_guide.h"
#import "ios/chrome/browser/ui/util/rtl_geometry.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// The toolbar's open button constants.
const CGFloat kOpenButtonTrailingPadding = 16.0f;
const CGFloat kOpenButtonHeight = 49.0f;
// The toolbar's border related constants.
const CGFloat kTopBorderHeight = 0.5f;

}  // anonymous namespace

@interface OpenInToolbar ()

// The "Open in..." button that's hooked up with the target and action passed
// on initialization.
@property(nonatomic, strong, readonly) UIButton* openButton;

// The line used as the border at the top of the toolbar.
@property(nonatomic, strong, readonly) UIView* topBorder;

// The bottomAnchor constraint of the openButton.
@property(nonatomic, strong) NSLayoutConstraint* openButtonBottomConstraint;

@end

@implementation OpenInToolbar

- (instancetype)initWithTarget:(id)target action:(SEL)action {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    DCHECK([target respondsToSelector:action]);
    self.backgroundColor = [UIColor colorNamed:kBackgroundColor];

    _openButton = [self openButtonWithTarget:target action:action];
    _topBorder = [self topBorderView];

    [self addSubview:_openButton];
    [self addSubview:_topBorder];

    [NSLayoutConstraint activateConstraints:@[
      [_openButton.trailingAnchor
          constraintEqualToAnchor:self.safeAreaLayoutGuide.trailingAnchor
                         constant:-kOpenButtonTrailingPadding],
      [_openButton.leadingAnchor
          constraintGreaterThanOrEqualToAnchor:self.safeAreaLayoutGuide
                                                   .leadingAnchor
                                      constant:kOpenButtonTrailingPadding],
      [_openButton.heightAnchor
          constraintGreaterThanOrEqualToConstant:kOpenButtonHeight],
      [_openButton.topAnchor
          constraintEqualToAnchor:self.safeAreaLayoutGuide.topAnchor],
      [_topBorder.heightAnchor constraintEqualToConstant:kTopBorderHeight],
    ]];

    AddSameConstraintsToSides(
        _topBorder, self,
        LayoutSides::kTop | LayoutSides::kLeading | LayoutSides::kTrailing);
    self.translatesAutoresizingMaskIntoConstraints = NO;
  }
  return self;
}

#pragma mark Helper

// Helper to create the OpenIn button.
- (UIButton*)openButtonWithTarget:(id)target action:(SEL)action {
  UIButton* openButton = [[UIButton alloc] init];
  [openButton setTitleColor:[UIColor colorNamed:kBlueColor]
                   forState:UIControlStateNormal];
  [openButton setTitle:l10n_util::GetNSString(IDS_IOS_OPEN_IN)
              forState:UIControlStateNormal];
  [openButton sizeToFit];
  openButton.translatesAutoresizingMaskIntoConstraints = NO;

  [openButton addTarget:target
                 action:action
       forControlEvents:UIControlEventTouchUpInside];

  return openButton;
}

// Helper to create the topBorder view.
- (UIView*)topBorderView {
  UIView* topBorder = [[UIView alloc] initWithFrame:CGRectZero];
  topBorder.backgroundColor = [UIColor colorNamed:kToolbarShadowColor];
  topBorder.translatesAutoresizingMaskIntoConstraints = NO;

  return topBorder;
}

// Helper to update constraints.
- (void)updateToolbarConstraints {
  if (!self.superview)
    return;
  NamedGuide* guide = [NamedGuide guideWithName:kSecondaryToolbarGuide
                                           view:self];
  [NSLayoutConstraint activateConstraints:@[
    [self.leadingAnchor constraintEqualToAnchor:self.superview.leadingAnchor],
    [self.trailingAnchor constraintEqualToAnchor:self.superview.trailingAnchor],
    [self.bottomAnchor constraintEqualToAnchor:self.superview.bottomAnchor],
  ]];

  self.openButtonBottomConstraint.active = NO;
  if (guide) {
    self.openButtonBottomConstraint =
        [self.openButton.bottomAnchor constraintEqualToAnchor:guide.topAnchor];
  } else {
    self.openButtonBottomConstraint = [self.openButton.bottomAnchor
        constraintEqualToAnchor:self.superview.safeAreaLayoutGuide
                                    .bottomAnchor];
  }
  self.openButtonBottomConstraint.active = YES;
}

#pragma mark UIView

- (void)didMoveToSuperview {
  [super didMoveToSuperview];
  [self updateToolbarConstraints];
}

- (void)didMoveToWindow {
  [super didMoveToWindow];
  [self updateToolbarConstraints];
}

@end
