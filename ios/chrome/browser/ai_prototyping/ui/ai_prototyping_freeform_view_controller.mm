// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ui/ai_prototyping_freeform_view_controller.h"

#import <UIKit/UIKit.h>

#import "base/logging.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "components/optimization_guide/optimization_guide_buildflags.h"
#import "components/optimization_guide/proto/features/bling_prototyping.pb.h"
#import "components/optimization_guide/proto/string_value.pb.h"
#import "ios/chrome/browser/ai_prototyping/ui/ai_prototyping_mutator.h"
#import "ios/chrome/browser/ai_prototyping/utils/ai_prototyping_constants.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Properties of UI elements in the debug menu.
constexpr CGFloat kVerticalInset = 12;
constexpr CGFloat kButtonStackViewSpacing = 10;

}  // namespace

@interface AIPrototypingFreeformViewController ()

@property(nonatomic, strong) UIButton* serverSideSubmitButton;
@property(nonatomic, strong) UIButton* onDeviceSubmitButton;
@property(nonatomic, strong) UITextField* systemInstructionsField;
@property(nonatomic, strong) UITextField* queryField;
@property(nonatomic, strong) UISwitch* includePageContextSwitch;
@property(nonatomic, strong) UITextView* responseContainer;

@end

@implementation AIPrototypingFreeformViewController

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

  UILabel* label = [[UILabel alloc] init];
  label.translatesAutoresizingMaskIntoConstraints = NO;
  label.font = [UIFont preferredFontForTextStyle:UIFontTextStyleTitle2];
  label.text = l10n_util::GetNSString(IDS_IOS_AI_PROTOTYPING_HEADER);

  UIColor* primaryColor = [UIColor colorNamed:kTextPrimaryColor];

  _queryField = [[UITextField alloc] init];
  _queryField.translatesAutoresizingMaskIntoConstraints = NO;
  _queryField.placeholder =
      l10n_util::GetNSString(IDS_IOS_AI_PROTOTYPING_QUERY_PLACEHOLDER);
  UIView* queryFieldContainer = [self textFieldContainer];
  [queryFieldContainer addSubview:_queryField];

  _systemInstructionsField = [[UITextField alloc] init];
  _systemInstructionsField.translatesAutoresizingMaskIntoConstraints = NO;
  _systemInstructionsField.placeholder = l10n_util::GetNSString(
      IDS_IOS_AI_PROTOTYPING_SYSTEM_INSTRUCTIONS_PLACEHOLDER);
  UIView* systemInstructionsFieldContainer = [self textFieldContainer];
  [systemInstructionsFieldContainer addSubview:_systemInstructionsField];

  _includePageContextSwitch = [[UISwitch alloc] init];
  _includePageContextSwitch.translatesAutoresizingMaskIntoConstraints = NO;
  _includePageContextSwitch.on = YES;

  UILabel* switchLabel = [[UILabel alloc] init];
  switchLabel.translatesAutoresizingMaskIntoConstraints = NO;
  switchLabel.numberOfLines = 0;
  switchLabel.text =
      l10n_util::GetNSString(IDS_IOS_AI_PROTOTYPING_PAGE_CONTEXT_SWITCH);

  UIStackView* switchContainer = [[UIStackView alloc] init];
  switchContainer.translatesAutoresizingMaskIntoConstraints = NO;
  switchContainer.axis = UILayoutConstraintAxisHorizontal;
  switchContainer.spacing = kButtonStackViewSpacing;
  switchContainer.alignment = UIStackViewAlignmentCenter;
  [switchContainer addArrangedSubview:_includePageContextSwitch];
  [switchContainer addArrangedSubview:switchLabel];

  _serverSideSubmitButton = [UIButton buttonWithType:UIButtonTypeSystem];
  _serverSideSubmitButton.backgroundColor = [UIColor colorNamed:kBlueColor];
  _serverSideSubmitButton.layer.cornerRadius = kCornerRadius;
  [_serverSideSubmitButton
      setTitle:l10n_util::GetNSString(IDS_IOS_AI_PROTOTYPING_SERVER_SIDE_SUBMIT)
      forState:UIControlStateNormal];
  [_serverSideSubmitButton setTitleColor:primaryColor
                                forState:UIControlStateNormal];
  [_serverSideSubmitButton addTarget:self
                              action:@selector(serverSideSubmitButtonPressed:)
                    forControlEvents:UIControlEventTouchUpInside];

  _onDeviceSubmitButton = [UIButton buttonWithType:UIButtonTypeSystem];
  _onDeviceSubmitButton.backgroundColor = [UIColor colorNamed:kBlueColor];
  _onDeviceSubmitButton.layer.cornerRadius = kCornerRadius;
  [_onDeviceSubmitButton
      setTitle:l10n_util::GetNSString(IDS_IOS_AI_PROTOTYPING_ON_DEVICE_SUBMIT)
      forState:UIControlStateNormal];
  [_onDeviceSubmitButton setTitleColor:primaryColor
                              forState:UIControlStateNormal];
  [_onDeviceSubmitButton addTarget:self
                            action:@selector(onDeviceSubmitButtonPressed:)
                  forControlEvents:UIControlEventTouchUpInside];

  UIStackView* buttonStackView =
      [[UIStackView alloc] initWithArrangedSubviews:@[
        _serverSideSubmitButton,
        _onDeviceSubmitButton,
      ]];
  buttonStackView.translatesAutoresizingMaskIntoConstraints = NO;
  buttonStackView.axis = UILayoutConstraintAxisHorizontal;
  buttonStackView.spacing = kButtonStackViewSpacing;
  buttonStackView.distribution = UIStackViewDistributionFillEqually;

  _responseContainer = [UITextView textViewUsingTextLayoutManager:NO];
  _responseContainer.translatesAutoresizingMaskIntoConstraints = NO;
  _responseContainer.editable = NO;
  _responseContainer.layer.cornerRadius = kCornerRadius;
  _responseContainer.layer.masksToBounds = YES;
  _responseContainer.layer.borderColor = [primaryColor CGColor];
  _responseContainer.layer.borderWidth = kBorderWidth;
  _responseContainer.textContainer.lineBreakMode = NSLineBreakByWordWrapping;

  UIStackView* stackView = [[UIStackView alloc] initWithArrangedSubviews:@[
    label, systemInstructionsFieldContainer, queryFieldContainer,
    switchContainer, buttonStackView, _responseContainer
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

    [systemInstructionsFieldContainer.heightAnchor
        constraintEqualToAnchor:_systemInstructionsField.heightAnchor
                       constant:kVerticalInset],
    [systemInstructionsFieldContainer.widthAnchor
        constraintEqualToAnchor:_systemInstructionsField.widthAnchor
                       constant:kHorizontalInset],
    [systemInstructionsFieldContainer.centerXAnchor
        constraintEqualToAnchor:_systemInstructionsField.centerXAnchor],
    [systemInstructionsFieldContainer.centerYAnchor
        constraintEqualToAnchor:_systemInstructionsField.centerYAnchor],

    [queryFieldContainer.heightAnchor
        constraintEqualToAnchor:_queryField.heightAnchor
                       constant:kVerticalInset],
    [queryFieldContainer.widthAnchor
        constraintEqualToAnchor:_queryField.widthAnchor
                       constant:kHorizontalInset],
    [queryFieldContainer.centerXAnchor
        constraintEqualToAnchor:_queryField.centerXAnchor],
    [queryFieldContainer.centerYAnchor
        constraintEqualToAnchor:_queryField.centerYAnchor],

    [_responseContainer.heightAnchor
        constraintGreaterThanOrEqualToAnchor:self.view.heightAnchor
                                  multiplier:
                                      kResponseContainerHeightMultiplier],
  ]];
}

