// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ui/ai_prototyping_freeform_view_controller.h"

#import "base/logging.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "components/optimization_guide/optimization_guide_buildflags.h"
#import "ios/chrome/browser/ai_prototyping/ui/ai_prototyping_mutator.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(BUILD_WITH_INTERNAL_OPTIMIZATION_GUIDE)
#import "components/optimization_guide/proto/features/bling_prototyping.pb.h"
#import "components/optimization_guide/proto/features/tab_organization.pb.h"
#import "components/optimization_guide/proto/string_value.pb.h"
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

@interface AIPrototypingFreeformViewController ()

@property(nonatomic, strong) UIButton* serverSideSubmitButton;
@property(nonatomic, strong) UIButton* onDeviceSubmitButton;
@property(nonatomic, strong) UIButton* groupTabsButton;
@property(nonatomic, strong) UITextField* queryField;
@property(nonatomic, strong) UITextField* nameField;
@property(nonatomic, strong) UIButton* groupingStrategyMenu;
@property(nonatomic, strong) UITextView* responseContainer;

// The currently selected strategy for tab grouping.
@property(nonatomic, assign) optimization_guide::proto::
    TabOrganizationRequest_TabOrganizationModelStrategy groupingStrategy;

@end

@implementation AIPrototypingFreeformViewController

// Synthesized from `AIPrototypingViewControllerProtocol`.
@synthesize mutator = _mutator;

- (void)viewDidLoad {
  [super viewDidLoad];

  self.sheetPresentationController.detents = @[
    UISheetPresentationControllerDetent.mediumDetent,
    UISheetPresentationControllerDetent.largeDetent,
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

#if BUILDFLAG(BUILD_WITH_INTERNAL_OPTIMIZATION_GUIDE)
  _groupingStrategy = optimization_guide::proto::
      TabOrganizationRequest_TabOrganizationModelStrategy_STRATEGY_TOPIC_BASED;
#endif
  _groupingStrategyMenu = [UIButton buttonWithType:UIButtonTypeSystem];
  _groupingStrategyMenu.layer.borderColor = [primaryColor CGColor];
  _groupingStrategyMenu.layer.borderWidth = kBorderWidth;
  [_groupingStrategyMenu setTitle:@"Grouping strategy"
                         forState:UIControlStateNormal];
  [_groupingStrategyMenu setTitleColor:primaryColor
                              forState:UIControlStateNormal];
  _groupingStrategyMenu.showsMenuAsPrimaryAction = YES;
  _groupingStrategyMenu.menu = [self createTabGroupingStrategyMenu];

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

  _groupTabsButton = [UIButton buttonWithType:UIButtonTypeSystem];
  _groupTabsButton.layer.borderColor = [primaryColor CGColor];
  _groupTabsButton.layer.borderWidth = kBorderWidth;
  [_groupTabsButton setTitle:@"Group tabs" forState:UIControlStateNormal];
  [_groupTabsButton setTitleColor:primaryColor forState:UIControlStateNormal];
  [_groupTabsButton addTarget:self
                       action:@selector(onGroupTabsButtonPressed:)
             forControlEvents:UIControlEventTouchUpInside];

  UIStackView* buttonStackView =
      [[UIStackView alloc] initWithArrangedSubviews:@[
        _serverSideSubmitButton,
        _onDeviceSubmitButton,
        _groupTabsButton,
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
    label, queryFieldContainer, nameFieldContainer, _groupingStrategyMenu,
    _responseContainer, buttonStackView
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
  [self.mutator executeServerQuery:request];
#endif
}

- (void)onDeviceSubmitButtonPressed:(UIButton*)button {
#if BUILDFLAG(BUILD_WITH_INTERNAL_OPTIMIZATION_GUIDE)
  optimization_guide::proto::StringValue request;
  request.set_value(base::SysNSStringToUTF8(_queryField.text));
  [self.mutator executeOnDeviceQuery:request];
#endif
}

- (void)onGroupTabsButtonPressed:(UIButton*)button {
#if BUILDFLAG(BUILD_WITH_INTERNAL_OPTIMIZATION_GUIDE)
  [self.mutator executeGroupTabsWithStrategy:self.groupingStrategy];
#endif
}

#pragma mark - AIPrototypingConsumer

- (void)updateQueryResult:(NSString*)result {
  _responseContainer.text = result;
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

// Creates menu for tab grouping strategy.
- (UIMenu*)createTabGroupingStrategyMenu {
  NSMutableArray<UIAction*>* strategies = [NSMutableArray array];

#if BUILDFLAG(BUILD_WITH_INTERNAL_OPTIMIZATION_GUIDE)
  UIAction* topicBasedStrategy = [UIAction
      actionWithTitle:@"Topic based"
                image:nil
           identifier:nil
              handler:^(UIAction* action) {
                self.groupingStrategy = optimization_guide::proto::
                    TabOrganizationRequest_TabOrganizationModelStrategy_STRATEGY_TOPIC_BASED;
                self.groupingStrategyMenu.menu =
                    [self createTabGroupingStrategyMenu];
              }];
  [strategies addObject:topicBasedStrategy];
  UIAction* taskBasedStrategy = [UIAction
      actionWithTitle:@"Task based"
                image:nil
           identifier:nil
              handler:^(UIAction* action) {
                self.groupingStrategy = optimization_guide::proto::
                    TabOrganizationRequest_TabOrganizationModelStrategy_STRATEGY_TASK_BASED;
                self.groupingStrategyMenu.menu =
                    [self createTabGroupingStrategyMenu];
              }];
  [strategies addObject:taskBasedStrategy];
  UIAction* domainBasedStrategy = [UIAction
      actionWithTitle:@"Domain based"
                image:nil
           identifier:nil
              handler:^(UIAction* action) {
                self.groupingStrategy = optimization_guide::proto::
                    TabOrganizationRequest_TabOrganizationModelStrategy_STRATEGY_DOMAIN_BASED;
                self.groupingStrategyMenu.menu =
                    [self createTabGroupingStrategyMenu];
              }];
  [strategies addObject:domainBasedStrategy];

  switch (self.groupingStrategy) {
    case optimization_guide::proto::
        TabOrganizationRequest_TabOrganizationModelStrategy_STRATEGY_TOPIC_BASED:
      topicBasedStrategy.state = UIMenuElementStateOn;
      break;
    case optimization_guide::proto::
        TabOrganizationRequest_TabOrganizationModelStrategy_STRATEGY_TASK_BASED:
      taskBasedStrategy.state = UIMenuElementStateOn;
      break;
    case optimization_guide::proto::
        TabOrganizationRequest_TabOrganizationModelStrategy_STRATEGY_DOMAIN_BASED:
      domainBasedStrategy.state = UIMenuElementStateOn;
      break;
    default:
      break;
  }
#endif

  return [UIMenu menuWithTitle:@"Tab grouping strategy" children:strategies];
}

@end
