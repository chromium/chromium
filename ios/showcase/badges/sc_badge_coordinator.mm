// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/showcase/badges/sc_badge_coordinator.h"

#import "ios/chrome/browser/infobars/badge_state.h"
#import "ios/chrome/browser/ui/badges/badge_button_factory.h"
#import "ios/chrome/browser/ui/badges/badge_consumer.h"
#import "ios/chrome/browser/ui/badges/badge_delegate.h"
#import "ios/chrome/browser/ui/badges/badge_popup_menu_coordinator.h"
#import "ios/chrome/browser/ui/badges/badge_static_item.h"
#import "ios/chrome/browser/ui/badges/badge_tappable_item.h"
#import "ios/chrome/browser/ui/badges/badge_view_controller.h"
#import "ios/chrome/browser/ui/util/named_guide.h"
#import "ios/chrome/browser/ui/util/named_guide_util.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/showcase/badges/sc_badge_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface BadgeContainerViewController : UIViewController
@property(nonatomic, strong) BadgeViewController* centeredChildViewController;
@property(nonatomic, weak) id<BadgeConsumer> consumer;
@end

@implementation BadgeContainerViewController
- (void)viewDidLoad {
  [super viewDidLoad];

  UIStackView* containerStack = [[UIStackView alloc] init];
  containerStack.translatesAutoresizingMaskIntoConstraints = NO;
  containerStack.axis = UILayoutConstraintAxisVertical;

  [self addChildViewController:self.centeredChildViewController];
  [containerStack addArrangedSubview:self.centeredChildViewController.view];
  [self didMoveToParentViewController:self.centeredChildViewController];

  UIStackView* buttonStackView = [[UIStackView alloc] init];
  buttonStackView.axis = UILayoutConstraintAxisHorizontal;
  UIButton* showAcceptedBadgeButton =
      [UIButton buttonWithType:UIButtonTypeSystem];
  showAcceptedBadgeButton.accessibilityIdentifier =
      kSCShowAcceptedDisplayedBadgeButton;
  showAcceptedBadgeButton.titleLabel.lineBreakMode = NSLineBreakByWordWrapping;
  showAcceptedBadgeButton.titleLabel.textAlignment = NSTextAlignmentCenter;
  [showAcceptedBadgeButton setTitle:@"Show Accepted Badge"
                           forState:UIControlStateNormal];
  [showAcceptedBadgeButton addTarget:self
                              action:@selector(showAcceptedDisplayedBadge)
                    forControlEvents:UIControlEventTouchUpInside];
  [buttonStackView addArrangedSubview:showAcceptedBadgeButton];

  UIButton* showOverflowBadgeButton =
      [UIButton buttonWithType:UIButtonTypeSystem];
  showOverflowBadgeButton.accessibilityIdentifier =
      kSCShowOverflowDisplayedBadgeButton;
  showOverflowBadgeButton.titleLabel.lineBreakMode = NSLineBreakByWordWrapping;
  showOverflowBadgeButton.titleLabel.textAlignment = NSTextAlignmentCenter;
  [showOverflowBadgeButton setTitle:@"Show Overflow badge"
                           forState:UIControlStateNormal];
  [showOverflowBadgeButton addTarget:self
                              action:@selector(addSecondBadge:)
                    forControlEvents:UIControlEventTouchUpInside];
  [buttonStackView addArrangedSubview:showOverflowBadgeButton];

  [containerStack addArrangedSubview:buttonStackView];
  [self.view addSubview:containerStack];
  AddSameCenterConstraints(containerStack, self.view);
  [NSLayoutConstraint activateConstraints:@[
    [containerStack.widthAnchor constraintEqualToConstant:300],
    [containerStack.heightAnchor constraintGreaterThanOrEqualToConstant:100]
  ]];

  UIView* containerView = self.view;
  containerView.backgroundColor = [UIColor whiteColor];

  self.title = @"Badges";
  AddNamedGuidesToView(@[ kBadgeOverflowMenuGuide ], self.view);
  BadgeStaticItem* incognitoItem =
      [[BadgeStaticItem alloc] initWithBadgeType:kBadgeTypeIncognito];
  BadgeTappableItem* passwordBadgeItem =
      [[BadgeTappableItem alloc] initWithBadgeType:kBadgeTypePasswordSave];
  passwordBadgeItem.badgeState = BadgeStateRead;
  [self.consumer setupWithDisplayedBadge:passwordBadgeItem
                         fullScreenBadge:incognitoItem];
}

