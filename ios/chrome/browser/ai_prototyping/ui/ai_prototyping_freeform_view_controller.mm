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

using optimization_guide::proto::BlingPrototypingRequest_ModelEnum;
using optimization_guide::proto::BlingPrototypingRequest_ModelEnum_Name;

@implementation AIPrototypingFreeformViewController {
  UIButton* _serverSideSubmitButton;
  UIButton* _onDeviceSubmitButton;
  UITextField* _systemInstructionsField;
  UITextField* _queryField;
  UISwitch* _includePageContextSwitch;
  UISwitch* _uploadMQLSSwitch;
  UISwitch* _storePageContextSwitch;
  UISlider* _temperatureSlider;
  UILabel* _temperatureLabel;
  UITextView* _responseContainer;
  UIButton* _modelPickerButton;
  BlingPrototypingRequest_ModelEnum _currentModelPicked;
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

  // Title/header.
  UILabel* label = [[UILabel alloc] init];
  label.translatesAutoresizingMaskIntoConstraints = NO;
  label.font = [UIFont preferredFontForTextStyle:UIFontTextStyleTitle2];
  label.text = l10n_util::GetNSString(IDS_IOS_AI_PROTOTYPING_HEADER);

  UIColor* primaryColor = [UIColor colorNamed:kTextPrimaryColor];

  // User query.
  _queryField = [[UITextField alloc] init];
  _queryField.translatesAutoresizingMaskIntoConstraints = NO;
  _queryField.placeholder =
      l10n_util::GetNSString(IDS_IOS_AI_PROTOTYPING_QUERY_PLACEHOLDER);
  UIView* queryFieldContainer = [self textFieldContainer];
  [queryFieldContainer addSubview:_queryField];

  // System instructions.
  _systemInstructionsField = [[UITextField alloc] init];
  _systemInstructionsField.translatesAutoresizingMaskIntoConstraints = NO;
  _systemInstructionsField.placeholder = l10n_util::GetNSString(
      IDS_IOS_AI_PROTOTYPING_SYSTEM_INSTRUCTIONS_PLACEHOLDER);
  UIView* systemInstructionsFieldContainer = [self textFieldContainer];
  [systemInstructionsFieldContainer addSubview:_systemInstructionsField];

  // Page context switch.
  _includePageContextSwitch = [[UISwitch alloc] init];
  _includePageContextSwitch.translatesAutoresizingMaskIntoConstraints = NO;
  _includePageContextSwitch.on = YES;

  UILabel* switchLabel = [[UILabel alloc] init];
  switchLabel.translatesAutoresizingMaskIntoConstraints = NO;
  switchLabel.numberOfLines = 0;
  switchLabel.text =
      l10n_util::GetNSString(IDS_IOS_AI_PROTOTYPING_PAGE_CONTEXT_SWITCH);

  UIStackView* switchContainer = [[UIStackView alloc]
      initWithArrangedSubviews:@[ _includePageContextSwitch, switchLabel ]];
  switchContainer.translatesAutoresizingMaskIntoConstraints = NO;
  switchContainer.axis = UILayoutConstraintAxisHorizontal;
  switchContainer.spacing = kButtonStackViewSpacing;
  switchContainer.alignment = UIStackViewAlignmentCenter;

  // MQLS upload switch.
  _uploadMQLSSwitch = [[UISwitch alloc] init];
  _uploadMQLSSwitch.translatesAutoresizingMaskIntoConstraints = NO;
  _uploadMQLSSwitch.on = NO;

  UILabel* uploadMQLSSwitchLabel = [[UILabel alloc] init];
  uploadMQLSSwitchLabel.translatesAutoresizingMaskIntoConstraints = NO;
  uploadMQLSSwitchLabel.numberOfLines = 0;
  uploadMQLSSwitchLabel.text =
      l10n_util::GetNSString(IDS_IOS_AI_PROTOTYPING_MQLS_SWITCH);

  UIStackView* uploadMQLSSwitchContainer = [[UIStackView alloc]
      initWithArrangedSubviews:@[ _uploadMQLSSwitch, uploadMQLSSwitchLabel ]];
  uploadMQLSSwitchContainer.translatesAutoresizingMaskIntoConstraints = NO;
  uploadMQLSSwitchContainer.axis = UILayoutConstraintAxisHorizontal;
  uploadMQLSSwitchContainer.spacing = kButtonStackViewSpacing;
  uploadMQLSSwitchContainer.alignment = UIStackViewAlignmentCenter;

  // Store page context on device switch.
  _storePageContextSwitch = [[UISwitch alloc] init];
  _storePageContextSwitch.translatesAutoresizingMaskIntoConstraints = NO;
  _storePageContextSwitch.on = NO;

  UILabel* storePageContextSwitchLabel = [[UILabel alloc] init];
  storePageContextSwitchLabel.translatesAutoresizingMaskIntoConstraints = NO;
  storePageContextSwitchLabel.numberOfLines = 0;
  storePageContextSwitchLabel.text =
      l10n_util::GetNSString(IDS_IOS_AI_PROTOTYPING_STORE_PAGE_CONTEXT_SWITCH);

  UIStackView* storePageContextSwitchContainer =
      [[UIStackView alloc] initWithArrangedSubviews:@[
        _storePageContextSwitch, storePageContextSwitchLabel
      ]];
  storePageContextSwitchContainer.translatesAutoresizingMaskIntoConstraints =
      NO;
  storePageContextSwitchContainer.axis = UILayoutConstraintAxisHorizontal;
  storePageContextSwitchContainer.spacing = kButtonStackViewSpacing;
  storePageContextSwitchContainer.alignment = UIStackViewAlignmentCenter;

  // Temperature slider.
  _temperatureSlider = [[UISlider alloc] init];
  _temperatureSlider.translatesAutoresizingMaskIntoConstraints = NO;
  _temperatureSlider.minimumValue = 0.0;
  _temperatureSlider.maximumValue = 1.0;
  _temperatureSlider.value = kDefaultTemperature;
  _temperatureSlider.continuous = YES;
  [_temperatureSlider addTarget:self
                         action:@selector(temperatureSliderValueChanged:)
               forControlEvents:UIControlEventValueChanged];

  _temperatureLabel = [[UILabel alloc] init];
  _temperatureLabel.translatesAutoresizingMaskIntoConstraints = NO;
  _temperatureLabel.numberOfLines = 1;
  _temperatureLabel.text =
      [NSString stringWithFormat:@"%@ %.01f",
                                 l10n_util::GetNSString(
                                     IDS_IOS_AI_PROTOTYPING_TEMPERATURE_SLIDER),
                                 _temperatureSlider.value];

  UIStackView* temperatureContainer = [[UIStackView alloc]
      initWithArrangedSubviews:@[ _temperatureLabel, _temperatureSlider ]];
  temperatureContainer.translatesAutoresizingMaskIntoConstraints = NO;
  temperatureContainer.axis = UILayoutConstraintAxisHorizontal;
  temperatureContainer.spacing = kButtonStackViewSpacing;
  temperatureContainer.alignment = UIStackViewAlignmentCenter;

  // Model picker button.
  _modelPickerButton = [UIButton buttonWithType:UIButtonTypeSystem];
  _modelPickerButton.layer.borderColor = [primaryColor CGColor];
  _modelPickerButton.layer.borderWidth = kBorderWidth;
  _modelPickerButton.layer.cornerRadius = kCornerRadius;
  [_modelPickerButton
      setTitle:l10n_util::GetNSString(IDS_IOS_AI_PROTOTYPING_MODEL_PICKER)
      forState:UIControlStateNormal];
  [_modelPickerButton setTitleColor:primaryColor forState:UIControlStateNormal];
  _modelPickerButton.showsMenuAsPrimaryAction = YES;
  _modelPickerButton.menu = [self createModelPickerButtonMenu];

  // Submit buttons.
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
#if BUILDFLAG(BUILD_WITH_INTERNAL_OPTIMIZATION_GUIDE)
  _onDeviceSubmitButton.backgroundColor = [UIColor colorNamed:kBlueColor];
  _onDeviceSubmitButton.enabled = YES;
#else   // BUILDFLAG(BUILD_WITH_INTERNAL_OPTIMIZATION_GUIDE)
  _onDeviceSubmitButton.backgroundColor =
      [UIColor colorNamed:kDisabledTintColor];
  _onDeviceSubmitButton.enabled = NO;
#endif  // BUILDFLAG(BUILD_WITH_INTERNAL_OPTIMIZATION_GUIDE)
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

  // Model response container.
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
    label, systemInstructionsFieldContainer, queryFieldContainer,
    _modelPickerButton, switchContainer, uploadMQLSSwitchContainer,
    storePageContextSwitchContainer, temperatureContainer, buttonStackView,
    _responseContainer
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
  [self disableSubmitButtons];
  [self updateResponseField:@""];

  [self.mutator executeFreeformServerQuery:_queryField.text
                        systemInstructions:_systemInstructionsField.text
                        includePageContext:_includePageContextSwitch.isOn
                              uploadToMQLS:_uploadMQLSSwitch.isOn
                          storePageContext:_storePageContextSwitch.isOn
                               temperature:_temperatureSlider.value
                                     model:_currentModelPicked];
}