- (void)serverSideSubmitButtonPressed:(UIButton*)button {
  [self.mutator executeFreeformServerQuery:_queryField.text
                        systemInstructions:_systemInstructionsField.text
                        includePageContext:_includePageContextSwitch.isOn];
}

- (void)onDeviceSubmitButtonPressed:(UIButton*)button {
  // TODO(crbug.com/387510419): Include stringified page context in prompt when
  // on-device is better supported.
#if BUILDFLAG(BUILD_WITH_INTERNAL_OPTIMIZATION_GUIDE)
  optimization_guide::proto::StringValue request;
  request.set_value(base::SysNSStringToUTF8(_queryField.text));
  [self.mutator executeFreeformOnDeviceQuery:request];
#endif  // BUILDFLAG(BUILD_WITH_INTERNAL_OPTIMIZATION_GUIDE)
}

#pragma mark - AIPrototypingViewControllerProtocol

- (void)updateResponseField:(NSString*)response {
  _responseContainer.text = response;
}

#pragma mark - Private

// Returns a container for a text field in the menu.
- (UIView*)textFieldContainer {
  UIView* container = [[UIView alloc] init];
  container.translatesAutoresizingMaskIntoConstraints = NO;
  container.layer.cornerRadius = kCornerRadius;
  container.layer.masksToBounds = YES;
  container.layer.borderColor =
      [[UIColor colorNamed:kTextPrimaryColor] CGColor];
  container.layer.borderWidth = kBorderWidth;
  return container;
}

@end
