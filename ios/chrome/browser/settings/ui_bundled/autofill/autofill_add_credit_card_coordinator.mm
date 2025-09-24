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
#import "ios/chrome/browser/settings/ui_bundled/autofill/autofill_add_credit_card_view_controller_presentation_delegate.h"
#import "ios/chrome/browser/settings/ui_bundled/credit_card_scanner/credit_card_scanner_coordinator.h"
#import "ios/chrome/browser/shared/coordinator/alert/alert_coordinator.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "ui/strings/grit/ui_strings.h"

@interface AutofillAddCreditCardCoordinator () <
    AddCreditCardMediatorDelegate,
    AddCreditCardViewControllerPresentationDelegate,
    UIAdaptivePresentationControllerDelegate>

@end

@implementation AutofillAddCreditCardCoordinator {
  // Display alerts.
  UIAlertController* _alertController;

  // The Credit Card Scanner Coordinator.
  CreditCardScannerCoordinator* _creditCardScannerCoordinator;

  // The view controller attached to this coordinator.
  AutofillAddCreditCardViewController* _addCreditCardViewController;

  // The mediator for the view controller attatched to this coordinator.
  AutofillAddCreditCardMediator* _mediator;
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
  _addCreditCardViewController.presentationDelegate = self;

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
  [_alertController.presentingViewController dismissViewControllerAnimated:YES
                                                                completion:nil];
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

#pragma mark - AddCreditCardViewControllerPresentationDelegate

- (void)addCreditCardViewControllerRequestedCameraScan:
    (AutofillAddCreditCardViewController*)viewController {
  _creditCardScannerCoordinator = [[CreditCardScannerCoordinator alloc]
      initWithBaseViewController:_addCreditCardViewController
                         browser:self.browser
                        consumer:_addCreditCardViewController];

  [_creditCardScannerCoordinator start];
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

- (void)showActionSheetAlert {
  _alertController = [UIAlertController
      alertControllerWithTitle:
          l10n_util::GetNSString(
              IDS_IOS_ADD_CREDIT_CARD_VIEW_CONTROLLER_DISMISS_ALERT_TITLE)
                       message:nil
                preferredStyle:UIAlertControllerStyleActionSheet];

  _alertController.popoverPresentationController.barButtonItem =
      _addCreditCardViewController.navigationItem.leftBarButtonItem;
  _alertController.popoverPresentationController.permittedArrowDirections =
      UIPopoverArrowDirectionUp;

  __weak __typeof(self) weakSelf = self;
  UIAlertAction* dismissalAction = [UIAlertAction
      actionWithTitle:l10n_util::GetNSString(
                          IDS_IOS_VIEW_CONTROLLER_DISMISS_DISCARD_CHANGES)
                style:UIAlertActionStyleDestructive
              handler:^(UIAlertAction* action) {
                [weakSelf.delegate
                    autofillAddCreditCardCoordinatorWantsToBeStopped:weakSelf];
              }];
  [_alertController addAction:dismissalAction];

  [_alertController
      addAction:[UIAlertAction
                    actionWithTitle:
                        l10n_util::GetNSString(
                            IDS_IOS_VIEW_CONTROLLER_DISMISS_CANCEL_CHANGES)
                              style:UIAlertActionStyleCancel
                            handler:nil]];

  [_addCreditCardViewController presentViewController:_alertController
                                             animated:YES
                                           completion:nil];
}

// Shows alert with received message by `AlertCoordinator`.
- (void)showAlertWithMessage:(NSString*)message {
  _alertController =
      [UIAlertController alertControllerWithTitle:message
                                          message:nil
                                   preferredStyle:UIAlertControllerStyleAlert];
  [_alertController
      addAction:[UIAlertAction
                    actionWithTitle:l10n_util::GetNSString(IDS_APP_OK)
                              style:UIAlertActionStyleDefault
                            handler:nil]];

  [_addCreditCardViewController presentViewController:_alertController
                                             animated:YES
                                           completion:nil];
}

@end
