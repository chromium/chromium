// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/ui/elements/form_input_accessory_view.h"

#import <QuartzCore/QuartzCore.h>

#import "base/check.h"
#import "base/i18n/rtl.h"
#import "ios/chrome/common/button_configuration_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/elements/form_input_accessory_view_text_data.h"
#import "ios/chrome/common/ui/elements/gradient_view.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

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

NSString* const kFormInputAccessoryViewOmniboxTypingShieldAccessibilityID =
    @"kFormInputAccessoryViewOmniboxTypingShieldAccessibilityID";

@interface FormInputAccessoryView ()

// The navigation delegate if any.
@property(nonatomic, weak) id<FormInputAccessoryViewDelegate> delegate;

@property(nonatomic, weak) UIButton* previousButton;

@property(nonatomic, weak) UIButton* nextButton;

@property(nonatomic, weak) UIView* leadingView;

@end

@implementation FormInputAccessoryView {
  // Transparent view on the top edge to let the keyboard know about the
  // omnibox.
  UIButton* _omniboxTypingShield;
  // Height constraint used to show/hide the `omniboxTypingShield`.
  NSLayoutConstraint* _omniboxTypingShieldHeightConstraint;
  // View containing the leading and trailing buttons.
  UIView* _contentView;
}

#pragma mark - Public

// Override `intrinsicContentSize` so Auto Layout hugs the content of this view.
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

