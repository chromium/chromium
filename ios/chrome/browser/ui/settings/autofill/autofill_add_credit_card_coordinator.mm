// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/autofill/autofill_add_credit_card_coordinator.h"

#include "components/autofill/core/browser/personal_data_manager.h"
#import "components/autofill/ios/browser/personal_data_manager_observer_bridge.h"
#include "ios/chrome/browser/autofill/personal_data_manager_factory.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/ui/alert_coordinator/action_sheet_coordinator.h"
#import "ios/chrome/browser/ui/alert_coordinator/alert_coordinator.h"
#import "ios/chrome/browser/ui/settings/autofill/autofill_add_credit_card_mediator.h"
#import "ios/chrome/browser/ui/settings/autofill/autofill_add_credit_card_mediator_delegate.h"
#import "ios/chrome/browser/ui/settings/autofill/autofill_add_credit_card_view_controller.h"
#import "ios/chrome/browser/ui/settings/credit_card_scanner/credit_card_scanner_coordinator.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface AutofillAddCreditCardCoordinator () <
    AddCreditCardMediatorDelegate,
    UIAdaptivePresentationControllerDelegate>

// Displays message for invalid credit card data.
@property(nonatomic, strong) AlertCoordinator* alertCoordinator;

// The Credit Card Scanner Coordinator.
@property(nonatomic, strong)
    CreditCardScannerCoordinator* creditCardScannerCoordinator API_AVAILABLE(
        ios(13.0));

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
      autofill::PersonalDataManagerFactory::GetForBrowserState(
          self.browserState->GetOriginalChromeBrowserState());

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
  if (@available(iOS 13, *)) {
    [self.creditCardScannerCoordinator stop];
    self.creditCardScannerCoordinator = nil;
  }

  [self.addCreditCardViewController.navigationController
      dismissViewControllerAnimated:YES
                         completion:nil];
  self.addCreditCardViewController = nil;
  self.mediator = nil;
}

#pragma mark - AddCreditCardMediatorDelegate

- (void)creditCardMediatorDidFinish:(AutofillAddCreditCardMediator*)mediator {
  [self stop];
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

- (void)creditCardMediatorShowScanner:(AutofillAddCreditCardMediator*)mediator
    API_AVAILABLE(ios(13.0)) {
  self.creditCardScannerCoordinator = [[CreditCardScannerCoordinator alloc]
      initWithBaseViewController:self.addCreditCardViewController
              creditCardConsumer:self.addCreditCardViewController];

  [self.creditCardScannerCoordinator start];
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
                  [weakSelf stop];
                }
                 style:UIAlertActionStyleDestructive];

  [self.actionSheetCoordinator
      addItemWithTitle:l10n_util::GetNSString(
                           IDS_IOS_VIEW_CONTROLLER_DISMISS_CANCEL_CHANGES)
                action:nil
                 style:UIAlertActionStyleCancel];

  [self.actionSheetCoordinator start];
}

// Shows alert with received message by |AlertCoordinator|.
- (void)showAlertWithMessage:(NSString*)message {
  self.alertCoordinator = [[AlertCoordinator alloc]
      initWithBaseViewController:self.addCreditCardViewController
                           title:message
                         message:nil];

  [self.alertCoordinator start];
}

@end
