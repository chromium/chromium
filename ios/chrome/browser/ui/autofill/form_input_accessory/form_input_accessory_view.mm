// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/form_input_accessory/form_input_accessory_view.h"

#import <QuartzCore/QuartzCore.h>

#include "base/i18n/rtl.h"
#include "base/logging.h"
#import "ios/chrome/browser/autofill/form_input_navigator.h"
#import "ios/chrome/browser/ui/image_util/image_util.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/common/colors/semantic_color_names.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Default Height for the accessory.
const CGFloat kDefaultAccessoryHeight = 44;

// The width for the white gradient UIView.
constexpr CGFloat ManualFillGradientWidth = 44;

// The margin for the white gradient UIView.
constexpr CGFloat ManualFillGradientMargin = 14;

// The spacing between the items in the navigation view.
constexpr CGFloat ManualFillNavigationItemSpacing = 4;

// The left content inset for the close button.
constexpr CGFloat ManualFillCloseButtonLeftInset = 7;

// The right content inset for the close button.
constexpr CGFloat ManualFillCloseButtonRightInset = 15;

// The height for the top and bottom sepparator lines.
constexpr CGFloat ManualFillSeparatorHeight = 0.5;

}  // namespace

NSString* const kFormInputAccessoryViewAccessibilityID =
    @"kFormInputAccessoryViewAccessibilityID";

@interface FormInputAccessoryView ()

// The navigation delegate if any.
@property(nonatomic, nullable, weak) id<FormInputAccessoryViewDelegate>
    delegate;

// Gradient layer to disolve the leading view's end.
@property(nonatomic, strong) CAGradientLayer* gradientLayer;

@property(nonatomic, weak) UIButton* previousButton;

@property(nonatomic, weak) UIButton* nextButton;

@property(nonatomic, weak) UIView* leadingView;

@end

@implementation FormInputAccessoryView

#pragma mark - Public

// Override |intrinsicContentSize| so Auto Layout hugs the content of this view.
- (CGSize)intrinsicContentSize {
  return CGSizeZero;
}

- (void)setUpWithLeadingView:(UIView*)leadingView
          customTrailingView:(UIView*)customTrailingView {
  [self setUpWithLeadingView:leadingView
          customTrailingView:customTrailingView
          navigationDelegate:nil];
}

- (void)setUpWithLeadingView:(UIView*)leadingView
          navigationDelegate:(id<FormInputAccessoryViewDelegate>)delegate {
  [self setUpWithLeadingView:leadingView
          customTrailingView:nil
          navigationDelegate:delegate];
}

- (void)layoutSubviews {
  [super layoutSubviews];
  self.gradientLayer.frame = self.gradientLayer.superlayer.bounds;
}

#pragma mark - UIInputViewAudioFeedback

- (BOOL)enableInputClicksWhenVisible {
  return YES;
}

#pragma mark - Private Methods

- (void)closeButtonTapped {
  [self.delegate formInputAccessoryViewDidTapCloseButton:self];
}

- (void)nextButtonTapped {
  [self.delegate formInputAccessoryViewDidTapNextButton:self];
}

- (void)previousButtonTapped {
  [self.delegate formInputAccessoryViewDidTapPreviousButton:self];
}

