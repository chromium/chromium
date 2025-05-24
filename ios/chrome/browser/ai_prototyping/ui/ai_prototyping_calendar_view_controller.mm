// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ui/ai_prototyping_calendar_view_controller.h"

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ai_prototyping/ui/ai_prototyping_mutator.h"
#import "ios/chrome/browser/ai_prototyping/utils/ai_prototyping_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

@implementation AIPrototypingCalendarViewController {
  UITextField* _selectedTextField;
  UITextView* _promptField;
  UIButton* _submitButton;
  UITextView* _responseContainer;
}

// Synthesized from `AIPrototypingViewControllerProtocol`.
@synthesize mutator = _mutator;
@synthesize feature = _feature;

- (instancetype)initForFeature:(AIPrototypingFeature)feature {
  self = [super init];
  if (self) {
    _feature = feature;
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  self.sheetPresentationController.detents = @[
    [UISheetPresentationControllerDetent mediumDetent],
    [UISheetPresentationControllerDetent largeDetent],
  ];

  UIColor* primaryColor = [UIColor colorNamed:kTextPrimaryColor];

  // Title. Outside the scroll view to allow something to drag onto to change
  // detents of the sheet.
  UILabel* label = [[UILabel alloc] init];
  label.translatesAutoresizingMaskIntoConstraints = NO;
  label.font = [UIFont preferredFontForTextStyle:UIFontTextStyleTitle2];
  label.text = l10n_util::GetNSString(IDS_IOS_AI_PROTOTYPING_CALENDAR_HEADER);
  [self.view addSubview:label];

  // Wrapper scrollview to allow for scrolling in case the prompt field becomes
  // too big to fit.
  UIScrollView* scrollView = [[UIScrollView alloc] init];
  scrollView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:scrollView];

  // Selected date/time text field.
  _selectedTextField = [[UITextField alloc] init];
  _selectedTextField.translatesAutoresizingMaskIntoConstraints = NO;
  _selectedTextField.placeholder = l10n_util::GetNSString(
      IDS_IOS_AI_PROTOTYPING_CALENDAR_SELECTED_TEXT_PLACEHOLDER);
  UIView* selectedTextFieldContainer = [self textFieldContainer];
  [selectedTextFieldContainer addSubview:_selectedTextField];

  // Optional user prompt. Label is separate since it is now a `UITextView` (no
  // placeholder).
  UILabel* promptFieldLabel = [[UILabel alloc] init];
  promptFieldLabel.translatesAutoresizingMaskIntoConstraints = NO;
  promptFieldLabel.text = l10n_util::GetNSString(
      IDS_IOS_AI_PROTOTYPING_CALENDAR_PROMPT_PLACEHOLDER);

  _promptField = [[UITextView alloc] init];
  _promptField.translatesAutoresizingMaskIntoConstraints = NO;
  _promptField.scrollEnabled = NO;
  _promptField.layer.cornerRadius = kCornerRadius;
  _promptField.layer.masksToBounds = YES;
  _promptField.layer.borderColor = [primaryColor CGColor];
  _promptField.layer.borderWidth = kBorderWidth;
  _promptField.textContainer.lineBreakMode = NSLineBreakByWordWrapping;
  _promptField.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];

  // Submit button.
  _submitButton = [UIButton buttonWithType:UIButtonTypeSystem];
  _submitButton.backgroundColor = [UIColor colorNamed:kBlueColor];
  _submitButton.layer.cornerRadius = kCornerRadius;
  [_submitButton
      setTitle:l10n_util::GetNSString(IDS_IOS_AI_PROTOTYPING_SERVER_SIDE_SUBMIT)
      forState:UIControlStateNormal];
  [_submitButton setTitleColor:primaryColor forState:UIControlStateNormal];
  [_submitButton addTarget:self
                    action:@selector(onSubmitButtonPressed:)
          forControlEvents:UIControlEventTouchUpInside];

  // Response text view.
  _responseContainer = [UITextView textViewUsingTextLayoutManager:NO];
  _responseContainer.translatesAutoresizingMaskIntoConstraints = NO;
  _responseContainer.editable = NO;
  _responseContainer.layer.cornerRadius = kCornerRadius;
  _responseContainer.layer.masksToBounds = YES;
  _responseContainer.layer.borderColor = [primaryColor CGColor];
  _responseContainer.layer.borderWidth = kBorderWidth;
  _responseContainer.textContainer.lineBreakMode = NSLineBreakByWordWrapping;
  _responseContainer.text =
      l10n_util::GetNSString(IDS_IOS_AI_PROTOTYPING_RESULT_PLACEHOLDER);

  UIStackView* stackView = [[UIStackView alloc] initWithArrangedSubviews:@[
    selectedTextFieldContainer, promptFieldLabel, _promptField, _submitButton,
    _responseContainer
  ]];
  stackView.translatesAutoresizingMaskIntoConstraints = NO;
  stackView.axis = UILayoutConstraintAxisVertical;
  stackView.spacing = kMainStackViewSpacing;
  [scrollView addSubview:stackView];

  // Constraints.
  [NSLayoutConstraint activateConstraints:@[
    [label.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor
                                        constant:kHorizontalInset],
    [label.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor
                                         constant:-kHorizontalInset],
    [label.topAnchor constraintEqualToAnchor:self.view.topAnchor
                                    constant:kMainStackTopInset],

    [scrollView.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
    [scrollView.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
    [scrollView.bottomAnchor constraintEqualToAnchor:self.view.bottomAnchor],
    [scrollView.topAnchor constraintEqualToAnchor:label.bottomAnchor
                                         constant:kMainStackViewSpacing],

    [stackView.topAnchor constraintEqualToAnchor:scrollView.topAnchor],
    [stackView.bottomAnchor
        constraintLessThanOrEqualToAnchor:scrollView.bottomAnchor],
    [stackView.leadingAnchor constraintEqualToAnchor:scrollView.leadingAnchor
                                            constant:kHorizontalInset],
    [stackView.trailingAnchor constraintEqualToAnchor:scrollView.trailingAnchor
                                             constant:-kHorizontalInset],
    [stackView.centerXAnchor constraintEqualToAnchor:scrollView.centerXAnchor],
    [_responseContainer.heightAnchor
        constraintGreaterThanOrEqualToAnchor:scrollView.heightAnchor
                                  multiplier:
                                      kResponseContainerHeightMultiplier],

    [selectedTextFieldContainer.heightAnchor
        constraintEqualToAnchor:_selectedTextField.heightAnchor
                       constant:kVerticalInset],
    [selectedTextFieldContainer.widthAnchor
        constraintEqualToAnchor:_selectedTextField.widthAnchor
                       constant:kHorizontalInset],
    [selectedTextFieldContainer.centerXAnchor
        constraintEqualToAnchor:_selectedTextField.centerXAnchor],
    [selectedTextFieldContainer.centerYAnchor
        constraintEqualToAnchor:_selectedTextField.centerYAnchor],

    [_promptField.heightAnchor
        constraintGreaterThanOrEqualToAnchor:_selectedTextField.heightAnchor
                                    constant:kVerticalInset],

  ]];
}

#pragma mark - AIPrototypingViewControllerProtocol

- (void)updateResponseField:(NSString*)response {
  _responseContainer.text = response;
}

- (void)enableSubmitButtons {
  _submitButton.enabled = YES;
  _submitButton.backgroundColor = [UIColor colorNamed:kBlueColor];
}

#pragma mark - Private

- (void)onSubmitButtonPressed:(UIButton*)button {
  [self disableSubmitButton];
  [self updateResponseField:@""];
  [_mutator executeEnhancedCalendarQueryWithPrompt:_promptField.text
                                      selectedText:_selectedTextField.text];
}

// Disable submit button, and style it accordingly.
- (void)disableSubmitButton {
  _submitButton.enabled = NO;
  _submitButton.backgroundColor = [UIColor colorNamed:kDisabledTintColor];
}

// Creates a text field container.
- (UIView*)textFieldContainer {
  UIView* container = [[UIView alloc] init];
  container.layer.masksToBounds = YES;
  container.layer.cornerRadius = kCornerRadius;
  container.layer.borderColor =
      [[UIColor colorNamed:kTextPrimaryColor] CGColor];
  container.layer.borderWidth = kBorderWidth;
  container.translatesAutoresizingMaskIntoConstraints = NO;
  return container;
}

@end
