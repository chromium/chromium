// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/autofill/autofill_add_credit_card_coordinator.h"

#import "components/autofill/core/browser/data_manager/personal_data_manager.h"
#import "components/autofill/ios/browser/personal_data_manager_observer_bridge.h"
#import "ios/chrome/browser/autofill/model/personal_data_manager_factory.h"
#import "ios/chrome/browser/settings/ui_bundled/autofill/autofill_add_credit_card_coordinator_delegate.h"
#import "ios/chrome/browser/settings/ui_bundled/autofill/autofill_add_credit_card_mediator.h"
#import "ios/chrome/browser/settings/ui_bundled/autofill/autofill_add_credit_card_mediator_delegate.h"
#import "ios/chrome/browser/settings/ui_bundled/autofill/autofill_add_credit_card_view_controller.h"
#import "ios/chrome/browser/shared/coordinator/alert/action_sheet_coordinator.h"
#import "ios/chrome/browser/shared/coordinator/alert/alert_coordinator.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

@interface AutofillAddCreditCardCoordinator () <
    AddCreditCardMediatorDelegate,
    UIAdaptivePresentationControllerDelegate>

@end

@implementation AutofillAddCreditCardCoordinator {
  // Displays message for invalid credit card data.
  AlertCoordinator* _alertCoordinator;

  // The view controller attached to this coordinator.
  AutofillAddCreditCardViewController* _addCreditCardViewController;

  // The mediator for the view controller attatched to this coordinator.
  AutofillAddCreditCardMediator* _mediator;

  // The action sheet coordinator, if one is currently being shown.
  ActionSheetCoordinator* _actionSheetCoordinator;
}

- (void)start {
  // There is no personal data manager in OTR (incognito). Get the original
  // one so the user can add credit cards.
  autofill::PersonalDataManager* personalDataManager =
      autofill::PersonalDataManagerFactory::GetForProfile(
          self.profile->GetOriginalProfile());

  _mediator = [[AutofillAddCreditCardMediator alloc]
         initWithDelegate:self
      personalDataManager:personalDataManager];

  _addCreditCardViewController =
      [[AutofillAddCreditCardViewController alloc] initWithDelegate:_mediator];

  UINavigationController* navigationController = [[UINavigationController alloc]
      initWithRootViewController:_addCreditCardViewController];
  navigationController.modalPresentationStyle = UIModalPresentationFormSheet;
  navigationController.presentationController.delegate = self;

  [self.baseViewController presentViewController:navigationController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [_addCreditCardViewController.navigationController
      dismissViewControllerAnimated:YES
                         completion:nil];
  _addCreditCardViewController = nil;
  [self dismissActionSheetCoordinator];
  _mediator = nil;
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
  return !_addCreditCardViewController.tableViewHasUserInput;
}

- (void)presentationControllerDidAttemptToDismiss:
    (UIPresentationController*)presentationController {
  [self showActionSheetAlert];
}

#pragma mark - Helper Methods

// Shows action sheet alert with a discard changes and a cancel action.
- (void)showActionSheetAlert {
  _actionSheetCoordinator = [[ActionSheetCoordinator alloc]
      initWithBaseViewController:_addCreditCardViewController
                         browser:self.browser
                           title:
                               l10n_util::GetNSString(
                                   IDS_IOS_ADD_CREDIT_CARD_VIEW_CONTROLLER_DISMISS_ALERT_TITLE)
                         message:nil
                   barButtonItem:_addCreditCardViewController.navigationItem
                                     .leftBarButtonItem];

  _actionSheetCoordinator.popoverArrowDirection = UIPopoverArrowDirectionUp;
  __weak __typeof(self) weakSelf = self;

  [_actionSheetCoordinator
      addItemWithTitle:l10n_util::GetNSString(
                           IDS_IOS_VIEW_CONTROLLER_DISMISS_DISCARD_CHANGES)
                action:^{
                  [weakSelf.delegate
                      autofillAddCreditCardCoordinatorWantsToBeStopped:
                          weakSelf];
                  [weakSelf dismissActionSheetCoordinator];
                }
                 style:UIAlertActionStyleDestructive];

  [_actionSheetCoordinator
      addItemWithTitle:l10n_util::GetNSString(
                           IDS_IOS_VIEW_CONTROLLER_DISMISS_CANCEL_CHANGES)
                action:^{
                  [weakSelf dismissActionSheetCoordinator];
                }
                 style:UIAlertActionStyleCancel];

  [_actionSheetCoordinator start];
}

// Shows alert with received message by `AlertCoordinator`.
- (void)showAlertWithMessage:(NSString*)message {
  _alertCoordinator = [[AlertCoordinator alloc]
      initWithBaseViewController:_addCreditCardViewController
                         browser:self.browser
                           title:message
                         message:nil];

  [_alertCoordinator start];
}

#pragma mark - Private

- (void)dismissActionSheetCoordinator {
  [_actionSheetCoordinator stop];
  _actionSheetCoordinator = nil;
}

@end
