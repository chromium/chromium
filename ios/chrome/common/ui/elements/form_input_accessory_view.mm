// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/ui/elements/form_input_accessory_view.h"

#import <QuartzCore/QuartzCore.h>

#import "base/check.h"
#import "base/i18n/rtl.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/elements/form_input_accessory_view_text_data.h"
#import "ios/chrome/common/ui/elements/gradient_view.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

// Default Height for the accessory.
constexpr CGFloat kDefaultAccessoryHeight = 44;

// Large Height for the accessory.
constexpr CGFloat kLargeAccessoryHeight = 59;

// Button target area for the large keyboard accessory.
constexpr CGFloat kLargeButtonTargetArea = 44;

// The width for the background-colored gradient UIView.
constexpr CGFloat ManualFillGradientWidth = 44;

// The width for the background-colored gradient UIView for the large keyboard
// accessory.
constexpr CGFloat ManualFillLargeAccessoryGradientWidth = 6;

// The margin for the background-colored gradient UIView.
constexpr CGFloat ManualFillGradientMargin = 14;

// The spacing between the items in the navigation view.
constexpr CGFloat ManualFillNavigationItemSpacing = 4;

// The left content inset for the close button.
constexpr CGFloat ManualFillCloseButtonLeftInset = 7;

// The right content inset for the close button.
constexpr CGFloat ManualFillCloseButtonRightInset = 15;

// The bottom content inset for the close button.
constexpr CGFloat ManualFillCloseButtonBottomInset = 4;

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

@property(nonatomic, weak) UIButton* manualFillButton;

@property(nonatomic, weak) UIButton* passwordManualFillButton;

@property(nonatomic, weak) UIButton* creditCardManualFillButton;

@property(nonatomic, weak) UIButton* addressManualFillButton;

@property(nonatomic, weak) UIView* leadingView;

@property(nonatomic, weak) UIView* trailingView;

@property(nonatomic, strong) UIImage* manualFillSymbol;

@property(nonatomic, strong) UIImage* passwordManualFillSymbol;

@property(nonatomic, strong) UIImage* creditCardManualFillSymbol;

@property(nonatomic, strong) UIImage* addressManualFillSymbol;

@property(nonatomic, strong) UIImage* closeButtonSymbol;

@end

@implementation FormInputAccessoryView {
  // Transparent view on the top edge to let the keyboard know about the
  // omnibox.
  UIButton* _omniboxTypingShield;
  // Height constraint used to show/hide the `omniboxTypingShield`.
  NSLayoutConstraint* _omniboxTypingShieldHeightConstraint;
  // Bottom constraint used to show/hide the `omniboxTypingShield`.
  NSLayoutConstraint* _omniboxTypingShieldBottomConstraint;
  // Bottom constraint used to show/hide the `omniboxTypingShield` when the view
  // is hidden.
  NSLayoutConstraint* _omniboxTypingShieldHiddenBottomConstraint;
  // View containing the leading and trailing buttons.
  UIView* _contentView;
  // Whether we are using the large accessory view.
  BOOL _largeAccessoryViewEnabled;
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
              navigationDelegate:nil
                manualFillSymbol:nil
        passwordManualFillSymbol:nil
      creditCardManualFillSymbol:nil
         addressManualFillSymbol:nil
               closeButtonSymbol:nil];
}

- (void)setUpWithLeadingView:(UIView*)leadingView
          navigationDelegate:(id<FormInputAccessoryViewDelegate>)delegate {
  [self setUpWithLeadingView:leadingView
              customTrailingView:nil
              navigationDelegate:delegate
                manualFillSymbol:nil
        passwordManualFillSymbol:nil
      creditCardManualFillSymbol:nil
         addressManualFillSymbol:nil
               closeButtonSymbol:nil];
}