- (void)showAcceptedDisplayedBadge {
  BadgeStaticItem* incognitoItem =
      [[BadgeStaticItem alloc] initWithBadgeType:kBadgeTypeIncognito];
  BadgeTappableItem* passwordBadgeItem =
      [[BadgeTappableItem alloc] initWithBadgeType:kBadgeTypePasswordSave];
  passwordBadgeItem.badgeState = BadgeStateRead | BadgeStateAccepted;
  [self.consumer setupWithDisplayedBadge:passwordBadgeItem
                         fullScreenBadge:incognitoItem];
}

- (void)addSecondBadge:(id)sender {
  BadgeStaticItem* incognitoItem =
      [[BadgeStaticItem alloc] initWithBadgeType:kBadgeTypeIncognito];
  BadgeTappableItem* displayedBadge =
      [[BadgeTappableItem alloc] initWithBadgeType:kBadgeTypeOverflow];
  [self.consumer setupWithDisplayedBadge:displayedBadge
                         fullScreenBadge:incognitoItem];
  [self.consumer markDisplayedBadgeAsRead:NO];
}

@end

@interface SCBadgeCoordinator () <BadgeDelegate>
@property(nonatomic, strong)
    BadgeContainerViewController* containerViewController;
@property(nonatomic, strong) BadgeViewController* badgeViewController;
@property(nonatomic, weak) id<BadgeConsumer> consumer;
@property(nonatomic, strong)
    BadgePopupMenuCoordinator* badgePopupMenuCoordinator;
@end

@implementation SCBadgeCoordinator
@synthesize baseViewController = _baseViewController;

- (NSArray<NSNumber*>*)badgeTypesForOverflowMenu {
  return @[ @(kBadgeTypePasswordSave) ];
}

- (void)start {
  self.containerViewController = [[BadgeContainerViewController alloc] init];
  BadgeButtonFactory* buttonFactory = [[BadgeButtonFactory alloc] init];
  buttonFactory.delegate = self;
  self.badgeViewController =
      [[BadgeViewController alloc] initWithButtonFactory:buttonFactory];
  self.consumer = self.badgeViewController;
  self.containerViewController.consumer = self.badgeViewController;
  self.containerViewController.centeredChildViewController =
      self.badgeViewController;
  [self.baseViewController pushViewController:self.containerViewController
                                     animated:YES];
}

- (void)addToReadingListBadgeButtonTapped:(id)sender {
}

- (void)passwordsBadgeButtonTapped:(id)sender {
}

- (void)saveCardBadgeButtonTapped:(id)sender {
}

- (void)saveAddressProfileBadgeButtonTapped:(id)sender {
}

- (void)translateBadgeButtonTapped:(id)sender {
}

- (void)permissionsBadgeButtonTapped:(id)sender {
}

- (void)overflowBadgeButtonTapped:(id)sender {
  self.badgePopupMenuCoordinator = [[BadgePopupMenuCoordinator alloc]
      initWithBaseViewController:self.containerViewController
                         browser:nil];
  NSArray* badgeItems =
      @[ [[BadgeTappableItem alloc] initWithBadgeType:kBadgeTypePasswordSave] ];
  [self.badgePopupMenuCoordinator setBadgeItemsToShow:badgeItems];
  [self.badgePopupMenuCoordinator start];
  [self.consumer markDisplayedBadgeAsRead:YES];
}

- (void)showModalForBadgeType:(BadgeType)badgeType {
}

@end
