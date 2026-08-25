// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ui/ai_prototyping_worklog_item_showcase_view_controller.h"

#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_item_view.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_view_data.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

const CGFloat kLayoutSpacing = 16.0;
const CGFloat kSymbolSize = 16.0;

// Struct for describing showcase items with layouts and configurations.
struct ShowcaseItemConfig {
  NSString* title;
  NSString* subtitle = nil;
  BOOL active = YES;
  ActuationWorklogItemStyle style = ActuationWorklogItemStyle::kCard;
  ActuationWorklogConnectorVisibility visibility;
};

// Helper to create and configure an ActuationWorklogItemView from a config.
ActuationWorklogItemView* CreateWorklogItemView(
    const ShowcaseItemConfig& config) {
  UIImage* icon = SymbolWithPointSize(SymbolPersonBadgeKeyFill, kSymbolSize);
  ActuationWorklogItem* item;
  switch (config.style) {
    case ActuationWorklogItemStyle::kSimple:
      item = [ActuationWorklogItem simpleItemWithTitle:config.title
                                                active:config.active];
      break;
    case ActuationWorklogItemStyle::kLabeled:
      item = [ActuationWorklogItem labeledItemWithTitle:config.title
                                               subtitle:config.subtitle
                                                   icon:icon
                                                 active:config.active];
      break;
    case ActuationWorklogItemStyle::kCard:
      item = [ActuationWorklogItem cardItemWithTitle:config.title
                                            subtitle:config.subtitle
                                                icon:icon
                                              active:config.active];
      break;
  }

  ActuationWorklogItemView* view = [[ActuationWorklogItemView alloc] init];
  [view configureWithItem:item];
  view.connectorVisibility = config.visibility;
  return view;
}

}  // namespace

@interface AIPrototypingWorklogItemShowcaseViewController () <
    ActuationWorklogItemViewDelegate>
@end

@implementation AIPrototypingWorklogItemShowcaseViewController {
  UIScrollView* _scrollView;
  UIStackView* _mainStackView;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.title = @"Worklog Steps";
  self.view.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];

  _scrollView = [[UIScrollView alloc] init];
  _scrollView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:_scrollView];

  _mainStackView = [[UIStackView alloc] init];
  _mainStackView.axis = UILayoutConstraintAxisVertical;
  _mainStackView.spacing = kLayoutSpacing;
  _mainStackView.translatesAutoresizingMaskIntoConstraints = NO;
  [_scrollView addSubview:_mainStackView];

  [self setupConstraints];
  [self addTimelineSequenceSection];
  [self addCardAccessoryShowcaseSection];
  [self addConnectorVisibilitiesSection];
}

#pragma mark - Private

// Configures the Auto Layout constraints.
- (void)setupConstraints {
  AddSameConstraintsWithInsets(
      _scrollView, self.view.safeAreaLayoutGuide,
      NSDirectionalEdgeInsetsMake(kLayoutSpacing, 0.0, 0.0, 0.0));
  AddSameConstraintsWithInset(_mainStackView, _scrollView.contentLayoutGuide,
                              kLayoutSpacing);
  [_mainStackView.widthAnchor
      constraintEqualToAnchor:_scrollView.frameLayoutGuide.widthAnchor
                     constant:-2.0 * kLayoutSpacing]
      .active = YES;
}

// Builds the connected timeline sequence section.
- (void)addTimelineSequenceSection {
  [self addHeaderTitle:@"Connected Timeline Sequence"];

  UIStackView* timelineStack = [[UIStackView alloc] init];
  timelineStack.axis = UILayoutConstraintAxisVertical;
  timelineStack.spacing = 0.0;
  timelineStack.translatesAutoresizingMaskIntoConstraints = NO;
  [_mainStackView addArrangedSubview:timelineStack];

  const ShowcaseItemConfig timelineConfigs[] = {
      {.title = @"Timeline Step 1 (Start)",
       .active = NO,
       .style = ActuationWorklogItemStyle::kSimple,
       .visibility = ActuationWorklogConnectorVisibility::kBottom},
      {.title = @"Timeline Step 2 (Active Simple)",
       .style = ActuationWorklogItemStyle::kSimple,
       .visibility = ActuationWorklogConnectorVisibility::kBoth},
      {.title = @"Timeline Step 3 (Active Labeled)",
       .subtitle = @"Detailed action step showing active state.",
       .style = ActuationWorklogItemStyle::kLabeled,
       .visibility = ActuationWorklogConnectorVisibility::kBoth},
      {.title = @"Timeline Step 4 (End Card)",
       .subtitle = @"Final step in card style.",
       .active = NO,
       .style = ActuationWorklogItemStyle::kCard,
       .visibility = ActuationWorklogConnectorVisibility::kTop},
  };

  for (const ShowcaseItemConfig& config : timelineConfigs) {
    [timelineStack addArrangedSubview:CreateWorklogItemView(config)];
  }
}