- (void)setUpWithLeadingView:(UIView*)leadingView
            navigationDelegate:(id<FormInputAccessoryViewDelegate>)delegate
              manualFillSymbol:(UIImage*)manualFillSymbol
      passwordManualFillSymbol:(UIImage*)passwordManualFillSymbol
    creditCardManualFillSymbol:(UIImage*)creditCardManualFillSymbol
       addressManualFillSymbol:(UIImage*)addressManualFillSymbol
             closeButtonSymbol:(UIImage*)closeButtonSymbol {
  DCHECK(manualFillSymbol);
  _largeAccessoryViewEnabled = YES;
  [self setUpWithLeadingView:leadingView
              customTrailingView:nil
              navigationDelegate:delegate
                manualFillSymbol:manualFillSymbol
        passwordManualFillSymbol:passwordManualFillSymbol
      creditCardManualFillSymbol:creditCardManualFillSymbol
         addressManualFillSymbol:addressManualFillSymbol
               closeButtonSymbol:closeButtonSymbol];
}

- (void)setOmniboxTypingShieldHeight:(CGFloat)typingShieldHeight {
  _omniboxTypingShieldHeightConstraint.constant = typingShieldHeight;
  if (self.window) {
    [self layoutIfNeeded];
  }
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

- (void)manualFillButtonTapped {
  [self.delegate formInputAccessoryViewDidTapManualFillButton:self];
}

- (void)passwordManualFillButtonTapped {
  [self.delegate formInputAccessoryViewDidTapPasswordManualFillButton:self];
}

- (void)creditCardManualFillButtonTapped {
  [self.delegate formInputAccessoryViewDidTapCreditCardManualFillButton:self];
}

- (void)addressManualFillButtonTapped {
  [self.delegate formInputAccessoryViewDidTapAddressManualFillButton:self];
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
            navigationDelegate:(id<FormInputAccessoryViewDelegate>)delegate
              manualFillSymbol:(UIImage*)manualFillSymbol
      passwordManualFillSymbol:(UIImage*)passwordManualFillSymbol
    creditCardManualFillSymbol:(UIImage*)creditCardManualFillSymbol
       addressManualFillSymbol:(UIImage*)addressManualFillSymbol
             closeButtonSymbol:(UIImage*)closeButtonSymbol {
  DCHECK(!self.subviews.count);  // This should only be called once.

  self.accessibilityIdentifier = kFormInputAccessoryViewAccessibilityID;
  self.translatesAutoresizingMaskIntoConstraints = NO;
  self.backgroundColor = UIColor.clearColor;
  self.opaque = NO;
  self.manualFillSymbol = manualFillSymbol;
  self.passwordManualFillSymbol = passwordManualFillSymbol;
  self.creditCardManualFillSymbol = creditCardManualFillSymbol;
  self.addressManualFillSymbol = addressManualFillSymbol;
  self.closeButtonSymbol = closeButtonSymbol;

  _contentView = [[UIView alloc] init];
  _contentView.translatesAutoresizingMaskIntoConstraints = NO;
  _contentView.backgroundColor = [self contentBackgroundColor];
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
  self.trailingView = trailingView;

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
      constraintEqualToConstant:[self accessoryHeight]];
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
  UIView* gradientView =
      [[GradientView alloc] initWithStartColor:[[self contentBackgroundColor]
                                                   colorWithAlphaComponent:0]
                                      endColor:[self contentBackgroundColor]
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
        constraintEqualToConstant:_largeAccessoryViewEnabled
                                      ? ManualFillLargeAccessoryGradientWidth
                                      : ManualFillGradientWidth],
    [gradientView.trailingAnchor
        constraintEqualToAnchor:trailingView.leadingAnchor
                       constant:_largeAccessoryViewEnabled
                                    ? 0
                                    : ManualFillGradientMargin],

    [leadingViewContainer.trailingAnchor
        constraintEqualToAnchor:trailingView.leadingAnchor],
  ]];

  [self createOmniboxTypingShield];
}

