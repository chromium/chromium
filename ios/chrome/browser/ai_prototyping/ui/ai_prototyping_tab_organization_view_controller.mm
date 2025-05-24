// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ui/ai_prototyping_tab_organization_view_controller.h"

#import "base/logging.h"
#import "base/memory/raw_ptr.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/ai_prototyping/ui/ai_prototyping_mutator.h"
#import "ios/chrome/browser/ai_prototyping/utils/ai_prototyping_constants.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "components/optimization_guide/proto/features/tab_organization.pb.h"

using optimization_guide::proto::
    TabOrganizationRequest_TabOrganizationModelStrategy;
using optimization_guide::proto::
    TabOrganizationRequest_TabOrganizationModelStrategy_STRATEGY_DOMAIN_BASED;
using optimization_guide::proto::
    TabOrganizationRequest_TabOrganizationModelStrategy_STRATEGY_TASK_BASED;
using optimization_guide::proto::
    TabOrganizationRequest_TabOrganizationModelStrategy_STRATEGY_TOPIC_BASED;

@interface AIPrototypingTabOrganizationViewController () {
  UIButton* _groupTabsButton;
  UITextView* _responseContainer;
}

@property(nonatomic, strong) UIButton* groupingStrategyButton;

// The currently selected strategy for tab grouping.
@property(nonatomic, assign)
    TabOrganizationRequest_TabOrganizationModelStrategy groupingStrategy;

@end

@implementation AIPrototypingTabOrganizationViewController

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

  UILabel* label = [[UILabel alloc] init];
  label.translatesAutoresizingMaskIntoConstraints = NO;
  label.font = [UIFont preferredFontForTextStyle:UIFontTextStyleTitle2];
  label.text =
      l10n_util::GetNSString(IDS_IOS_AI_PROTOTYPING_TAB_ORGANIZATION_HEADER);

  UIColor* primaryColor = [UIColor colorNamed:kTextPrimaryColor];

  _groupingStrategyButton = [UIButton buttonWithType:UIButtonTypeSystem];
  _groupingStrategyButton.layer.borderColor = [primaryColor CGColor];
  _groupingStrategyButton.layer.borderWidth = kBorderWidth;
  _groupingStrategyButton.layer.cornerRadius = kCornerRadius;
  [_groupingStrategyButton setTitleColor:primaryColor
                                forState:UIControlStateNormal];
  _groupingStrategyButton.showsMenuAsPrimaryAction = YES;

  self.groupingStrategy =
      TabOrganizationRequest_TabOrganizationModelStrategy_STRATEGY_TOPIC_BASED;

  _groupingStrategyButton.menu = [self createTabGroupingStrategyMenu];

  _groupTabsButton = [UIButton buttonWithType:UIButtonTypeSystem];
  _groupTabsButton.backgroundColor = [UIColor colorNamed:kBlueColor];
  _groupTabsButton.layer.cornerRadius = kCornerRadius;
  [_groupTabsButton
      setTitle:l10n_util::GetNSString(
                   IDS_IOS_AI_PROTOTYPING_TAB_ORGANIZATION_GROUP_TABS_BUTTON)
      forState:UIControlStateNormal];
  [_groupTabsButton setTitleColor:primaryColor forState:UIControlStateNormal];
  [_groupTabsButton addTarget:self
                       action:@selector(onGroupTabsButtonPressed:)
             forControlEvents:UIControlEventTouchUpInside];

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
    label, _groupingStrategyButton, _groupTabsButton, _responseContainer
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

- (void)onGroupTabsButtonPressed:(UIButton*)button {
  [self disableSubmitButton];
  [self updateResponseField:@""];
  [self.mutator executeGroupTabsWithStrategy:self.groupingStrategy];
}

#pragma mark - AIPrototypingViewControllerProtocol

- (void)updateResponseField:(NSString*)response {
  _responseContainer.text = response;
}

- (void)enableSubmitButtons {
  _groupTabsButton.enabled = YES;
  _groupTabsButton.backgroundColor = [UIColor colorNamed:kBlueColor];
}

#pragma mark - Private

