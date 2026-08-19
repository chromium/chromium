// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/payments/coordinator/autofill_credit_card_coordinator.h"

#import "base/check.h"
#import "base/check_op.h"
#import "components/autofill/core/browser/data_model/payments/credit_card.h"
#import "ios/chrome/browser/autofill/model/personal_data_manager_factory.h"
#import "ios/chrome/browser/settings/autofill/payments/coordinator/autofill_add_credit_card_coordinator.h"
#import "ios/chrome/browser/settings/autofill/payments/coordinator/autofill_add_credit_card_coordinator_delegate.h"
#import "ios/chrome/browser/settings/autofill/payments/coordinator/autofill_credit_card_coordinator_delegate.h"
#import "ios/chrome/browser/settings/autofill/payments/coordinator/autofill_cvc_storage_view_coordinator.h"
#import "ios/chrome/browser/settings/autofill/payments/coordinator/autofill_cvc_storage_view_coordinator_delegate.h"
#import "ios/chrome/browser/settings/autofill/payments/ui/autofill_credit_card_navigation_commands.h"
#import "ios/chrome/browser/settings/ui_bundled/autofill/autofill_credit_card_edit_table_view_controller.h"
#import "ios/chrome/browser/settings/ui_bundled/autofill/autofill_credit_card_table_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/browser_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"

@interface AutofillCreditCardCoordinator () <
    AutofillAddCreditCardCoordinatorDelegate,
    AutofillCreditCardNavigationCommands,
    AutofillCvcStorageViewCoordinatorDelegate>
@end

@implementation AutofillCreditCardCoordinator {
  AutofillCreditCardTableViewController* _viewController;
  AutofillAddCreditCardCoordinator* _addCreditCardCoordinator;
  AutofillCvcStorageViewCoordinator* _cvcStorageCoordinator;
}

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser {
  self = [super initWithBaseViewController:navigationController
                                   browser:browser];
  if (self) {
    _baseNavigationController = navigationController;
  }
  return self;
}

- (void)start {
  _viewController = [[AutofillCreditCardTableViewController alloc]
      initWithBrowser:self.browser];
  _viewController.navigationHandler = self;
  _viewController.shouldShowLevelUpPaymentMethodsWalkthroughIPH =
      self.shouldShowLevelUpPaymentMethodsWalkthroughIPH;

  CommandDispatcher* dispatcher = self.browser->GetCommandDispatcher();
  _viewController.sceneHandler = HandlerForProtocol(dispatcher, SceneCommands);
  _viewController.browserHandler =
      HandlerForProtocol(dispatcher, BrowserCommands);
  _viewController.settingsHandler =
      HandlerForProtocol(dispatcher, SettingsCommands);
  _viewController.snackbarHandler =
      HandlerForProtocol(dispatcher, SnackbarCommands);

  [self.baseNavigationController pushViewController:_viewController
                                           animated:YES];
}

- (void)stop {
  [self stopAutofillAddCreditCardCoordinator];
  [self stopCvcStorageCoordinator];
  _viewController.navigationHandler = nil;
  _viewController = nil;
}

#pragma mark - AutofillCreditCardNavigationCommands

- (void)handleDismiss {
  [self.delegate autofillCreditCardCoordinatorDidRemove:self];
}

- (void)showAddPaymentMethod {
  if (_addCreditCardCoordinator) {
    return;
  }
  _addCreditCardCoordinator = [[AutofillAddCreditCardCoordinator alloc]
      initWithBaseViewController:_viewController
                         browser:self.browser];
  _addCreditCardCoordinator.delegate = self;
  [_addCreditCardCoordinator start];
}

- (void)showCvcStorage {
  [self stopCvcStorageCoordinator];
  _cvcStorageCoordinator = [[AutofillCvcStorageViewCoordinator alloc]
      initWithBaseViewController:self.baseNavigationController
                         browser:self.browser];
  _cvcStorageCoordinator.delegate = self;
  [_cvcStorageCoordinator start];
}

- (void)showCreditCardDetails:(const autofill::CreditCard&)creditCard {
  autofill::PersonalDataManager* personalDataManager =
      autofill::PersonalDataManagerFactory::GetForProfile(
          self.browser->GetProfile());
  AutofillCreditCardEditTableViewController* editController =
      [[AutofillCreditCardEditTableViewController alloc]
           initWithCreditCard:creditCard
          personalDataManager:personalDataManager];
  CommandDispatcher* dispatcher = self.browser->GetCommandDispatcher();
  editController.sceneHandler = HandlerForProtocol(dispatcher, SceneCommands);
  editController.browserHandler =
      HandlerForProtocol(dispatcher, BrowserCommands);
  editController.settingsHandler =
      HandlerForProtocol(dispatcher, SettingsCommands);
  editController.snackbarHandler =
      HandlerForProtocol(dispatcher, SnackbarCommands);
  [self.baseNavigationController pushViewController:editController
                                           animated:YES];
}

#pragma mark - AutofillAddCreditCardCoordinatorDelegate

- (void)autofillAddCreditCardCoordinatorWantsToBeStopped:
    (AutofillAddCreditCardCoordinator*)coordinator {
  CHECK_EQ(coordinator, _addCreditCardCoordinator);
  [self stopAutofillAddCreditCardCoordinator];
}

#pragma mark - AutofillCvcStorageViewCoordinatorDelegate

- (void)autofillCvcStorageCoordinatorWantsToBeStopped:
    (AutofillCvcStorageViewCoordinator*)coordinator {
  DCHECK_EQ(coordinator, _cvcStorageCoordinator);
  [self stopCvcStorageCoordinator];
}

#pragma mark - Private

- (void)stopAutofillAddCreditCardCoordinator {
  [_addCreditCardCoordinator stop];
  _addCreditCardCoordinator.delegate = nil;
  _addCreditCardCoordinator = nil;
}

- (void)stopCvcStorageCoordinator {
  [_cvcStorageCoordinator stop];
  _cvcStorageCoordinator = nil;
}

@end