- (void)onDeviceSubmitButtonPressed:(UIButton*)button {
  [self disableSubmitButtons];
  [self updateResponseField:@""];

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

- (void)enableSubmitButtons {
  _serverSideSubmitButton.enabled = YES;
  _serverSideSubmitButton.backgroundColor = [UIColor colorNamed:kBlueColor];

#if BUILDFLAG(BUILD_WITH_INTERNAL_OPTIMIZATION_GUIDE)
  _onDeviceSubmitButton.enabled = YES;
  _onDeviceSubmitButton.backgroundColor = [UIColor colorNamed:kBlueColor];
#endif  // BUILDFLAG(BUILD_WITH_INTERNAL_OPTIMIZATION_GUIDE)
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

// Disable submit buttons, and style them accordingly.
- (void)disableSubmitButtons {
  _serverSideSubmitButton.enabled = NO;
  _serverSideSubmitButton.backgroundColor =
      [UIColor colorNamed:kDisabledTintColor];

  _onDeviceSubmitButton.enabled = NO;
  _onDeviceSubmitButton.backgroundColor =
      [UIColor colorNamed:kDisabledTintColor];
}

// Creates the menu for the model picker button.
- (UIMenu*)createModelPickerButtonMenu {
  NSMutableArray<UIAction*>* models = [NSMutableArray array];
  __weak AIPrototypingFreeformViewController* weakSelf = self;

  // Iterate over every model enum value, create the associated action and
  // populate the menu with said actions.
  for (int i = optimization_guide::proto::
           BlingPrototypingRequest_ModelEnum_ModelEnum_MIN;
       i <= optimization_guide::proto::
                BlingPrototypingRequest_ModelEnum_ModelEnum_MAX;
       ++i) {
    if (!optimization_guide::proto::BlingPrototypingRequest_ModelEnum_IsValid(
            i)) {
      continue;
    }

    BlingPrototypingRequest_ModelEnum enum_value =
        static_cast<BlingPrototypingRequest_ModelEnum>(i);
    std::string enum_name = BlingPrototypingRequest_ModelEnum_Name(enum_value);

    UIAction* menuAction = [UIAction
        actionWithTitle:base::SysUTF8ToNSString(enum_name)
                  image:nil
             identifier:nil
                handler:^(UIAction* action) {
                  [weakSelf handleModelPickerMenuActionWithModel:enum_value];
                }];

    if (enum_value == _currentModelPicked) {
      menuAction.state = UIMenuElementStateOn;
    }

    [models addObject:menuAction];
  }

  return [UIMenu
      menuWithTitle:l10n_util::GetNSString(IDS_IOS_AI_PROTOTYPING_MODEL_PICKER)
           children:models];
}

// Handle a model picker button action by setting the currently picked model and
// updating the button menu.
- (void)handleModelPickerMenuActionWithModel:
    (BlingPrototypingRequest_ModelEnum)model {
  _currentModelPicked = model;

  [_modelPickerButton
      setTitle:base::SysUTF8ToNSString(
                   BlingPrototypingRequest_ModelEnum_Name(model))
      forState:UIControlStateNormal];
  _modelPickerButton.menu = [self createModelPickerButtonMenu];
}

- (void)temperatureSliderValueChanged:(UISlider*)slider {
  // Round the slider value to the nearest step.
  float multiplier = 1.0 / kTemperatureSliderSteps;
  _temperatureSlider.value =
      roundf(_temperatureSlider.value * multiplier) / multiplier;

  _temperatureLabel.text =
      [NSString stringWithFormat:@"%@ %.01f",
                                 l10n_util::GetNSString(
                                     IDS_IOS_AI_PROTOTYPING_TEMPERATURE_SLIDER),
                                 _temperatureSlider.value];
}

@end
