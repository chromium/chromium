// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/translate/translate_infobar_language_tab_view.h"

#include "base/logging.h"
#import "ios/chrome/browser/ui/colors/MDCPalette+CrAdditions.h"
#import "ios/chrome/browser/ui/translate/translate_infobar_language_tab_strip_view.h"
#import "ios/chrome/browser/ui/translate/translate_infobar_language_tab_view_delegate.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/colors/semantic_color_names.h"
#import "ios/chrome/common/highlight_button.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"
#import "ios/third_party/material_components_ios/src/components/ActivityIndicator/src/MaterialActivityIndicator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Size of the activity indicator.
const CGFloat kActivityIndicatorSize = 24;

// Radius of the activity indicator.
const CGFloat kActivityIndicatorRadius = 10;

// Duration to wait before changing visibility of subviews after they are added.
const CGFloat kUpdateSubviewsDelay = 0.1;

// Padding for contents of the button.
const CGFloat kButtonPadding = 12;

}  // namespace

@interface TranslateInfobarLanguageTabView ()

// Button subview providing the tappable area.
@property(nonatomic, weak) UIButton* button;

// Activity indicator replacing the button when in loading state.
@property(nonatomic, weak) MDCActivityIndicator* activityIndicator;

@end

@implementation TranslateInfobarLanguageTabView

#pragma mark - UIView

- (void)willMoveToSuperview:(UIView*)newSuperview {
  // Create and add subviews the first time this moves to a superview.
  if (newSuperview && !self.subviews.count) {
    [self setupSubviews];
    // Delay updating visiblity of the subviews. Otherwise the activity
    // indicator will not animate in the loading state.
    dispatch_after(
        dispatch_time(DISPATCH_TIME_NOW, kUpdateSubviewsDelay * NSEC_PER_SEC),
        dispatch_get_main_queue(), ^{
          [self updateSubviewsForState:self.state];
        });
  }
  [super willMoveToSuperview:newSuperview];
}

#pragma mark - Properties

- (void)setTitle:(NSString*)title {
  _title = title;
  [self.button setTitle:title forState:UIControlStateNormal];
}

- (void)setState:(TranslateInfobarLanguageTabViewState)state {
  _state = state;

  [self updateSubviewsForState:state];
}

#pragma mark - Private

- (void)setupSubviews {
  MDCActivityIndicator* activityIndicator = [[MDCActivityIndicator alloc] init];
  self.activityIndicator = activityIndicator;
  self.activityIndicator.translatesAutoresizingMaskIntoConstraints = NO;
  self.activityIndicator.cycleColors = @[ [UIColor colorNamed:kBlueColor] ];
  [self.activityIndicator setRadius:kActivityIndicatorRadius];
  [self addSubview:self.activityIndicator];

  [NSLayoutConstraint activateConstraints:@[
    [self.activityIndicator.heightAnchor
        constraintEqualToConstant:kActivityIndicatorSize],
  ]];
  AddSameCenterConstraints(self, self.activityIndicator);

  UIButton* button = [[HighlightButton alloc] init];
  self.button = button;
  self.button.translatesAutoresizingMaskIntoConstraints = NO;
  self.button.contentEdgeInsets = UIEdgeInsetsMake(
      kButtonPadding, kButtonPadding, kButtonPadding, kButtonPadding);
  [self.button setTitle:self.title forState:UIControlStateNormal];
  self.button.titleLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
  self.button.titleLabel.adjustsFontForContentSizeCategory = YES;
  [self.button setTitleColor:[self titleColor] forState:UIControlStateNormal];
  [self.button addTarget:self
                  action:@selector(buttonWasTapped)
        forControlEvents:UIControlEventTouchUpInside];
  [self addSubview:self.button];

  AddSameConstraints(self, self.button);
}

// Updates visibility of the subviews depending on |state|.
- (void)updateSubviewsForState:(TranslateInfobarLanguageTabViewState)state {
  // Clear UIAccessibilityTraitSelected from the accessibility traits, if any.
  self.button.accessibilityTraits &= ~UIAccessibilityTraitSelected;

  if (state == TranslateInfobarLanguageTabViewStateLoading) {
    [self.activityIndicator startAnimating];
    self.activityIndicator.hidden = NO;
    self.button.hidden = YES;
  } else {
    self.button.hidden = NO;
    self.activityIndicator.hidden = YES;
    [self.activityIndicator stopAnimating];

    self.button.accessibilityTraits |= [self accessibilityTraits];
    [self.button setTitleColor:[self titleColor] forState:UIControlStateNormal];
  }
}

// Returns the button's title color depending on the state.
- (UIColor*)titleColor {
  return self.state == TranslateInfobarLanguageTabViewStateSelected
             ? [UIColor colorNamed:kBlueColor]
             : [UIColor colorNamed:kTextSecondaryColor];
}

// Returns the button's accessibility traits depending on the state.
- (UIAccessibilityTraits)accessibilityTraits {
  return self.state == TranslateInfobarLanguageTabViewStateSelected
             ? UIAccessibilityTraitSelected
             : UIAccessibilityTraitNone;
}

- (void)buttonWasTapped {
  [self.delegate translateInfobarTabViewDidTap:self];
}

@end
