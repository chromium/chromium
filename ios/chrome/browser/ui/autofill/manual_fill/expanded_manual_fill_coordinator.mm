// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/manual_fill/expanded_manual_fill_coordinator.h"

#import "components/autofill/core/common/unique_ids.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/address_coordinator.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/card_coordinator.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/expanded_manual_fill_view_controller.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_constants.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_password_coordinator.h"
#import "ios/web/public/web_state.h"
#import "url/gurl.h"

using manual_fill::ManualFillDataType;

@interface ExpandedManualFillCoordinator () <
    ExpandedManualFillViewControllerDelegate,
    AddressCoordinatorDelegate,
    CardCoordinatorDelegate>

// Main view controller for this coordinator.
@property(nonatomic, strong)
    ExpandedManualFillViewController* expandedManualFillViewController;

@end

@implementation ExpandedManualFillCoordinator {
  // Initial data type to present in the expanded manual fill view.
  ManualFillDataType _initialDataType;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                               forDataType:(ManualFillDataType)dataType {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _initialDataType = dataType;
  }
  return self;
}

- (void)start {
  self.expandedManualFillViewController =
      [[ExpandedManualFillViewController alloc]
          initWithDelegate:self
               forDataType:_initialDataType];

  [self showManualFillingOptionsForDataType:_initialDataType];
}

- (void)stop {
  [self stopChildCoordinators];
  self.expandedManualFillViewController = nil;
}

- (UIViewController*)viewController {
  return self.expandedManualFillViewController;
}

#pragma mark - ExpandedManualFillViewControllerDelegate

- (void)expandedManualFillViewController:
            (ExpandedManualFillViewController*)expandedManualFillViewController
                     didPressCloseButton:(UIButton*)closeButton {
  [self.delegate stopExpandedManualFillCoordinator:self];
}

- (void)expandedManualFillViewController:
            (ExpandedManualFillViewController*)expandedManualFillViewController
                  didSelectSegmentOfType:(ManualFillDataType)dataType {
  [self showManualFillingOptionsForDataType:dataType];
}

#pragma mark - FallbackCoordinatorDelegate

- (void)fallbackCoordinatorDidDismissPopover:
    (FallbackCoordinator*)fallbackCoordinator {
  // No-op as the expanded manual fill view is never presented as a popover for
  // now.
}

#pragma mark - CardCoordinatorDelegate

- (void)openCardSettings {
  //  TODO(b/40942168): Implement logic.
}

- (void)openAddCreditCard {
  //  TODO(b/40942168): Implement logic.
}

#pragma mark - AddressCoordinatorDelegate

- (void)openAddressSettings {
  //  TODO(b/40942168): Implement logic.
}

#pragma mark - Private

// Stops and deletes all active child coordinators.
- (void)stopChildCoordinators {
  for (ChromeCoordinator* coordinator in self.childCoordinators) {
    [coordinator stop];
  }
  [self.childCoordinators removeAllObjects];
}

// Shows the right manual filling options depending on `dataType`.
- (void)showManualFillingOptionsForDataType:(ManualFillDataType)dataType {
  switch (dataType) {
    case ManualFillDataType::kPassword:
      [self showPasswordManualFillingOptions];
      break;
    case ManualFillDataType::kPaymentMethod:
      [self showPaymentMethodManualFillingOptions];
      break;
    case ManualFillDataType::kAddress:
      [self showAddressManualFillingOptions];
      break;
  }
}

// Shows the password manual filling options.
- (void)showPasswordManualFillingOptions {
  [self stopChildCoordinators];

  WebStateList* webStateList = self.browser->GetWebStateList();
  CHECK(webStateList->GetActiveWebState());
  const GURL& URL = webStateList->GetActiveWebState()->GetLastCommittedURL();

  ManualFillPasswordCoordinator* passwordCoordinator =
      [[ManualFillPasswordCoordinator alloc]
          initWithBaseViewController:self.baseViewController
                             browser:self.browser
                                 URL:URL
                    injectionHandler:self.injectionHandler
            invokedOnObfuscatedField:self.invokedOnObfuscatedField
                              formID:self.formID
                             frameID:self.frameID];
  passwordCoordinator.delegate = self.delegate;

  self.expandedManualFillViewController.childViewController =
      passwordCoordinator.viewController;

  [self.childCoordinators addObject:passwordCoordinator];
}

// Shows the payment method manual filling options.
- (void)showPaymentMethodManualFillingOptions {
  [self stopChildCoordinators];

  CardCoordinator* cardCoordinator = [[CardCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:self.browser
                injectionHandler:self.injectionHandler];
  cardCoordinator.delegate = self;

  self.expandedManualFillViewController.childViewController =
      cardCoordinator.viewController;

  [self.childCoordinators addObject:cardCoordinator];
}

// Shows the address manual filling options.
- (void)showAddressManualFillingOptions {
  [self stopChildCoordinators];

  AddressCoordinator* addressCoordinator = [[AddressCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:self.browser
                injectionHandler:self.injectionHandler];
  addressCoordinator.delegate = self;

  self.expandedManualFillViewController.childViewController =
      addressCoordinator.viewController;

  [self.childCoordinators addObject:addressCoordinator];
}

@end