// Creates menu for tab grouping strategy.
- (UIMenu*)createTabGroupingStrategyMenu {
  NSMutableArray<UIAction*>* strategies = [NSMutableArray array];

  UIAction* topicBasedStrategy = [UIAction
      actionWithTitle:
          [self
              titleForGroupingStrategy:
                  TabOrganizationRequest_TabOrganizationModelStrategy_STRATEGY_TOPIC_BASED]
                image:nil
           identifier:nil
              handler:^(UIAction* action) {
                self.groupingStrategy =
                    TabOrganizationRequest_TabOrganizationModelStrategy_STRATEGY_TOPIC_BASED;
                self.groupingStrategyButton.menu =
                    [self createTabGroupingStrategyMenu];
              }];
  [strategies addObject:topicBasedStrategy];
  UIAction* taskBasedStrategy = [UIAction
      actionWithTitle:
          [self
              titleForGroupingStrategy:
                  TabOrganizationRequest_TabOrganizationModelStrategy_STRATEGY_TASK_BASED]
                image:nil
           identifier:nil
              handler:^(UIAction* action) {
                self.groupingStrategy =
                    TabOrganizationRequest_TabOrganizationModelStrategy_STRATEGY_TASK_BASED;
                self.groupingStrategyButton.menu =
                    [self createTabGroupingStrategyMenu];
              }];
  [strategies addObject:taskBasedStrategy];
  UIAction* domainBasedStrategy = [UIAction
      actionWithTitle:
          [self
              titleForGroupingStrategy:
                  TabOrganizationRequest_TabOrganizationModelStrategy_STRATEGY_DOMAIN_BASED]
                image:nil
           identifier:nil
              handler:^(UIAction* action) {
                self.groupingStrategy =
                    TabOrganizationRequest_TabOrganizationModelStrategy_STRATEGY_DOMAIN_BASED;
                self.groupingStrategyButton.menu =
                    [self createTabGroupingStrategyMenu];
              }];
  [strategies addObject:domainBasedStrategy];

  switch (self.groupingStrategy) {
    case TabOrganizationRequest_TabOrganizationModelStrategy_STRATEGY_TOPIC_BASED:
      topicBasedStrategy.state = UIMenuElementStateOn;
      break;
    case TabOrganizationRequest_TabOrganizationModelStrategy_STRATEGY_TASK_BASED:
      taskBasedStrategy.state = UIMenuElementStateOn;
      break;
    case TabOrganizationRequest_TabOrganizationModelStrategy_STRATEGY_DOMAIN_BASED:
      domainBasedStrategy.state = UIMenuElementStateOn;
      break;
    default:
      NOTREACHED();
  }

  return [UIMenu
      menuWithTitle:
          l10n_util::GetNSString(
              IDS_IOS_AI_PROTOTYPING_TAB_ORGANIZATION_GROUPING_STRATEGY_LABEL)
           children:strategies];
}

- (void)setGroupingStrategy:
    (TabOrganizationRequest_TabOrganizationModelStrategy)groupingStrategy {
  _groupingStrategy = groupingStrategy;
  [_groupingStrategyButton
      setTitle:
          [NSString
              stringWithFormat:
                  @"%@: %@",
                  l10n_util::GetNSString(
                      IDS_IOS_AI_PROTOTYPING_TAB_ORGANIZATION_GROUPING_STRATEGY_LABEL),
                  [self titleForGroupingStrategy:groupingStrategy]]
      forState:UIControlStateNormal];
}

- (NSString*)titleForGroupingStrategy:
    (TabOrganizationRequest_TabOrganizationModelStrategy)groupingStrategy {
  switch (groupingStrategy) {
    case TabOrganizationRequest_TabOrganizationModelStrategy_STRATEGY_TOPIC_BASED:
      return l10n_util::GetNSString(
          IDS_IOS_AI_PROTOTYPING_TAB_ORGANIZATION_GROUPING_STRATEGY_TOPIC);
    case TabOrganizationRequest_TabOrganizationModelStrategy_STRATEGY_TASK_BASED:
      return l10n_util::GetNSString(
          IDS_IOS_AI_PROTOTYPING_TAB_ORGANIZATION_GROUPING_STRATEGY_TASK);
    case TabOrganizationRequest_TabOrganizationModelStrategy_STRATEGY_DOMAIN_BASED:
      return l10n_util::GetNSString(
          IDS_IOS_AI_PROTOTYPING_TAB_ORGANIZATION_GROUPING_STRATEGY_DOMAIN);
    default:
      NOTREACHED();
  }
}

// Disable submit button, and style the accordingly.
- (void)disableSubmitButton {
  _groupTabsButton.enabled = NO;
  _groupTabsButton.backgroundColor = [UIColor colorNamed:kDisabledTintColor];
}

@end