// Sets up the view with the given |leadingView|. If |delegate| is not nil,
// navigation controls are shown on the right and use |delegate| for actions.
// Else navigation controls are replaced with |customTrailingView|. If none of
// |delegate| and |customTrailingView| is set, leadingView will take all the
// space.
- (void)setUpWithLeadingView:(UIView*)leadingView
          customTrailingView:(UIView*)customTrailingView
          navigationDelegate:(id<FormInputAccessoryViewDelegate>)delegate {
  DCHECK(!self.subviews.count);  // This should only be called once.

  self.accessibilityIdentifier = kFormInputAccessoryViewAccessibilityID;

  leadingView = leadingView ?: [[UIView alloc] init];
  self.leadingView = leadingView;
  leadingView.translatesAutoresizingMaskIntoConstraints = NO;

  UIView* trailingView;
  if (delegate) {
    self.delegate = delegate;
    trailingView = [self viewForNavigationButtons];
  } else {
    trailingView = customTrailingView;
  }

  // If there is no trailing view, set the leading view as the only view and
  // return early.
  if (!trailingView) {
    [self addSubview:leadingView];
    AddSameConstraints(self, leadingView);
    return;
  }

  self.translatesAutoresizingMaskIntoConstraints = NO;
  UIView* leadingViewContainer = [[UIView alloc] init];
  leadingViewContainer.translatesAutoresizingMaskIntoConstraints = NO;
  [self addSubview:leadingViewContainer];
  [leadingViewContainer addSubview:leadingView];
  AddSameConstraints(leadingViewContainer, leadingView);

  trailingView.translatesAutoresizingMaskIntoConstraints = NO;
  [self addSubview:trailingView];

  NSLayoutConstraint* defaultHeightConstraint =
      [self.heightAnchor constraintEqualToConstant:kDefaultAccessoryHeight];
  defaultHeightConstraint.priority = UILayoutPriorityDefaultHigh;

  id<LayoutGuideProvider> layoutGuide = self.safeAreaLayoutGuide;
  [NSLayoutConstraint activateConstraints:@[
    defaultHeightConstraint,
    [leadingViewContainer.topAnchor constraintEqualToAnchor:self.topAnchor],
    [leadingViewContainer.bottomAnchor
        constraintEqualToAnchor:self.bottomAnchor],
    [leadingViewContainer.leadingAnchor
        constraintEqualToAnchor:layoutGuide.leadingAnchor],
    [trailingView.trailingAnchor
        constraintEqualToAnchor:layoutGuide.trailingAnchor],
    [trailingView.topAnchor constraintEqualToAnchor:self.topAnchor],
    [trailingView.bottomAnchor constraintEqualToAnchor:self.bottomAnchor],
  ]];

  self.backgroundColor = [UIColor colorNamed:kBackgroundColor];

  CAGradientLayer* gradientLayer = [[CAGradientLayer alloc] init];
  gradientLayer.colors = @[
    (id)[[UIColor clearColor] CGColor], (id)[[UIColor whiteColor] CGColor]
  ];
  gradientLayer.startPoint = CGPointMake(0, 0.5);
  gradientLayer.endPoint = CGPointMake(0.6, 0.5);
  self.gradientLayer = gradientLayer;

  UIView* gradientView = [[UIView alloc] init];
  gradientView.userInteractionEnabled = NO;
  gradientView.backgroundColor = [UIColor colorNamed:kBackgroundColor];
  gradientView.layer.mask = gradientLayer;
  gradientView.translatesAutoresizingMaskIntoConstraints = NO;
  if (base::i18n::IsRTL()) {
    gradientView.transform = CGAffineTransformMakeRotation(M_PI);
  }
  [self insertSubview:gradientView belowSubview:trailingView];

  UIView* topGrayLine = [[UIView alloc] init];
  topGrayLine.backgroundColor = [UIColor colorNamed:kGrey50Color];
  topGrayLine.translatesAutoresizingMaskIntoConstraints = NO;
  [self addSubview:topGrayLine];

  UIView* bottomGrayLine = [[UIView alloc] init];
  bottomGrayLine.backgroundColor = [UIColor colorNamed:kGrey50Color];
  bottomGrayLine.translatesAutoresizingMaskIntoConstraints = NO;
  [self addSubview:bottomGrayLine];

  [NSLayoutConstraint activateConstraints:@[
    [topGrayLine.topAnchor constraintEqualToAnchor:self.topAnchor],
    [topGrayLine.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
    [topGrayLine.trailingAnchor constraintEqualToAnchor:self.trailingAnchor],
    [topGrayLine.heightAnchor
        constraintEqualToConstant:ManualFillSeparatorHeight],

    [bottomGrayLine.bottomAnchor constraintEqualToAnchor:self.bottomAnchor],
    [bottomGrayLine.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
    [bottomGrayLine.trailingAnchor constraintEqualToAnchor:self.trailingAnchor],
    [bottomGrayLine.heightAnchor
        constraintEqualToConstant:ManualFillSeparatorHeight],

    [gradientView.topAnchor constraintEqualToAnchor:trailingView.topAnchor],
    [gradientView.bottomAnchor
        constraintEqualToAnchor:trailingView.bottomAnchor],
    [gradientView.widthAnchor
        constraintEqualToConstant:ManualFillGradientWidth],
    [gradientView.trailingAnchor
        constraintEqualToAnchor:trailingView.leadingAnchor
                       constant:ManualFillGradientMargin],

    [leadingViewContainer.trailingAnchor
        constraintEqualToAnchor:trailingView.leadingAnchor],
  ]];
}

// Returns a view that shows navigation buttons.
- (UIView*)viewForNavigationButtons {
  UIButton* previousButton = [UIButton buttonWithType:UIButtonTypeSystem];
  [previousButton setImage:[UIImage imageNamed:@"mf_arrow_up"]
                  forState:UIControlStateNormal];
  [previousButton addTarget:self
                     action:@selector(previousButtonTapped)
           forControlEvents:UIControlEventTouchUpInside];
  previousButton.enabled = NO;
  NSString* previousButtonAccessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_AUTOFILL_ACCNAME_PREVIOUS_FIELD);
  [previousButton setAccessibilityLabel:previousButtonAccessibilityLabel];

  UIButton* nextButton = [UIButton buttonWithType:UIButtonTypeSystem];
  [nextButton setImage:[UIImage imageNamed:@"mf_arrow_down"]
              forState:UIControlStateNormal];
  [nextButton addTarget:self
                 action:@selector(nextButtonTapped)
       forControlEvents:UIControlEventTouchUpInside];
  nextButton.enabled = NO;
  NSString* nextButtonAccessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_AUTOFILL_ACCNAME_NEXT_FIELD);
  [nextButton setAccessibilityLabel:nextButtonAccessibilityLabel];

  UIButton* closeButton = [UIButton buttonWithType:UIButtonTypeSystem];
  NSString* title = l10n_util::GetNSString(IDS_IOS_AUTOFILL_INPUT_BAR_DONE);
  [closeButton setTitle:title forState:UIControlStateNormal];
  [closeButton addTarget:self
                  action:@selector(closeButtonTapped)
        forControlEvents:UIControlEventTouchUpInside];
  closeButton.contentEdgeInsets = UIEdgeInsetsMake(
      0, ManualFillCloseButtonLeftInset, 0, ManualFillCloseButtonRightInset);
  NSString* closeButtonAccessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_AUTOFILL_ACCNAME_HIDE_KEYBOARD);
  [closeButton setAccessibilityLabel:closeButtonAccessibilityLabel];

  self.nextButton = nextButton;
  self.previousButton = previousButton;

  UIStackView* navigationView = [[UIStackView alloc]
      initWithArrangedSubviews:@[ previousButton, nextButton, closeButton ]];
  navigationView.spacing = ManualFillNavigationItemSpacing;
  return navigationView;
}

@end
