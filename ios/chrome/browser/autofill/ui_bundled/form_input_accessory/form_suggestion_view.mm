// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/form_input_accessory/form_suggestion_view.h"

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/i18n/rtl.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/strings/strcat.h"
#import "components/autofill/core/browser/filling/filling_product.h"
#import "components/autofill/core/browser/suggestions/suggestion_type.h"
#import "components/autofill/ios/browser/form_suggestion.h"
#import "ios/chrome/browser/autofill/model/form_suggestion_client.h"
#import "ios/chrome/browser/autofill/model/form_suggestion_constants.h"
#import "ios/chrome/browser/autofill/ui_bundled/form_input_accessory/form_suggestion_label.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/rtl_geometry.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/elements/form_input_accessory_view.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

using autofill::FillingProduct;
using autofill::SuggestionType;
using base::UmaHistogramSparse;

namespace {

// Vertical margin between suggestions and the edge of the suggestion content
// frame.
constexpr CGFloat kSuggestionVerticalMargin = 6;

// Horizontal margin around suggestions (i.e. between suggestions, and between
// the end suggestions and the suggestion content frame).
constexpr CGFloat kSuggestionHorizontalMargin = 6;

// Horizontal space between suggestions.
constexpr CGFloat kSpacing = 4;

// Horizontal margin at the end of the last suggestion.
constexpr CGFloat kSuggestionEndHorizontalMargin = 10;

// Initial spacing between suggestion chips at the beginning of the scroll hint
// animation.
constexpr CGFloat kScrollHintInitialSpacing = 50;

// The amount of time (in seconds) the scroll hint animation takes.
constexpr CGFloat kScrollHintDuration = 0.5;

// Leading horizontal offset.
constexpr CGFloat kLeadingOffset = 16;

// Top and bottom padding when using liquid glass.
constexpr CGFloat kLiquidGlassVerticalPadding = 10;

// Width of the suggestion separator when using liquid glass.
constexpr CGFloat kLiquidGlassSeparatorWidth = 1.0;

// Logs the right histogram when a suggestion from the keyboard accessory is
// selected. `suggestion_type` is the type of the selected suggestion and
// `index` is the position of the selected position among the available
// suggestions.
void LogSelectedSuggestionIndexMetric(SuggestionType suggestion_type,
                                      NSInteger index) {
  FillingProduct filling_product =
      GetFillingProductFromSuggestionType(suggestion_type);
  std::string filling_product_bucket;
  switch (filling_product) {
    case FillingProduct::kCreditCard:
    case FillingProduct::kIban:
    case FillingProduct::kLoyaltyCard:
    case FillingProduct::kAddress:
    case FillingProduct::kPlusAddresses:
    case FillingProduct::kPassword:
    case FillingProduct::kNone:
    case FillingProduct::kAutocomplete:
    case FillingProduct::kDataList:
      filling_product_bucket = FillingProductToString(filling_product);
      break;
    case FillingProduct::kPasskey:
    case FillingProduct::kCompose:
    case FillingProduct::kAutofillAi:
    case FillingProduct::kMerchantPromoCode:
    case FillingProduct::kIdentityCredential:
    case FillingProduct::kOneTimePassword:
      // These cases are currently not available on iOS.
      NOTREACHED();
  }
  UmaHistogramSparse(
      base::StrCat({"Autofill.UserAcceptedSuggestionAtIndex.",
                    filling_product_bucket, ".KeyboardAccessory"}),
      index);
}

}  // namespace

@interface FormSuggestionView () <FormSuggestionLabelDelegate>

// The FormSuggestions that are displayed by this view.
@property(nonatomic) NSArray<FormSuggestion*>* suggestions;

// The stack view with the suggestions.
@property(nonatomic) UIStackView* stackView;

// The view containing accessory buttons at the trailing end,
// after the form suggestion view.
@property(nonatomic, weak) UIView* accessoryTrailingView;

@end

@implementation FormSuggestionView {
  // Whether the size of the accessory is compact.
  BOOL _isCompact;
}

#pragma mark - Public

- (instancetype)init {
  if ((self = [super init])) {
    _isCompact = YES;
  }
  return self;
}

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