// Builds the card layout sub-card accessories showcase section.
- (void)addCardAccessoryShowcaseSection {
  [self addHeaderTitle:@"Card Sub-Card Accessories"];

  // 1. Account / Credential Accessory Card.
  ActuationWorklogAccessoryItem* credentialAccessory =
      [[ActuationWorklogAccessoryItem alloc]
          initWithIcon:SymbolWithPointSize(SymbolPersonCropCircle, 24.0)
                 title:@"Elisa Beckett"
              subtitle:@"elisa.beckett@nipon.com"
            detailText:nil
            hasChevron:YES];

  ActuationWorklogItem* credentialCardItem = [ActuationWorklogItem
      cardItemWithTitle:@"Sign in to amctheatres.com"
               subtitle:@"We can use your saved info in Waffle to sign in "
                        @"for you."
                   icon:SymbolWithPointSize(SymbolPersonBadgeKeyFill,
                                            kSymbolSize)
                 active:YES
          accessoryItem:credentialAccessory];

  ActuationWorklogItemView* credentialView =
      [[ActuationWorklogItemView alloc] init];
  credentialView.delegate = self;
  [credentialView configureWithItem:credentialCardItem];
  credentialView.connectorVisibility =
      ActuationWorklogConnectorVisibility::kNone;
  [_mainStackView addArrangedSubview:credentialView];

  // 2. Payment Info Accessory Card.
  ActuationWorklogAccessoryItem* paymentAccessory =
      [[ActuationWorklogAccessoryItem alloc]
          initWithIcon:SymbolWithPointSize(SymbolCreditCard, 24.0)
                 title:@"Niponcarte"
              subtitle:@"••••3222"
            detailText:@"05/28"
            hasChevron:YES];

  ActuationWorklogItem* paymentCardItem = [ActuationWorklogItem
      cardItemWithTitle:@"Filling payment info"
               subtitle:
                   @"To continue the task, Interthings can ask Waffle Ballet "
                   @"to fill out your credit card info."
                   icon:SymbolWithPointSize(SymbolCreditCard, kSymbolSize)
                 active:YES
          accessoryItem:paymentAccessory];

  ActuationWorklogItemView* paymentView =
      [[ActuationWorklogItemView alloc] init];
  paymentView.delegate = self;
  [paymentView configureWithItem:paymentCardItem];
  paymentView.connectorVisibility = ActuationWorklogConnectorVisibility::kNone;
  [_mainStackView addArrangedSubview:paymentView];
}

// Builds the individual connector visibilities showcase section.
- (void)addConnectorVisibilitiesSection {
  [self addHeaderTitle:@"Individual Connector Visibilities"];
  const ShowcaseItemConfig showcaseConfigs[] = {
      {.title = @"Visibility: None (No lines)",
       .subtitle = @"Showcasing line connector configuration.",
       .visibility = ActuationWorklogConnectorVisibility::kNone},
      {.title = @"Visibility: Bottom (Bottom line only)",
       .subtitle = @"Showcasing line connector configuration.",
       .visibility = ActuationWorklogConnectorVisibility::kBottom},
      {.title = @"Visibility: Both (Top & Bottom lines)",
       .subtitle = @"Showcasing line connector configuration.",
       .visibility = ActuationWorklogConnectorVisibility::kBoth},
      {.title = @"Visibility: Top (Top line only)",
       .subtitle = @"Showcasing line connector configuration.",
       .visibility = ActuationWorklogConnectorVisibility::kTop},
  };

  for (const ShowcaseItemConfig& config : showcaseConfigs) {
    [_mainStackView addArrangedSubview:CreateWorklogItemView(config)];
  }
}

// Adds a section header title.
- (void)addHeaderTitle:(NSString*)title {
  UILabel* header = [[UILabel alloc] init];
  header.text = title;
  header.font = [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
  header.textColor = [UIColor colorNamed:kTextPrimaryColor];
  [_mainStackView addArrangedSubview:header];
}

#pragma mark - ActuationWorklogItemViewDelegate

- (void)worklogItemViewDidTapItem:(ActuationWorklogItemView*)itemView {
  // Showcase item tap demo handler.
}

- (void)worklogItemView:(ActuationWorklogItemView*)itemView
    didTapAccessoryItem:(ActuationWorklogAccessoryItem*)accessoryItem {
  NSString* message = [NSString
      stringWithFormat:@"Tapped accessory item: %@", accessoryItem.title];
  UIAlertController* alert =
      [UIAlertController alertControllerWithTitle:@"Accessory Card Tapped"
                                          message:message
                                   preferredStyle:UIAlertControllerStyleAlert];
  [alert addAction:[UIAlertAction actionWithTitle:@"OK"
                                            style:UIAlertActionStyleDefault
                                          handler:nil]];
  [self presentViewController:alert animated:YES completion:nil];
}

@end