// Returns a view that shows navigation buttons.
- (UIView*)viewForNavigationButtons {
  FormInputAccessoryViewTextData* textData =
      [self.delegate textDataforFormInputAccessoryView:self];

  UIButton* closeButton = [self createCloseButtonWithText:textData];

  UIStackView* navigationView = nil;
  if (_largeAccessoryViewEnabled) {
    UIButton* manualFillButton = [self createManualFillButtonWithText:textData];
    manualFillButton.hidden = YES;
    self.manualFillButton = manualFillButton;

    UIButton* passwordManualFillButton =
        [self createPasswordManualFillButtonWithText:textData];
    passwordManualFillButton.hidden = YES;
    self.passwordManualFillButton = passwordManualFillButton;

    UIButton* creditCardManualFillButton =
        [self createCreditCardManualFillButtonWithText:textData];
    creditCardManualFillButton.hidden = YES;
    self.creditCardManualFillButton = creditCardManualFillButton;

    UIButton* addressManualFillButton =
        [self createAddressManualFillButtonWithText:textData];
    addressManualFillButton.hidden = YES;
    self.addressManualFillButton = addressManualFillButton;

    navigationView = [[UIStackView alloc] initWithArrangedSubviews:@[
      passwordManualFillButton, creditCardManualFillButton,
      addressManualFillButton, manualFillButton, closeButton
    ]];
  } else {
    UIButton* previousButton = [self createPreviousButtonWithText:textData];
    self.previousButton = previousButton;

    UIButton* nextButton = [self createNextButtonWithText:textData];
    self.nextButton = nextButton;

    navigationView = [[UIStackView alloc]
        initWithArrangedSubviews:@[ previousButton, nextButton, closeButton ]];
  }
  navigationView.spacing = ManualFillNavigationItemSpacing;
  return navigationView;
}

// Create a button with the desired image, action and accessibility label.
- (UIButton*)createImageButton:(UIImage*)image
                        action:(SEL)action
            accessibilityLabel:(NSString*)accessibilityLabel {
  UIButton* imageButton = [UIButton buttonWithType:UIButtonTypeSystem];
  [imageButton setImage:image forState:UIControlStateNormal];
  if (_largeAccessoryViewEnabled) {
    [imageButton.widthAnchor
        constraintGreaterThanOrEqualToConstant:kLargeButtonTargetArea]
        .active = YES;
    [imageButton.heightAnchor
        constraintGreaterThanOrEqualToConstant:kLargeButtonTargetArea]
        .active = YES;
  }
  [imageButton addTarget:self
                  action:action
        forControlEvents:UIControlEventTouchUpInside];
  [imageButton setAccessibilityLabel:accessibilityLabel];
  return imageButton;
}

// Create the manual fill button.
- (UIButton*)createManualFillButtonWithText:
    (FormInputAccessoryViewTextData*)textData {
  return [self createImageButton:self.manualFillSymbol
                          action:@selector(manualFillButtonTapped)
              accessibilityLabel:textData.manualFillButtonAccessibilityLabel];
}

// Create the password manual fill button.
- (UIButton*)createPasswordManualFillButtonWithText:
    (FormInputAccessoryViewTextData*)textData {
  return [self
       createImageButton:self.passwordManualFillSymbol
                  action:@selector(passwordManualFillButtonTapped)
      accessibilityLabel:textData.passwordManualFillButtonAccessibilityLabel];
}

// Create the credit card manual fill button.
- (UIButton*)createCreditCardManualFillButtonWithText:
    (FormInputAccessoryViewTextData*)textData {
  return [self
       createImageButton:self.creditCardManualFillSymbol
                  action:@selector(creditCardManualFillButtonTapped)
      accessibilityLabel:textData.creditCardManualFillButtonAccessibilityLabel];
}

