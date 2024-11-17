// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/autofill/autofill_add_credit_card_coordinator.h"

#import "components/autofill/core/browser/personal_data_manager.h"
#import "components/autofill/ios/browser/personal_data_manager_observer_bridge.h"
#import "ios/chrome/browser/autofill/model/personal_data_manager_factory.h"
#import "ios/chrome/browser/shared/coordinator/alert/action_sheet_coordinator.h"
#import "ios/chrome/browser/shared/coordinator/alert/alert_coordinator.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/ui/settings/autofill/autofill_add_credit_card_coordinator_delegate.h"
#import "ios/chrome/browser/ui/settings/autofill/autofill_add_credit_card_mediator.h"
#import "ios/chrome/browser/ui/settings/autofill/autofill_add_credit_card_mediator_delegate.h"
#import "ios/chrome/browser/ui/settings/autofill/autofill_add_credit_card_view_controller.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

@interface AutofillAddCreditCardCoordinator () <
    AddCreditCardMediatorDelegate,
    UIAdaptivePresentationControllerDelegate>

// Displays message for invalid credit card data.
@property(nonatomic, strong) AlertCoordinator* alertCoordinator;

// The view controller attached to this coordinator.
@property(nonatomic, strong)
    AutofillAddCreditCardViewController* addCreditCardViewController;

// The mediator for the view controller attatched to this coordinator.
@property(nonatomic, strong) AutofillAddCreditCardMediator* mediator;

// The action sheet coordinator, if one is currently being shown.
@property(nonatomic, strong) ActionSheetCoordinator* actionSheetCoordinator;

@end

@implementation AutofillAddCreditCardCoordinator

- (void)start {
  // There is no personal data manager in OTR (incognito). Get the original
  // one so the user can add credit cards.
  autofill::PersonalDataManager* personalDataManager =
      autofill::PersonalDataManagerFactory::GetForProfile(
          self.browser->GetProfile()->GetOriginalProfile());

  self.mediator = [[AutofillAddCreditCardMediator alloc]
         initWithDelegate:self
      personalDataManager:personalDataManager];

  self.addCreditCardViewController =
      [[AutofillAddCreditCardViewController alloc]
          initWithDelegate:self.mediator];

  UINavigationController* navigationController = [[UINavigationController alloc]
      initWithRootViewController:self.addCreditCardViewController];
  navigationController.modalPresentationStyle = UIModalPresentationFormSheet;
  navigationController.presentationController.delegate = self;

  [self.baseViewController presentViewController:navigationController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [self.addCreditCardViewController.navigationController
      dismissViewControllerAnimated:YES
                         completion:nil];
  self.addCreditCardViewController = nil;
  [self dismissActionSheetCoordinator];
  self.mediator = nil;
}

#pragma mark - AddCreditCardMediatorDelegate

- (void)creditCardMediatorDidFinish:(AutofillAddCreditCardMediator*)mediator {
  [self.delegate autofillAddCreditCardCoordinatorWantsToBeStopped:self];
}

- (void)creditCardMediatorHasInvalidCardNumber:
    (AutofillAddCreditCardMediator*)mediator {
  [self showAlertWithMessage:
            l10n_util::GetNSString(
                IDS_IOS_ADD_CREDIT_CARD_INVALID_CARD_NUMBER_ALERT)];
}

- (void)creditCardMediatorHasInvalidExpirationDate:
    (AutofillAddCreditCardMediator*)mediator {
  [self showAlertWithMessage:
            l10n_util::GetNSString(
                IDS_IOS_ADD_CREDIT_CARD_INVALID_EXPIRATION_DATE_ALERT)];
}

- (void)creditCardMediatorHasInvalidNickname:
    (AutofillAddCreditCardMediator*)mediator {
  [self
      showAlertWithMessage:l10n_util::GetNSString(
                               IDS_IOS_ADD_CREDIT_CARD_INVALID_NICKNAME_ALERT)];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (BOOL)presentationControllerShouldDismiss:
    (UIPresentationController*)presentationController {
  return !self.addCreditCardViewController.tableViewHasUserInput;
}

- (void)presentationControllerDidAttemptToDismiss:
    (UIPresentationController*)presentationController {
  [self showActionSheetAlert];
}

#pragma mark - Helper Methods

// Shows action sheet alert with a discard changes and a cancel action.
- (void)showActionSheetAlert {
  self.actionSheetCoordinator = [[ActionSheetCoordinator alloc]
      initWithBaseViewController:self.addCreditCardViewController
                         browser:self.browser
                           title:
                               l10n_util::GetNSString(
                                   IDS_IOS_ADD_CREDIT_CARD_VIEW_CONTROLLER_DISMISS_ALERT_TITLE)
                         message:nil
                   barButtonItem:self.addCreditCardViewController.navigationItem
                                     .leftBarButtonItem];

  self.actionSheetCoordinator.popoverArrowDirection = UIPopoverArrowDirectionUp;
  __weak __typeof(self) weakSelf = self;

  [self.actionSheetCoordinator
      addItemWithTitle:l10n_util::GetNSString(
                           IDS_IOS_VIEW_CONTROLLER_DISMISS_DISCARD_CHANGES)
                action:^{
                  [weakSelf.delegate
                      autofillAddCreditCardCoordinatorWantsToBeStopped:
                          weakSelf];
                  [weakSelf dismissActionSheetCoordinator];
                }
                 style:UIAlertActionStyleDestructive];

  [self.actionSheetCoordinator
      addItemWithTitle:l10n_util::GetNSString(
                           IDS_IOS_VIEW_CONTROLLER_DISMISS_CANCEL_CHANGES)
                action:^{
                  [weakSelf dismissActionSheetCoordinator];
                }
                 style:UIAlertActionStyleCancel];

  [self.actionSheetCoordinator start];
}

// Shows alert with received message by `AlertCoordinator`.
- (void)showAlertWithMessage:(NSString*)message {
  self.alertCoordinator = [[AlertCoordinator alloc]
      initWithBaseViewController:self.addCreditCardViewController
                         browser:self.browser
                           title:message
                         message:nil];

  [self.alertCoordinator start];
}

#pragma mark - Private

- (void)dismissActionSheetCoordinator {
  [self.actionSheetCoordinator stop];
  self.actionSheetCoordinator = nil;
}

@end
