// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ui/ai_prototyping_view_controller.h"

#import "base/logging.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions_apple.mm"
#import "components/optimization_guide/optimization_guide_buildflags.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(BUILD_WITH_INTERNAL_OPTIMIZATION_GUIDE)
#import "components/optimization_guide/proto/features/bling_prototyping.pb.h"
#import "components/optimization_guide/proto/string_value.pb.h"  // nogncheck
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#endif

namespace {

// Properties of UI elements in the debug menu.
constexpr CGFloat kBorderWidth = 2;
constexpr CGFloat kButtonStackViewSpacing = 10;
constexpr CGFloat kCornerRadius = 8;
constexpr CGFloat kHorizontalInset = 12;
constexpr CGFloat kMainStackTopInset = 20;
constexpr CGFloat kMainStackViewSpacing = 20;
constexpr CGFloat kResponseContainerHeightMultiplier = 0.3;
constexpr CGFloat kVerticalInset = 12;

}  // namespace

@interface AIPrototypingViewController () {
#if BUILDFLAG(BUILD_WITH_INTERNAL_OPTIMIZATION_GUIDE)
  // Service used to execute LLM queries.
  raw_ptr<OptimizationGuideService> _service;

  // Retains the on-device session in memory.
  std::unique_ptr<optimization_guide::OptimizationGuideModelExecutor::Session>
      on_device_session_;
#endif

  // The WebState that triggered the menu.
  base::WeakPtr<web::WebState> _webState;
}

@property(nonatomic, strong) UIButton* serverSideSubmitButton;
@property(nonatomic, strong) UIButton* onDeviceSubmitButton;
@property(nonatomic, strong) UITextField* queryField;
@property(nonatomic, strong) UITextField* nameField;
@property(nonatomic, strong) UITextView* responseContainer;

@end

@implementation AIPrototypingViewController

- (instancetype)initWithWebState:(web::WebState*)webState {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _webState = webState->GetWeakPtr();
#if BUILDFLAG(BUILD_WITH_INTERNAL_OPTIMIZATION_GUIDE)
    _service = OptimizationGuideServiceFactory::GetForProfile(
        ProfileIOS::FromBrowserState(_webState->GetBrowserState()));
#endif
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  self.view.translatesAutoresizingMaskIntoConstraints = NO;
  self.view.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];
  self.sheetPresentationController.detents = @[
    UISheetPresentationControllerDetent.mediumDetent,
  ];

  UILabel* label = [[UILabel alloc] init];
  label.translatesAutoresizingMaskIntoConstraints = NO;
  label.text = l10n_util::GetNSString(IDS_IOS_AI_PROTOTYPING_HEADER);

  UIColor* primaryColor = [UIColor colorNamed:kTextPrimaryColor];

  _queryField = [[UITextField alloc] init];
  _queryField.translatesAutoresizingMaskIntoConstraints = NO;
  _queryField.placeholder =
      l10n_util::GetNSString(IDS_IOS_AI_PROTOTYPING_QUERY_PLACEHOLDER);
  UIView* queryFieldContainer = [self textFieldContainer];
  [queryFieldContainer addSubview:_queryField];

  _nameField = [[UITextField alloc] init];
  _nameField.translatesAutoresizingMaskIntoConstraints = NO;
  _nameField.placeholder =
      l10n_util::GetNSString(IDS_IOS_AI_PROTOTYPING_NAME_PLACEHOLDER);
  UIView* nameFieldContainer = [self textFieldContainer];
  [nameFieldContainer addSubview:_nameField];

  _serverSideSubmitButton = [UIButton buttonWithType:UIButtonTypeSystem];
  _serverSideSubmitButton.layer.borderColor = [primaryColor CGColor];
  _serverSideSubmitButton.layer.borderWidth = kBorderWidth;
  [_serverSideSubmitButton
      setTitle:l10n_util::GetNSString(IDS_IOS_AI_PROTOTYPING_SERVER_SIDE_SUBMIT)
      forState:UIControlStateNormal];
  [_serverSideSubmitButton setTitleColor:primaryColor
                                forState:UIControlStateNormal];
  [_serverSideSubmitButton addTarget:self
                              action:@selector(serverSideSubmitButtonPressed:)
                    forControlEvents:UIControlEventTouchUpInside];

  _onDeviceSubmitButton = [UIButton buttonWithType:UIButtonTypeSystem];
  _onDeviceSubmitButton.layer.borderColor = [primaryColor CGColor];
  _onDeviceSubmitButton.layer.borderWidth = kBorderWidth;
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
        _serverSideSubmitButton, _onDeviceSubmitButton
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
    label, queryFieldContainer, nameFieldContainer, _responseContainer,
    buttonStackView
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

    [nameFieldContainer.heightAnchor
        constraintEqualToAnchor:_nameField.heightAnchor
                       constant:kVerticalInset],
    [nameFieldContainer.widthAnchor
        constraintEqualToAnchor:_nameField.widthAnchor
                       constant:kHorizontalInset],
    [nameFieldContainer.centerXAnchor
        constraintEqualToAnchor:_nameField.centerXAnchor],
    [nameFieldContainer.centerYAnchor
        constraintEqualToAnchor:_nameField.centerYAnchor],

    [_responseContainer.heightAnchor
        constraintGreaterThanOrEqualToAnchor:self.view.heightAnchor
                                  multiplier:
                                      kResponseContainerHeightMultiplier],
  ]];
}

