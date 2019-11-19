// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/find_bar/find_bar_view.h"

#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/find_bar/find_bar_constants.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_constants.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/colors/UIColor+cr_semantic_colors.h"
#import "ios/chrome/common/colors/dynamic_color_util.h"
#import "ios/chrome/common/colors/semantic_color_names.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Horizontal padding betwee all elements (except the previous/next buttons).
const CGFloat kPadding = 8;
const CGFloat kInputFieldCornerRadius = 10;
const CGFloat kFontSize = 15;
const CGFloat kButtonFontSize = 17;
const CGFloat kInputFieldHeight = 36;
const CGFloat kButtonLength = 44;
}  // namespace

@interface FindBarView ()

// The overlay that shows number of results in format "1 of 13".
@property(nonatomic, strong) UILabel* resultsCountLabel;

@property(nonatomic, assign) BOOL darkMode;

@end

@implementation FindBarView

@synthesize inputField = _inputField;
@synthesize previousButton = _previousButton;
@synthesize nextButton = _nextButton;
@synthesize closeButton = _closeButton;
@synthesize resultsCountLabel = _resultsCountLabel;
@synthesize darkMode = _darkMode;

#pragma mark - Public

- (instancetype)initWithDarkAppearance:(BOOL)darkAppearance {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    _darkMode = darkAppearance;
  }
  return self;
}

- (void)updateResultsLabelWithText:(NSString*)text {
  self.resultsCountLabel.hidden = (text.length == 0);
  self.resultsCountLabel.text = text;
}

- (void)didMoveToSuperview {
  [super didMoveToSuperview];

  if (self.inputField.superview)
    return;

  [self setupSubviews];
  [self setupConstraints];
}

#pragma mark - Private

// Creates, adds and configures the subviews.
- (void)setupSubviews {
  [self addLabel:self.resultsCountLabel asRightViewOfTextField:self.inputField];
  [self addSubview:self.inputField];
  [self addSubview:self.previousButton];
  [self addSubview:self.nextButton];
  [self addSubview:self.closeButton];

  [self setupColors];
}

// Sets the constraints for the subviews up.
// The subviews layout is the following:
// |-[inputField]-[previousButton][nextButton]-[closeButton]-|
- (void)setupConstraints {
  id<LayoutGuideProvider> safeArea = self.safeAreaLayoutGuide;

  [self.closeButton
      setContentCompressionResistancePriority:UILayoutPriorityDefaultHigh + 1
                                      forAxis:UILayoutConstraintAxisHorizontal];
  [self.inputField setContentHuggingPriority:UILayoutPriorityDefaultLow - 1
                                     forAxis:UILayoutConstraintAxisHorizontal];

  [NSLayoutConstraint activateConstraints:@[
    // Input Field.
    [self.inputField.centerYAnchor constraintEqualToAnchor:self.centerYAnchor],
    [self.inputField.heightAnchor constraintEqualToConstant:kInputFieldHeight],
    [self.inputField.leadingAnchor
        constraintEqualToAnchor:safeArea.leadingAnchor
                       constant:kPadding],

    [self.inputField.trailingAnchor
        constraintEqualToAnchor:self.previousButton.leadingAnchor
                       constant:-kPadding],

    // Previous Button.
    [self.previousButton.centerYAnchor
        constraintEqualToAnchor:self.centerYAnchor],
    [self.previousButton.widthAnchor constraintEqualToConstant:kButtonLength],
    [self.previousButton.heightAnchor constraintEqualToConstant:kButtonLength],

    [self.previousButton.trailingAnchor
        constraintEqualToAnchor:self.nextButton.leadingAnchor],

    // Next Button.
    [self.nextButton.centerYAnchor constraintEqualToAnchor:self.centerYAnchor],
    [self.nextButton.widthAnchor constraintEqualToConstant:kButtonLength],
    [self.nextButton.heightAnchor constraintEqualToConstant:kButtonLength],

    [self.nextButton.trailingAnchor
        constraintEqualToAnchor:self.closeButton.leadingAnchor
                       constant:-kPadding],

    // Close Button.
    [self.closeButton.centerYAnchor constraintEqualToAnchor:self.centerYAnchor],
    [self.closeButton.heightAnchor constraintEqualToConstant:kButtonLength],
    // Use button intrinsic width.
    [self.closeButton.trailingAnchor
        constraintEqualToAnchor:safeArea.trailingAnchor
                       constant:-kPadding],
  ]];
}