- (void)setIsCompact:(BOOL)isCompact {
  if (_isCompact == isCompact) {
    return;
  }

  _isCompact = isCompact;
  self.stackView.layoutMargins = [self adjustedLayoutMargins];
  if (self.window) {
    [self layoutIfNeeded];
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
  NSUInteger index = formSuggestionLabel.suggestionIndex;
  FormSuggestion* suggestion = formSuggestionLabel.suggestion;
  LogSelectedSuggestionIndexMetric(suggestion.type, index);
  base::RecordAction(base::UserMetricsAction(
      suggestion.type == SuggestionType::kBackupPasswordEntry
          ? "KeyboardAccessory_SuggestionAccepted_BackupPassword"
          : "KeyboardAccessory_SuggestionAccepted"));
  [self.formSuggestionViewDelegate formSuggestionView:self
                                  didAcceptSuggestion:suggestion
                                              atIndex:index];
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
  stackView.layoutMargins = [self adjustedLayoutMargins];
  stackView.spacing = kSpacing;
  stackView.translatesAutoresizingMaskIntoConstraints = NO;
  [self addSubview:stackView];
  if (IsLiquidGlassEffectEnabled()) {
    AddSameConstraintsToSides(
        stackView, self,
        LayoutSides::kTop | LayoutSides::kLeading | LayoutSides::kTrailing);
  } else {
    AddSameConstraints(stackView, self);
  }
  [stackView.heightAnchor constraintEqualToAnchor:self.heightAnchor].active =
      YES;

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

// Creates a tiny vertical separator.
- (UIView*)createSeparatorView {
  UIView* wrapperContainer = [[UIView alloc] init];
  wrapperContainer.translatesAutoresizingMaskIntoConstraints = NO;
  UIView* separator = [[UIView alloc] init];
  separator.backgroundColor = [UIColor colorNamed:kSeparatorColor];
  separator.translatesAutoresizingMaskIntoConstraints = NO;
  [wrapperContainer addSubview:separator];
  [NSLayoutConstraint activateConstraints:@[
    [separator.widthAnchor
        constraintEqualToConstant:kLiquidGlassSeparatorWidth],
    [separator.bottomAnchor
        constraintEqualToAnchor:wrapperContainer.bottomAnchor
                       constant:-kLiquidGlassVerticalPadding],
    [separator.topAnchor constraintEqualToAnchor:wrapperContainer.topAnchor
                                        constant:kLiquidGlassVerticalPadding],
  ]];
  return wrapperContainer;
}

// Adds a FormSuggestionLabel to this FormSuggestionView's stack view.
- (void)addFormSuggestionLabel:(FormSuggestionLabel*)label
                       atIndex:(NSUInteger)idx {
  if (IsLiquidGlassEffectEnabled()) {
    if (idx > 0) {
      [self.stackView addArrangedSubview:[self createSeparatorView]];
    }

    // This constraint is added to ensure that the keyboard accessory's
    // suggestion label maintains its height when a hardware keyboard is
    // connected and the keyboard accessory is located at the bottom of the
    // screen. Without this constraint, the label's height is reduced and looks
    // squeezed.
    [label.heightAnchor
        constraintEqualToConstant:kLargeKeyboardAccessoryHeight -
                                  (2 * kSuggestionVerticalMargin)]
        .active = YES;
  }

  [self.stackView addArrangedSubview:label];
}

// Creates a FormSuggestionLabel for each suggestion and adds them to this
// FormSuggestionView's stack view, along with the trailing view, if any.
- (void)createAndInsertArrangedSubviews {
  auto setupBlock = ^(FormSuggestion* suggestion, NSUInteger idx, BOOL* stop) {
    FormSuggestionLabel* label = [[FormSuggestionLabel alloc]
           initWithSuggestion:suggestion
                        index:idx
          numberOfSuggestions:[self.suggestions count]
        accessoryTrailingView:self.accessoryTrailingView
                     delegate:self];
    [self addFormSuggestionLabel:label atIndex:idx];
    if (idx == 0 &&
        suggestion.featureForIPH != SuggestionFeatureForIPH::kUnknown) {
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
  if (!self.stackView.arrangedSubviews.count) {
    if (completion) {
      completion(NO);
    }
    return;
  }

  // Check if the view is in the current hierarchy before performing layouts.
  if (self.stackView.window) {
    // Make sure all subview layouts are done before computing frame offsets.
    for (UIView* view in self.stackView.arrangedSubviews) {
      [view layoutIfNeeded];
    }
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

// Adjust origin based on whether the keyboard accessory is in compact mode.
- (UIEdgeInsets)adjustedLayoutMargins {
  // Because of the way the scroll view is transformed for RTL, the insets don't
  // need to be directed.
  return UIEdgeInsetsMake(
      kSuggestionVerticalMargin,
      kSuggestionHorizontalMargin + (_isCompact ? 0.0 : kLeadingOffset),
      kSuggestionVerticalMargin, kSuggestionEndHorizontalMargin);
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

@end
