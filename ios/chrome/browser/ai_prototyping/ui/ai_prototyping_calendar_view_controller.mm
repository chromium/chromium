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
  UITextView* _responseContainer;
  UIButton* _submitButton;
  UITextField* _promptField;
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
    UISheetPresentationControllerDetent.mediumDetent,
    UISheetPresentationControllerDetent.largeDetent,
  ];

  UIColor* primaryColor = [UIColor colorNamed:kTextPrimaryColor];

  // Title.
  UILabel* label = [[UILabel alloc] init];
  label.translatesAutoresizingMaskIntoConstraints = NO;
  label.font = [UIFont preferredFontForTextStyle:UIFontTextStyleTitle2];
  label.text = l10n_util::GetNSString(IDS_IOS_AI_PROTOTYPING_CALENDAR_HEADER);

  // Optional user query.
  _promptField = [[UITextField alloc] init];
  _promptField.translatesAutoresizingMaskIntoConstraints = NO;
  _promptField.placeholder = l10n_util::GetNSString(
      IDS_IOS_AI_PROTOTYPING_CALENDAR_PROMPT_PLACEHOLDER);

  UIView* promptFieldContainer = [[UIView alloc] init];
  promptFieldContainer.translatesAutoresizingMaskIntoConstraints = NO;
  promptFieldContainer.layer.cornerRadius = kCornerRadius;
  promptFieldContainer.layer.masksToBounds = YES;
  promptFieldContainer.layer.borderColor =
      [[UIColor colorNamed:kTextPrimaryColor] CGColor];
  promptFieldContainer.layer.borderWidth = kBorderWidth;
  [promptFieldContainer addSubview:_promptField];

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

  UIStackView* stackView = [[UIStackView alloc] initWithArrangedSubviews:@[
    label, promptFieldContainer, _submitButton, _responseContainer
  ]];
  stackView.translatesAutoresizingMaskIntoConstraints = NO;
  stackView.axis = UILayoutConstraintAxisVertical;
  stackView.spacing = kMainStackViewSpacing;
  [self.view addSubview:stackView];

  // Constraints.
  [NSLayoutConstraint activateConstraints:@[
    [stackView.topAnchor constraintEqualToAnchor:self.view.topAnchor
                                        constant:kMainStackTopInset],
    [stackView.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor
                                            constant:kHorizontalInset],
    [stackView.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor
                                             constant:-kHorizontalInset],
    [_responseContainer.heightAnchor
        constraintGreaterThanOrEqualToAnchor:self.view.heightAnchor
                                  multiplier:
                                      kResponseContainerHeightMultiplier],

    [promptFieldContainer.heightAnchor
        constraintEqualToAnchor:_promptField.heightAnchor
                       constant:kVerticalInset],
    [promptFieldContainer.widthAnchor
        constraintEqualToAnchor:_promptField.widthAnchor
                       constant:kHorizontalInset],
    [promptFieldContainer.centerXAnchor
        constraintEqualToAnchor:_promptField.centerXAnchor],
    [promptFieldContainer.centerYAnchor
        constraintEqualToAnchor:_promptField.centerYAnchor],

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
  [_mutator executeEnhancedCalendarQueryWithPrompt:_promptField.text];
}

// Disable submit button, and style it accordingly.
- (void)disableSubmitButton {
  _submitButton.enabled = NO;
  _submitButton.backgroundColor = [UIColor colorNamed:kDisabledTintColor];
}

@end