- (void)serverSideSubmitButtonPressed:(UIButton*)button {
#if BUILDFLAG(BUILD_WITH_INTERNAL_OPTIMIZATION_GUIDE)
  optimization_guide::proto::BlingPrototypingRequest request;
  request.set_query(base::SysNSStringToUTF8(_queryField.text));
  request.set_name(base::SysNSStringToUTF8(_nameField.text));
  __weak __typeof(self) weakSelf = self;
  _service->ExecuteModel(
      optimization_guide::ModelBasedCapabilityKey::kBlingPrototyping, request,
      /*execution_timeout*/ std::nullopt,
      base::BindOnce(
          ^(optimization_guide::OptimizationGuideModelExecutionResult result,
            std::unique_ptr<optimization_guide::ModelQualityLogEntry> entry) {
            [weakSelf onServerModelExecuteResponse:std::move(result)
                                      withLogEntry:std::move(entry)];
          }));
#endif
}

- (void)onDeviceSubmitButtonPressed:(UIButton*)button {
#if BUILDFLAG(BUILD_WITH_INTERNAL_OPTIMIZATION_GUIDE)
  optimization_guide::SessionConfigParams configParams =
      optimization_guide::SessionConfigParams{
          .execution_mode = optimization_guide::SessionConfigParams::
              ExecutionMode::kOnDeviceOnly,
          .logging_mode = optimization_guide::SessionConfigParams::LoggingMode::
              kAlwaysDisable,
      };

  if (!on_device_session_) {
    on_device_session_ = _service->StartSession(
        optimization_guide::ModelBasedCapabilityKey::kPromptApi, configParams);
  }
  optimization_guide::proto::StringValue request;
  request.set_value(base::SysNSStringToUTF8(_queryField.text));

  __weak __typeof(self) weakSelf = self;
  on_device_session_->ExecuteModel(
      request,
      base::RepeatingCallback(base::BindRepeating(
          ^(optimization_guide::OptimizationGuideModelStreamingExecutionResult
                result) {
            [weakSelf onDeviceModelExecuteResponse:std::move(result)];
          })));
#endif
}

#pragma mark - Model execution
#if BUILDFLAG(BUILD_WITH_INTERNAL_OPTIMIZATION_GUIDE)
// Response callback when using server inference.
- (void)onServerModelExecuteResponse:
            (optimization_guide::OptimizationGuideModelExecutionResult)result
                        withLogEntry:
                            (std::unique_ptr<
                                optimization_guide::ModelQualityLogEntry>)
                                log_entry {
  std::string response = "";

  if (result.response.has_value()) {
    auto parsed = optimization_guide::ParsedAnyMetadata<
        optimization_guide::proto::BlingPrototypingResponse>(
        result.response.value());
    if (!parsed->output().empty()) {
      response = parsed->output();
    } else {
      response = "Empty server response.";
    }
  } else {
    response =
        base::StringPrintf("Server model execution error: %d",
                           static_cast<int>(result.response.error().error()));
  }

  _responseContainer.text = base::SysUTF8ToNSString(response);
}

// Response callback when using on device model execution.
- (void)onDeviceModelExecuteResponse:
    (optimization_guide::OptimizationGuideModelStreamingExecutionResult)result {
  std::string response = "";

  if (result.response.has_value()) {
    auto parsed = optimization_guide::ParsedAnyMetadata<
        optimization_guide::proto::StringValue>(result.response->response);
    if (parsed->has_value()) {
      response = parsed->value();
    } else {
      response = "Failed to parse device response as a string";
    }
    if (result.response->is_complete) {
      on_device_session_.reset();
    }
  } else {
    response =
        base::StringPrintf("On-device model execution error: %d",
                           static_cast<int>(result.response.error().error()));
  }

  _responseContainer.text = base::SysUTF8ToNSString(response);
}
#endif

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