- (void)setOmniboxTypingShieldHeight:(CGFloat)typingShieldHeight {
  _omniboxTypingShieldHeightConstraint.constant = typingShieldHeight;
  [self layoutIfNeeded];
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

- (void)omniboxTypingShieldTapped {
  [self.delegate fromInputAccessoryViewDidTapOmniboxTypingShield:self];
}

// Sets up the view with the given `leadingView`. If `delegate` is not nil,
// navigation controls are shown on the right and use `delegate` for actions.
// Else navigation controls are replaced with `customTrailingView`. If none of
// `delegate` and `customTrailingView` is set, leadingView will take all the
// space.
- (void)setUpWithLeadingView:(UIView*)leadingView
          customTrailingView:(UIView*)customTrailingView
          navigationDelegate:(id<FormInputAccessoryViewDelegate>)delegate {
  DCHECK(!self.subviews.count);  // This should only be called once.

  self.accessibilityIdentifier = kFormInputAccessoryViewAccessibilityID;
  self.translatesAutoresizingMaskIntoConstraints = NO;
  self.backgroundColor = UIColor.clearColor;
  self.opaque = NO;

  _contentView = [[UIView alloc] init];
  _contentView.translatesAutoresizingMaskIntoConstraints = NO;
  _contentView.backgroundColor = [UIColor colorNamed:kBackgroundColor];
  [self addSubview:_contentView];
  AddSameConstraintsToSides(
      self, _contentView,
      LayoutSides::kLeading | LayoutSides::kTrailing | LayoutSides::kBottom);
  // Lower the top constraint as the omniboxTypingShield can be above it.
  NSLayoutConstraint* topConstraint =
      [self.topAnchor constraintEqualToAnchor:_contentView.topAnchor];
  topConstraint.priority = UILayoutPriorityRequired - 1;
  topConstraint.active = YES;

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
    [_contentView addSubview:leadingView];
    AddSameConstraints(_contentView, leadingView);
    return;
  }

  UIView* leadingViewContainer = [[UIView alloc] init];
  leadingViewContainer.translatesAutoresizingMaskIntoConstraints = NO;
  [_contentView addSubview:leadingViewContainer];
  [leadingViewContainer addSubview:leadingView];
  AddSameConstraints(leadingViewContainer, leadingView);

  trailingView.translatesAutoresizingMaskIntoConstraints = NO;
  [self addSubview:trailingView];

  NSLayoutConstraint* defaultHeightConstraint = [_contentView.heightAnchor
      constraintEqualToConstant:kDefaultAccessoryHeight];
  defaultHeightConstraint.priority = UILayoutPriorityDefaultHigh;

  id<LayoutGuideProvider> layoutGuide = self.safeAreaLayoutGuide;
  [NSLayoutConstraint activateConstraints:@[
    defaultHeightConstraint,
    [leadingViewContainer.topAnchor
        constraintEqualToAnchor:_contentView.topAnchor],
    [leadingViewContainer.bottomAnchor
        constraintEqualToAnchor:self.safeAreaLayoutGuide.bottomAnchor],
    [leadingViewContainer.leadingAnchor
        constraintEqualToAnchor:layoutGuide.leadingAnchor],
    [trailingView.trailingAnchor
        constraintEqualToAnchor:layoutGuide.trailingAnchor],
    [trailingView.topAnchor constraintEqualToAnchor:_contentView.topAnchor],
    [trailingView.bottomAnchor
        constraintEqualToAnchor:self.safeAreaLayoutGuide.bottomAnchor],
  ]];

  // Gradient view to disolve the leading view's end.
  UIView* gradientView = [[GradientView alloc]
      initWithStartColor:[[UIColor colorNamed:kBackgroundColor]
                             colorWithAlphaComponent:0]
                endColor:[UIColor colorNamed:kBackgroundColor]
              startPoint:CGPointMake(0, 0.5)
                endPoint:CGPointMake(0.6, 0.5)];

  gradientView.translatesAutoresizingMaskIntoConstraints = NO;
  if (base::i18n::IsRTL()) {
    gradientView.transform = CGAffineTransformMakeRotation(M_PI);
  }
  [_contentView insertSubview:gradientView belowSubview:trailingView];

  UIView* topGrayLine = [[UIView alloc] init];
  topGrayLine.backgroundColor = [UIColor colorNamed:kGrey50Color];
  topGrayLine.translatesAutoresizingMaskIntoConstraints = NO;
  [_contentView addSubview:topGrayLine];

  UIView* bottomGrayLine = [[UIView alloc] init];
  bottomGrayLine.backgroundColor = [UIColor colorNamed:kGrey50Color];
  bottomGrayLine.translatesAutoresizingMaskIntoConstraints = NO;
  [_contentView addSubview:bottomGrayLine];

  [NSLayoutConstraint activateConstraints:@[
    [topGrayLine.topAnchor constraintEqualToAnchor:_contentView.topAnchor],
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

  [self createOmniboxTypingShield];
}

// Returns a view that shows navigation buttons.
- (UIView*)viewForNavigationButtons {
  FormInputAccessoryViewTextData* textData =
      [self.delegate textDataforFormInputAccessoryView:self];

  UIButton* previousButton = [UIButton buttonWithType:UIButtonTypeSystem];
  [previousButton setImage:[UIImage imageNamed:@"mf_arrow_up"]
                  forState:UIControlStateNormal];
  [previousButton addTarget:self
                     action:@selector(previousButtonTapped)
           forControlEvents:UIControlEventTouchUpInside];
  previousButton.enabled = NO;
  [previousButton
      setAccessibilityLabel:textData.previousButtonAccessibilityLabel];

  UIButton* nextButton = [UIButton buttonWithType:UIButtonTypeSystem];
  [nextButton setImage:[UIImage imageNamed:@"mf_arrow_down"]
              forState:UIControlStateNormal];
  [nextButton addTarget:self
                 action:@selector(nextButtonTapped)
       forControlEvents:UIControlEventTouchUpInside];
  nextButton.enabled = NO;
  [nextButton setAccessibilityLabel:textData.nextButtonAccessibilityLabel];

  UIButton* closeButton = [UIButton buttonWithType:UIButtonTypeSystem];
  [closeButton setTitle:textData.closeButtonTitle
               forState:UIControlStateNormal];
  [closeButton addTarget:self
                  action:@selector(closeButtonTapped)
        forControlEvents:UIControlEventTouchUpInside];

  // TODO(crbug.com/1418068): Replace with UIButtonConfiguration when min
  // deployment target is iOS 15.
  UIEdgeInsets contentInsets = UIEdgeInsetsMake(
      0, ManualFillCloseButtonLeftInset, 0, ManualFillCloseButtonRightInset);
  SetContentEdgeInsets(closeButton, contentInsets);

  [closeButton setAccessibilityLabel:textData.closeButtonAccessibilityLabel];

  self.nextButton = nextButton;
  self.previousButton = previousButton;

  UIStackView* navigationView = [[UIStackView alloc]
      initWithArrangedSubviews:@[ previousButton, nextButton, closeButton ]];
  navigationView.spacing = ManualFillNavigationItemSpacing;
  return navigationView;
}

- (void)createOmniboxTypingShield {
  if (!_omniboxTypingShield) {
    CHECK(_contentView);
    _omniboxTypingShield = [[UIButton alloc] init];
    _omniboxTypingShield.translatesAutoresizingMaskIntoConstraints = NO;
    _omniboxTypingShield.backgroundColor = UIColor.clearColor;
    _omniboxTypingShield.isAccessibilityElement = NO;
    _omniboxTypingShield.opaque = NO;
    _omniboxTypingShield.accessibilityIdentifier =
        kFormInputAccessoryViewOmniboxTypingShieldAccessibilityID;
    [self addSubview:_omniboxTypingShield];

    AddSameConstraintsToSides(
        self, _omniboxTypingShield,
        LayoutSides::kTop | LayoutSides::kLeading | LayoutSides::kTrailing);
    _omniboxTypingShieldHeightConstraint =
        [_omniboxTypingShield.heightAnchor constraintEqualToConstant:0];
    [NSLayoutConstraint activateConstraints:@[
      _omniboxTypingShieldHeightConstraint,
      [_omniboxTypingShield.bottomAnchor
          constraintEqualToAnchor:_contentView.topAnchor]
    ]];
    [_omniboxTypingShield addTarget:self
                             action:@selector(omniboxTypingShieldTapped)
                   forControlEvents:UIControlEventTouchUpInside];
  }
}

@end
