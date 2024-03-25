// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/form_input_accessory/form_suggestion_view.h"

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/i18n/rtl.h"
#import "components/autofill/core/browser/ui/popup_item_ids.h"
#import "components/autofill/ios/browser/form_suggestion.h"
#import "ios/chrome/browser/autofill/model/form_suggestion_client.h"
#import "ios/chrome/browser/autofill/model/form_suggestion_constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/rtl_geometry.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/browser/ui/autofill/form_input_accessory/form_suggestion_label.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

// Vertical margin between suggestions and the edge of the suggestion content
// frame.
constexpr CGFloat kSuggestionVerticalMargin = 6;

// Horizontal margin around suggestions (i.e. between suggestions, and between
// the end suggestions and the suggestion content frame).
constexpr CGFloat kSuggestionHorizontalMargin = 6;

// Horizontal margin at the end of the last suggestion.
constexpr CGFloat kSuggestionEndHorizontalMargin = 10;

// Initial spacing between suggestion chips at the beginning of the scroll hint
// animation.
constexpr CGFloat kScrollHintInitialSpacing = 50;

// The amount of time (in seconds) the scroll hint animation takes.
constexpr CGFloat kScrollHintDuration = 0.5;

}  // namespace

@interface FormSuggestionView () <FormSuggestionLabelDelegate,
                                  UIScrollViewDelegate>

// The FormSuggestions that are displayed by this view.
@property(nonatomic) NSArray<FormSuggestion*>* suggestions;

// The stack view with the suggestions.
@property(nonatomic) UIStackView* stackView;

// The view containing accessory buttons at the trailing end,
// after the form suggestion view.
@property(nonatomic, weak) UIView* accessoryTrailingView;

@end

@implementation FormSuggestionView

#pragma mark - Public

- (void)updateSuggestions:(NSArray<FormSuggestion*>*)suggestions
           showScrollHint:(BOOL)showScrollHint
    accessoryTrailingView:(UIView*)accessoryTrailingView
               completion:(void (^)(BOOL finished))completion {
  if ([self.suggestions isEqualToArray:suggestions] && !suggestions.count) {
    if (completion) {
      completion(NO);
    }
    return;
  }
  self.suggestions = [suggestions copy];

  if (!self.stackView) {
    if (completion) {
      completion(NO);
    }
    return;
  }

  for (UIView* view in [self.stackView.arrangedSubviews copy]) {
    [self.stackView removeArrangedSubview:view];
    [view removeFromSuperview];
  }
  self.contentInset = UIEdgeInsetsZero;
  self.accessoryTrailingView = accessoryTrailingView;
  [self createAndInsertArrangedSubviews];
  [self setContentOffset:CGPointZero];
  if (showScrollHint) {
    [self scrollHint:completion];
  } else if (completion) {
    completion(NO);
  }
}

- (void)resetContentInsetAndDelegateAnimated:(BOOL)animated {
  self.delegate = nil;
  __weak __typeof(self) weakSelf = self;
  [UIView animateWithDuration:animated ? 0.2 : 0.0
                   animations:^{
                     weakSelf.contentInset = UIEdgeInsetsZero;
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
  __weak __typeof(self) weakSelf = self;
  [UIView animateWithDuration:0.2
      animations:^{
        weakSelf.contentInset = lockedContentInsets;
      }
      completion:^(BOOL finished) {
        if (!IsKeyboardAccessoryUpgradeEnabled()) {
          weakSelf.delegate = weakSelf;
        }
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

#pragma mark - FormSuggestionLabelDelegate

- (void)didTapFormSuggestionLabel:(FormSuggestionLabel*)formSuggestionLabel {
  NSUInteger index =
      [self.stackView.arrangedSubviews indexOfObject:formSuggestionLabel];
  DCHECK(index != NSNotFound);
  FormSuggestion* suggestion = [self.suggestions objectAtIndex:index];
  [self.formSuggestionViewDelegate formSuggestionView:self
                                  didAcceptSuggestion:suggestion];
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
  stackView.layoutMargins = UIEdgeInsetsMake(
      kSuggestionVerticalMargin, kSuggestionHorizontalMargin,
      kSuggestionVerticalMargin,
      IsKeyboardAccessoryUpgradeEnabled() ? kSuggestionEndHorizontalMargin
                                          : kSuggestionHorizontalMargin);
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
    UIView* label = [[FormSuggestionLabel alloc]
           initWithSuggestion:suggestion
                        index:idx
               numSuggestions:[self.suggestions count]
        accessoryTrailingView:self.accessoryTrailingView
                     delegate:self];
    [self.stackView addArrangedSubview:label];
    if (idx == 0 && suggestion.featureForIPH.length > 0) {
      // Track the first element.
      [self.layoutGuideCenter referenceView:label
                                  underName:kAutofillFirstSuggestionGuide];
    }
  };
  [self.suggestions enumerateObjectsUsingBlock:setupBlock];
  if (self.trailingView) {
    [self.stackView addArrangedSubview:self.trailingView];
  }
}

// Performs the scroll hint. This is triggered when the keyboard accessory
// initially receives suggestions.
- (void)scrollHint:(void (^)(BOOL finished))completion {
  if (!IsKeyboardAccessoryUpgradeEnabled() ||
      !self.stackView.arrangedSubviews.count) {
    if (completion) {
      completion(NO);
    }
    return;
  }

  // Make sure all the subview layouts are done before computing frame offsets.
  for (UIView* view in self.stackView.arrangedSubviews) {
    [view layoutIfNeeded];
  }

  // This creates an animation where suggestions fly in from the right.
  [self spaceSubviews:kScrollHintInitialSpacing alpha:0];
  __weak __typeof(self) weakSelf = self;
  [UIView animateWithDuration:kScrollHintDuration
                   animations:^{
                     [weakSelf spaceSubviews:-kScrollHintInitialSpacing
                                       alpha:1];
                   }
                   completion:completion];
}

// Inserts spacing between suggestion views and sets their transparency.
- (void)spaceSubviews:(CGFloat)spacing alpha:(CGFloat)alpha {
  CGFloat offset = spacing;
  for (UIView* view in self.stackView.arrangedSubviews) {
    CGRect frame = view.frame;
    frame.origin.x += offset;
    view.frame = frame;
    view.alpha = alpha;
    offset += spacing;
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

#pragma mark - UIScrollViewDelegate

- (void)scrollViewDidScroll:(UIScrollView*)scrollView {
  DCHECK(!IsKeyboardAccessoryUpgradeEnabled());

  CGFloat offset = self.contentOffset.x;
  CGFloat inset = self.contentInset.left;  // Inset is negative when locked.
  CGFloat diff = offset + inset;
  if (diff < -55) {
    [self.formSuggestionViewDelegate
        formSuggestionViewShouldResetFromPull:self];
  }
}

@end
