// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/find_bar/ui_bundled/find_bar_view.h"

#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/find_bar/ui_bundled/find_bar_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/dynamic_type_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {
// Horizontal padding betwee all elements (except the previous/next buttons).
const CGFloat kPadding = 8;
// Horizontal padding between the last button and the trailing edge in a
// Regular x Regular environment.
const CGFloat kIPadButtonEdgeSpacing = 17;
const CGFloat kInputFieldCornerRadius = 10;
const CGFloat kFontSize = 15;
const CGFloat kButtonFontSize = 17;
const CGFloat kInputFieldHeight = 36;
const CGFloat kButtonLength = 44;
}  // namespace

@interface FindBarView ()

// The overlay that shows number of results in format "1 of 13".
@property(nonatomic, strong) UILabel* resultsCountLabel;

@end

@implementation FindBarView

@synthesize inputField = _inputField;
@synthesize previousButton = _previousButton;
@synthesize nextButton = _nextButton;
@synthesize closeButton = _closeButton;
@synthesize resultsCountLabel = _resultsCountLabel;

#pragma mark - Public

- (instancetype)init {
  self = [super initWithFrame:CGRectZero];
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

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  [self updateFonts];
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
  [self updateFonts];
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

  const CGFloat closeButtonTrailingPadding =
      ShouldShowCompactToolbar(self) ? kPadding : kIPadButtonEdgeSpacing;

  NSLayoutConstraint* inputFieldHeightConstraint = [self.inputField.heightAnchor
      constraintEqualToConstant:kInputFieldHeight];

  [NSLayoutConstraint activateConstraints:@[
    // Input Field.
    [self.inputField.centerYAnchor constraintEqualToAnchor:self.centerYAnchor],
    inputFieldHeightConstraint,
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
                       constant:-closeButtonTrailingPadding],
  ]];
}

// Sets the colors for the different subviews.
- (void)setupColors {
  UIColor* inputFieldBackground =
      [UIColor colorNamed:kTextfieldBackgroundColor];
  UIColor* inputFieldPlaceHolderTextColor =
      [UIColor colorNamed:kTextfieldPlaceholderColor];
  UIColor* inputFieldTextColor = [UIColor colorNamed:kTextPrimaryColor];
  UIColor* resultsCountLabelTextColor =
      [UIColor colorNamed:kTextfieldPlaceholderColor];
  UIColor* buttonTintColor = [UIColor colorNamed:kBlueColor];

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

// Update fonts to account for new preferred content size category.
- (void)updateFonts {
  _inputField.font = PreferredFontForTextStyleWithMaxCategory(
      UIFontTextStyleBody, self.traitCollection.preferredContentSizeCategory,
      UIContentSizeCategoryAccessibilityLarge);
  _resultsCountLabel.font = PreferredFontForTextStyleWithMaxCategory(
      UIFontTextStyleBody, self.traitCollection.preferredContentSizeCategory,
      UIContentSizeCategoryAccessibilityMedium);
  _closeButton.titleLabel.font = PreferredFontForTextStyleWithMaxCategory(
      UIFontTextStyleBody, self.traitCollection.preferredContentSizeCategory,
      UIContentSizeCategoryAccessibilityMedium);
}

// Adds `rightLabel` as right view of the `textField`.
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
      NSDirectionalEdgeInsetsMake(0, kPadding, 0, kPadding));
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
    _resultsCountLabel.accessibilityElementsHidden = YES;
    _resultsCountLabel.accessibilityIdentifier = kFindInPageResultsCountLabelId;
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
    _previousButton.pointerInteractionEnabled = YES;
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
    _nextButton.pointerInteractionEnabled = YES;
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
    _closeButton.pointerInteractionEnabled = YES;
  }

  return _closeButton;
}

@end
