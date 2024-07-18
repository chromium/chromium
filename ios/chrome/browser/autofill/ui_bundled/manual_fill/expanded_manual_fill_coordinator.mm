// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/expanded_manual_fill_coordinator.h"

#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/address_coordinator.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/card_coordinator.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/expanded_manual_fill_view_controller.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_constants.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_password_coordinator.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"
#import "ios/web/public/web_state.h"
#import "url/gurl.h"

using manual_fill::ManualFillDataType;

@interface ExpandedManualFillCoordinator () <
    ExpandedManualFillViewControllerDelegate>

// Main view controller for this coordinator.
@property(nonatomic, strong)
    ExpandedManualFillViewController* expandedManualFillViewController;

@end

@implementation ExpandedManualFillCoordinator {
  // Focused field data type to present in the expanded manual fill view.
  ManualFillDataType _focusedFieldDataType;

  // Stores the selected segment's data type.
  ManualFillDataType _selectedSegmentDataType;

  // Reauthentication Module used for re-authentication.
  ReauthenticationModule* _reauthenticationModule;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                               forDataType:(ManualFillDataType)dataType
                    reauthenticationModule:
                        (ReauthenticationModule*)reauthenticationModule {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _focusedFieldDataType = dataType;
    _selectedSegmentDataType = dataType;
    _reauthenticationModule = reauthenticationModule;
  }
  return self;
}

- (void)start {
  self.expandedManualFillViewController =
      [[ExpandedManualFillViewController alloc]
          initWithDelegate:self
               forDataType:_focusedFieldDataType];

  [self showManualFillingOptionsForDataType:_focusedFieldDataType];
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
  _selectedSegmentDataType = dataType;
  [self showManualFillingOptionsForDataType:_selectedSegmentDataType];
}

#pragma mark - FallbackCoordinatorDelegate

- (void)fallbackCoordinatorDidDismissPopover:
    (FallbackCoordinator*)fallbackCoordinator {
  // No-op as the expanded manual fill view is never presented as a popover for
  // now.
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
              showAutofillFormButton:(_focusedFieldDataType ==
                                      ManualFillDataType::kPassword)];
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
                injectionHandler:self.injectionHandler
          reauthenticationModule:_reauthenticationModule
          showAutofillFormButton:(_focusedFieldDataType ==
                                  ManualFillDataType::kPaymentMethod)];
  cardCoordinator.delegate = self.delegate;

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
                injectionHandler:self.injectionHandler
          showAutofillFormButton:(_focusedFieldDataType ==
                                  ManualFillDataType::kAddress)];
  addressCoordinator.delegate = self.delegate;

  self.expandedManualFillViewController.childViewController =
      addressCoordinator.viewController;

  [self.childCoordinators addObject:addressCoordinator];
}

#pragma mark - FormInputInteractionDelegate

- (void)focusDidChangedWithFillingProduct:
    (autofill::FillingProduct)fillingProduct {
  ManualFillDataType previousFocusedFieldDataType = _focusedFieldDataType;
  _focusedFieldDataType =
      [ManualFillUtil manualFillDataTypeFromFillingProduct:fillingProduct];
  if (previousFocusedFieldDataType == _focusedFieldDataType) {
    return;
  }

  BOOL autofillFormButtonCurrentlyVisible =
      previousFocusedFieldDataType == _selectedSegmentDataType;
  BOOL shouldAutofillFormButtonBeVisible =
      _focusedFieldDataType == _selectedSegmentDataType;
  if (autofillFormButtonCurrentlyVisible != shouldAutofillFormButtonBeVisible) {
    [self showManualFillingOptionsForDataType:_selectedSegmentDataType];
  }
}

@end