// Create the address manual fill button.
- (UIButton*)createAddressManualFillButtonWithText:
    (FormInputAccessoryViewTextData*)textData {
  return [self
       createImageButton:self.addressManualFillSymbol
                  action:@selector(addressManualFillButtonTapped)
      accessibilityLabel:textData.addressManualFillButtonAccessibilityLabel];
}

// Create the previous button.
- (UIButton*)createPreviousButtonWithText:
    (FormInputAccessoryViewTextData*)textData {
  return [self createImageButton:[UIImage imageNamed:@"mf_arrow_up"]
                          action:@selector(previousButtonTapped)
              accessibilityLabel:textData.previousButtonAccessibilityLabel];
}

// Create the next button.
- (UIButton*)createNextButtonWithText:
    (FormInputAccessoryViewTextData*)textData {
  return [self createImageButton:[UIImage imageNamed:@"mf_arrow_down"]
                          action:@selector(nextButtonTapped)
              accessibilityLabel:textData.nextButtonAccessibilityLabel];
}

// Create the close button.
- (UIButton*)createCloseButtonWithText:
    (FormInputAccessoryViewTextData*)textData {
  UIButton* closeButton = [UIButton buttonWithType:UIButtonTypeSystem];
  [closeButton addTarget:self
                  action:@selector(closeButtonTapped)
        forControlEvents:UIControlEventTouchUpInside];

  UIButtonConfiguration* buttonConfiguration =
      [UIButtonConfiguration plainButtonConfiguration];
  if (self.closeButtonSymbol) {
    buttonConfiguration.image = self.closeButtonSymbol;
  } else {
    buttonConfiguration.title = textData.closeButtonTitle;
  }
  buttonConfiguration.contentInsets = NSDirectionalEdgeInsetsMake(
      0, ManualFillCloseButtonLeftInset,
      self.closeButtonSymbol ? ManualFillCloseButtonBottomInset : 0,
      ManualFillCloseButtonRightInset);
  closeButton.configuration = buttonConfiguration;

  [closeButton setAccessibilityLabel:textData.closeButtonAccessibilityLabel];

  return closeButton;
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
    _omniboxTypingShieldBottomConstraint = [_omniboxTypingShield.bottomAnchor
        constraintEqualToAnchor:_contentView.topAnchor];
    _omniboxTypingShieldHiddenBottomConstraint =
        [_omniboxTypingShield.bottomAnchor
            constraintEqualToAnchor:self.bottomAnchor];
    [NSLayoutConstraint activateConstraints:@[
      _omniboxTypingShieldHeightConstraint, _omniboxTypingShieldBottomConstraint
    ]];
    [_omniboxTypingShield addTarget:self
                             action:@selector(omniboxTypingShieldTapped)
                   forControlEvents:UIControlEventTouchUpInside];
  }
}

// Returns the height of the accessory. Returns a larger height when using the
// large accessory view.
- (CGFloat)accessoryHeight {
  return _largeAccessoryViewEnabled ? kLargeAccessoryHeight
                                    : kDefaultAccessoryHeight;
}

// Returns the content view's background color. Returns grey when using the
// large accessory view.
- (UIColor*)contentBackgroundColor {
  return _largeAccessoryViewEnabled
             ? [UIColor colorNamed:kGroupedPrimaryBackgroundColor]
             : [UIColor colorNamed:kBackgroundColor];
}

#pragma mark - UIView

- (void)setHidden:(BOOL)hidden {
  [super setHidden:hidden];

  // The bottom omnibox is anchored to the typing shield. If we don't change the
  // shield's anchor, when hiding the accessory view, there is a blank space the
  // size of the keyboard accessory view between the top of the view below the
  // accessory view and the omnibox. By changing the anchor here, the omnibox
  // appears directly above the view below the accessory view, without any gaps.
  _omniboxTypingShieldBottomConstraint.active = !hidden;
  _omniboxTypingShieldHiddenBottomConstraint.active = hidden;

  [self layoutIfNeeded];
}

@end
