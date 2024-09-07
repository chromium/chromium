// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_sharing/family_promo_coordinator.h"

#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/family_promo_action_handler.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/family_promo_coordinator_delegate.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/family_promo_mediator.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/family_promo_view_controller.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/password_sharing_constants.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/password_sharing_metrics.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "url/gurl.h"

@interface FamilyPromoCoordinator () <FamilyPromoActionHandler,
                                      UIAdaptivePresentationControllerDelegate>

// Main view controller for this coordinator.
@property(nonatomic, strong) FamilyPromoViewController* viewController;

// Main mediator for this coordinator.
@property(nonatomic, strong) FamilyPromoMediator* mediator;

// Type of the family promo to be displayed.
@property(nonatomic, assign) FamilyPromoType familyPromoType;

@end

@implementation FamilyPromoCoordinator

- (instancetype)initWithFamilyPromoType:(FamilyPromoType)familyPromoType
                     baseViewController:(UIViewController*)viewController
                                browser:(Browser*)browser {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _familyPromoType = familyPromoType;
  }
  return self;
}

- (void)start {
  [super start];

  FamilyPromoViewController* viewController =
      [[FamilyPromoViewController alloc] init];
  viewController.actionHandler = self;
  viewController.presentationController.delegate = self;
  viewController.sheetPresentationController.detents = @[
    viewController.preferredHeightDetent,
    UISheetPresentationControllerDetent.largeDetent,
  ];
  self.viewController = viewController;

  FamilyPromoMediator* mediator = [[FamilyPromoMediator alloc]
      initWithFamilyPromoType:self.familyPromoType];
  mediator.consumer = viewController;
  self.mediator = mediator;

  [self.baseViewController presentViewController:viewController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [self.viewController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
  self.viewController = nil;
}

#pragma mark - FamilyPromoActionHandler

- (void)confirmationAlertPrimaryAction {
  LogPasswordSharingInteraction(
      PasswordSharingInteraction::kFamilyPromoGotItClicked);

  [self.delegate familyPromoCoordinatorWasDismissed:self];
}

- (void)createFamilyGroupLinkWasTapped {
  LogPasswordSharingInteraction(
      _familyPromoType == FamilyPromoType::kUserNotInFamilyGroup
          ? PasswordSharingInteraction::kFamilyPromoCreateFamilyGroupClicked
          : PasswordSharingInteraction::kFamilyPromoInviteFamilyMembersClicked);

  id<ApplicationCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ApplicationCommands);
  OpenNewTabCommand* command =
      [OpenNewTabCommand commandWithURLFromChrome:[self familyManagementURL]];
  [handler closeSettingsUIAndOpenURL:command];
  [self.delegate familyPromoCoordinatorWasDismissed:self];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  [self.delegate familyPromoCoordinatorWasDismissed:self];
}

#pragma mark - Private

// Returns family management url based on the `_familyPromoType`.
- (GURL)familyManagementURL {
  switch (_familyPromoType) {
    case FamilyPromoType::kUserNotInFamilyGroup:
      return GURL(kCreateFamilyGroupURL);
    case FamilyPromoType::kUserWithNoOtherFamilyMembers:
      return GURL(kManageFamilyGroupURL);
  }
}

@end