// Sets the colors for the different subviews, based on the |darkMode|.
- (void)setupColors {
  UIColor* inputFieldBackground = color::DarkModeDynamicColor(
      [UIColor colorNamed:kTextfieldBackgroundColor], self.darkMode,
      [UIColor colorNamed:kTextfieldBackgroundDarkColor]);
  UIColor* inputFieldPlaceHolderTextColor = color::DarkModeDynamicColor(
      [UIColor colorNamed:kTextfieldPlaceholderColor], self.darkMode,
      [UIColor colorNamed:kTextfieldPlaceholderDarkColor]);
  UIColor* inputFieldTextColor = color::DarkModeDynamicColor(
      [UIColor colorNamed:kTextPrimaryColor], self.darkMode,
      [UIColor colorNamed:kTextPrimaryDarkColor]);
  UIColor* resultsCountLabelTextColor = color::DarkModeDynamicColor(
      [UIColor colorNamed:kTextfieldPlaceholderColor], self.darkMode,
      [UIColor colorNamed:kTextfieldPlaceholderDarkColor]);
  UIColor* buttonTintColor = color::DarkModeDynamicColor(
      [UIColor colorNamed:kBlueColor], self.darkMode,
      [UIColor colorNamed:kBlueDarkColor]);

  self.inputField.backgroundColor = inputFieldBackground;
  NSString* placeholder = [self.inputField placeholder];
  NSDictionary* attributes =
      @{NSForegroundColorAttributeName : inputFieldPlaceHolderTextColor};
  [self.inputField setAttributedPlaceholder:[[NSAttributedString alloc]
                                                initWithString:placeholder
                                                    attributes:attributes]];
  self.inputField.textColor = inputFieldTextColor;
  self.inputField.tintColor = buttonTintColor;

  // Results count.
  self.resultsCountLabel.textColor = resultsCountLabelTextColor;

  // Buttons.
  self.previousButton.tintColor = buttonTintColor;
  self.nextButton.tintColor = buttonTintColor;
  self.closeButton.tintColor = buttonTintColor;
}

#pragma mark - Configuration

// Adds |rightLabel| as right view of the |textField|.
- (void)addLabel:(UILabel*)rightLabel
    asRightViewOfTextField:(UITextField*)textField {
  UIView* rightView = [[UIView alloc] init];
  rightView.userInteractionEnabled = NO;
  rightView.translatesAutoresizingMaskIntoConstraints = NO;
  [rightView addSubview:rightLabel];
  AddSameConstraintsToSidesWithInsets(
      rightLabel, rightView,
      LayoutSides::kTop | LayoutSides::kBottom | LayoutSides::kLeading |
          LayoutSides::kTrailing,
      ChromeDirectionalEdgeInsetsMake(0, kPadding, 0, kPadding));
  textField.rightView = rightView;
  textField.rightViewMode = UITextFieldViewModeAlways;
}

#pragma mark - Property accessors

// Creates and returns the input text field.
- (UITextField*)inputField {
  if (!_inputField) {
    _inputField = [[UITextField alloc] init];
    UIView* leftPadding =
        [[UIView alloc] initWithFrame:CGRectMake(0, 0, kPadding, 0)];
    leftPadding.userInteractionEnabled = NO;
    _inputField.leftView = leftPadding;
    _inputField.leftViewMode = UITextFieldViewModeAlways;

    _inputField.layer.cornerRadius = kInputFieldCornerRadius;
    _inputField.translatesAutoresizingMaskIntoConstraints = NO;
    _inputField.placeholder =
        l10n_util::GetNSString(IDS_IOS_PLACEHOLDER_FIND_IN_PAGE);
    _inputField.font = [UIFont systemFontOfSize:kFontSize];
    _inputField.accessibilityIdentifier = kFindInPageInputFieldId;
  }
  return _inputField;
}

// Creates and returns the results count label.
- (UILabel*)resultsCountLabel {
  if (!_resultsCountLabel) {
    _resultsCountLabel = [[UILabel alloc] init];
    _resultsCountLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _resultsCountLabel.font = [UIFont systemFontOfSize:kFontSize];
  }

  return _resultsCountLabel;
}

// Creates and returns the previous button.
- (UIButton*)previousButton {
  if (!_previousButton) {
    _previousButton = [UIButton buttonWithType:UIButtonTypeSystem];
    [_previousButton
        setImage:[[UIImage imageNamed:@"find_prev"]
                     imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate]
        forState:UIControlStateNormal];
    _previousButton.translatesAutoresizingMaskIntoConstraints = NO;
    SetA11yLabelAndUiAutomationName(_previousButton,
                                    IDS_FIND_IN_PAGE_PREVIOUS_TOOLTIP,
                                    kFindInPagePreviousButtonId);
  }

  return _previousButton;
}

// Creates and returns the next button.
- (UIButton*)nextButton {
  if (!_nextButton) {
    _nextButton = [UIButton buttonWithType:UIButtonTypeSystem];
    [_nextButton
        setImage:[[UIImage imageNamed:@"find_next"]
                     imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate]
        forState:UIControlStateNormal];
    _nextButton.translatesAutoresizingMaskIntoConstraints = NO;
    SetA11yLabelAndUiAutomationName(_nextButton, IDS_FIND_IN_PAGE_NEXT_TOOLTIP,
                                    kFindInPageNextButtonId);
  }

  return _nextButton;
}

// Creates and returns the closes button.
- (UIButton*)closeButton {
  if (!_closeButton) {
    _closeButton = [UIButton buttonWithType:UIButtonTypeSystem];
    [_closeButton setTitle:l10n_util::GetNSString(IDS_DONE)
                  forState:UIControlStateNormal];
    _closeButton.translatesAutoresizingMaskIntoConstraints = NO;
    _closeButton.accessibilityIdentifier = kFindInPageCloseButtonId;
    _closeButton.titleLabel.font = [UIFont systemFontOfSize:kButtonFontSize];
  }

  return _closeButton;
}

@end
