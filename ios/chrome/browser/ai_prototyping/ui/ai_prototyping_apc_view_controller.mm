// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ui/ai_prototyping_apc_view_controller.h"

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ai_prototyping/ui/ai_prototyping_mutator.h"
#import "ios/chrome/browser/ai_prototyping/utils/ai_prototyping_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

@implementation AIPrototypingAPCViewController {
  UIButton* _submitButton;
  UIButton* _copyButton;
  UISwitch* _richExtractionSwitch;
  UITextView* _responseContainer;
  // The raw bytes of the APC in Base64 format.
  NSString* _rawBytes;
}

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

  UILabel* label = [[UILabel alloc] init];
  label.translatesAutoresizingMaskIntoConstraints = NO;
  label.font = [UIFont preferredFontForTextStyle:UIFontTextStyleTitle2];
  label.text = @"Annotated Page Content";

  UIColor* primaryColor = [UIColor colorNamed:kTextPrimaryColor];

  _submitButton = [UIButton buttonWithType:UIButtonTypeSystem];
  _submitButton.backgroundColor = [UIColor colorNamed:kBlueColor];
  _submitButton.layer.cornerRadius = kCornerRadius;
  [_submitButton setTitle:@"Extract APC" forState:UIControlStateNormal];
  [_submitButton setTitleColor:primaryColor forState:UIControlStateNormal];
  [_submitButton addTarget:self
                    action:@selector(submitButtonPressed:)
          forControlEvents:UIControlEventTouchUpInside];

  _copyButton = [UIButton buttonWithType:UIButtonTypeSystem];
  _copyButton.backgroundColor = [UIColor colorNamed:kBlueColor];
  _copyButton.layer.cornerRadius = kCornerRadius;
  [_copyButton setTitle:@"Copy Raw Bytes" forState:UIControlStateNormal];
  [_copyButton setTitleColor:primaryColor forState:UIControlStateNormal];
  [_copyButton addTarget:self
                  action:@selector(copyButtonPressed:)
        forControlEvents:UIControlEventTouchUpInside];

  UIStackView* buttonsContainer = [[UIStackView alloc]
      initWithArrangedSubviews:@[ _submitButton, _copyButton ]];
  buttonsContainer.translatesAutoresizingMaskIntoConstraints = NO;
  buttonsContainer.axis = UILayoutConstraintAxisHorizontal;
  buttonsContainer.spacing = kButtonStackViewSpacing;
  buttonsContainer.distribution = UIStackViewDistributionFillEqually;

  _richExtractionSwitch = [[UISwitch alloc] init];
  _richExtractionSwitch.translatesAutoresizingMaskIntoConstraints = NO;
  _richExtractionSwitch.on = YES;

  UILabel* switchLabel = [[UILabel alloc] init];
  switchLabel.translatesAutoresizingMaskIntoConstraints = NO;
  switchLabel.numberOfLines = 0;
  switchLabel.text = @"Use APC V2 (Rich Extraction)";

  UIStackView* switchContainer = [[UIStackView alloc]
      initWithArrangedSubviews:@[ _richExtractionSwitch, switchLabel ]];
  switchContainer.translatesAutoresizingMaskIntoConstraints = NO;
  switchContainer.axis = UILayoutConstraintAxisHorizontal;
  switchContainer.spacing = kButtonStackViewSpacing;
  switchContainer.alignment = UIStackViewAlignmentCenter;

  _responseContainer = [UITextView textViewUsingTextLayoutManager:NO];
  _responseContainer.translatesAutoresizingMaskIntoConstraints = NO;
  _responseContainer.editable = NO;
  _responseContainer.layer.cornerRadius = kCornerRadius;
  _responseContainer.layer.masksToBounds = YES;
  _responseContainer.layer.borderColor = [primaryColor CGColor];
  _responseContainer.layer.borderWidth = kBorderWidth;
  _responseContainer.textContainer.lineBreakMode = NSLineBreakByWordWrapping;
  _responseContainer.text = @"Press 'Extract APC' to begin.";

  UIStackView* stackView = [[UIStackView alloc] initWithArrangedSubviews:@[
    label, switchContainer, buttonsContainer, _responseContainer
  ]];
  stackView.translatesAutoresizingMaskIntoConstraints = NO;
  stackView.axis = UILayoutConstraintAxisVertical;
  stackView.spacing = kMainStackViewSpacing;
  [self.view addSubview:stackView];

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
  ]];
}

- (void)submitButtonPressed:(UIButton*)button {
  _submitButton.enabled = NO;
  _submitButton.backgroundColor = [UIColor colorNamed:kDisabledTintColor];
  _responseContainer.text = @"";
  [self.mutator
      executeAPCExtractionWithRichExtraction:_richExtractionSwitch.isOn];
}

- (void)copyButtonPressed:(UIButton*)button {
  UIPasteboard* pasteboard = [UIPasteboard generalPasteboard];
  if (_rawBytes.length > 0) {
    pasteboard.string = _rawBytes;
  } else {
    pasteboard.string = _responseContainer.text;
  }
}

#pragma mark - AIPrototypingViewControllerProtocol

- (void)updateResponseField:(NSString*)response {
  _responseContainer.text = response;
}

- (void)updateRawBytes:(NSString*)rawBytes {
  _rawBytes = rawBytes;
}

- (void)enableSubmitButtons {
  _submitButton.enabled = YES;
  _submitButton.backgroundColor = [UIColor colorNamed:kBlueColor];
}

@end
