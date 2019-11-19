// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/form_suggestion_view.h"

#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "components/autofill/core/browser/ui/popup_item_ids.h"
#import "components/autofill/ios/browser/form_suggestion.h"
#import "ios/chrome/browser/autofill/form_suggestion_client.h"
#import "ios/chrome/browser/autofill/form_suggestion_constants.h"
#import "ios/chrome/browser/autofill/form_suggestion_label.h"
#include "ios/chrome/browser/ui/util/rtl_geometry.h"
#include "ios/chrome/common/ui_util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Vertical margin between suggestions and the edge of the suggestion content
// frame.
const CGFloat kSuggestionVerticalMargin = 6;

// Horizontal margin around suggestions (i.e. between suggestions, and between
// the end suggestions and the suggestion content frame).
const CGFloat kSuggestionHorizontalMargin = 6;

}  // namespace

@interface FormSuggestionView () <UIScrollViewDelegate>

// The FormSuggestions that are displayed by this view.
@property(nonatomic) NSArray<FormSuggestion*>* suggestions;

// The stack view with the suggestions.
@property(nonatomic) UIStackView* stackView;

// Handles user interactions.
@property(nonatomic, weak) id<FormSuggestionClient> client;

@end

@implementation FormSuggestionView

#pragma mark - Public

- (void)updateClient:(id<FormSuggestionClient>)client
         suggestions:(NSArray<FormSuggestion*>*)suggestions {
  if ([self.suggestions isEqualToArray:suggestions] &&
      (self.client == client || !suggestions.count)) {
    return;
  }
  self.client = client;
  self.suggestions = [suggestions copy];

  if (self.stackView) {
    for (UIView* view in [self.stackView.arrangedSubviews copy]) {
      [self.stackView removeArrangedSubview:view];
      [view removeFromSuperview];
    }
    self.contentInset = UIEdgeInsetsZero;
    [self createAndInsertArrangedSubviews];
  }
}

- (void)resetContentInsetAndDelegateAnimated:(BOOL)animated {
  self.delegate = nil;
  [UIView animateWithDuration:animated ? 0.2 : 0.0
                   animations:^{
                     self.contentInset = UIEdgeInsetsZero;
                   }];
}

- (void)lockTrailingView {
  if (!self.superview || !self.trailingView) {
    return;
  }
  LayoutOffset layoutOffset = CGRectGetLeadingLayoutOffsetInBoundingRect(
      self.trailingView.frame, {CGPointZero, self.contentSize});
  // Because the way the scroll view is transformed for RTL, the insets don't
  // need to be directed.
  UIEdgeInsets lockedContentInsets = UIEdgeInsetsMake(0, -layoutOffset, 0, 0);
  [UIView animateWithDuration:0.2
      animations:^{
        self.contentInset = lockedContentInsets;
      }
      completion:^(BOOL finished) {
        self.delegate = self;
      }];
}

#pragma mark - UIView

- (void)willMoveToSuperview:(UIView*)newSuperview {
  // Create and add subviews the first time this moves to a superview.
  if (newSuperview && self.subviews.count == 0) {
    [self setupSubviews];
  }
  [super willMoveToSuperview:newSuperview];
}

#pragma mark - Helper methods

// Creates and adds subviews.
- (void)setupSubviews {
  self.showsVerticalScrollIndicator = NO;
  self.showsHorizontalScrollIndicator = NO;
  self.canCancelContentTouches = YES;
  self.alwaysBounceHorizontal = YES;

  UIStackView* stackView = [[UIStackView alloc] initWithArrangedSubviews:@[]];
  stackView.axis = UILayoutConstraintAxisHorizontal;
  stackView.layoutMarginsRelativeArrangement = YES;
  stackView.layoutMargins =
      UIEdgeInsetsMake(kSuggestionVerticalMargin, kSuggestionHorizontalMargin,
                       kSuggestionVerticalMargin, kSuggestionHorizontalMargin);
  stackView.spacing = kSuggestionHorizontalMargin;
  stackView.translatesAutoresizingMaskIntoConstraints = NO;
  [self addSubview:stackView];
  AddSameConstraints(stackView, self);
  [stackView.heightAnchor constraintEqualToAnchor:self.heightAnchor].active =
      true;

  // Rotate the UIScrollView and its UIStackView subview 180 degrees so that the
  // first suggestion actually shows up first.
  if (base::i18n::IsRTL()) {
    self.transform = CGAffineTransformMakeRotation(M_PI);
    stackView.transform = CGAffineTransformMakeRotation(M_PI);
  }
  self.stackView = stackView;
  [self createAndInsertArrangedSubviews];

  self.accessibilityIdentifier = kFormSuggestionsViewAccessibilityIdentifier;
}

- (void)createAndInsertArrangedSubviews {
  auto setupBlock = ^(FormSuggestion* suggestion, NSUInteger idx, BOOL* stop) {
    UIView* label =
        [[FormSuggestionLabel alloc] initWithSuggestion:suggestion
                                                  index:idx
                                         numSuggestions:[self.suggestions count]
                                                 client:self.client];
    [self.stackView addArrangedSubview:label];
  };
  [self.suggestions enumerateObjectsUsingBlock:setupBlock];
  if (self.trailingView) {
    [self.stackView addArrangedSubview:self.trailingView];
  }
}

#pragma mark - Setters

- (void)setTrailingView:(UIView*)subview {
  if (_trailingView.superview) {
    [_stackView removeArrangedSubview:_trailingView];
    [_trailingView removeFromSuperview];
  }
  _trailingView = subview;
  if (_stackView) {
    [_stackView addArrangedSubview:_trailingView];
  }
}

- (void)scrollViewDidScroll:(UIScrollView*)scrollView {
  CGFloat offset = self.contentOffset.x;
  CGFloat inset = self.contentInset.left;  // Inset is negative when locked.
  CGFloat diff = offset + inset;
  if (diff < -55) {
    [self.formSuggestionViewDelegate
        formSuggestionViewShouldResetFromPull:self];
  }
}

@end
