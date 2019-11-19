// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/payments/credit_card_edit_coordinator.h"

#include "base/guid.h"
#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/common/autofill_constants.h"
#import "components/autofill/ios/browser/credit_card_util.h"
#include "components/payments/core/autofill_payment_app.h"
#include "components/payments/core/payment_app.h"
#include "ios/chrome/browser/payments/payment_request.h"
#import "ios/chrome/browser/ui/autofill/autofill_ui_type_util.h"
#import "ios/chrome/browser/ui/payments/credit_card_edit_mediator.h"
#import "ios/chrome/browser/ui/payments/payment_request_editor_field.h"
#import "ios/chrome/browser/ui/payments/payment_request_navigation_controller.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface CreditCardEditCoordinator ()

@property(nonatomic, assign) autofill::CreditCard* creditCard;

@property(nonatomic, strong)
    BillingAddressSelectionCoordinator* billingAddressSelectionCoordinator;

@property(nonatomic, strong) AddressEditCoordinator* addressEditCoordinator;

@property(nonatomic, strong) PaymentRequestNavigationController* viewController;

@property(nonatomic, strong)
    PaymentRequestEditViewController* editViewController;

@property(nonatomic, strong) CreditCardEditMediator* mediator;

@end

@implementation CreditCardEditCoordinator

@synthesize paymentMethod = _paymentMethod;
@synthesize paymentRequest = _paymentRequest;
@synthesize delegate = _delegate;
@synthesize creditCard = _creditCard;
@synthesize billingAddressSelectionCoordinator =
    _billingAddressSelectionCoordinator;
@synthesize addressEditCoordinator = _addressEditCoordinator;
@synthesize viewController = _viewController;
@synthesize editViewController = _editViewController;
@synthesize mediator = _mediator;

- (void)start {
  _creditCard = _paymentMethod ? _paymentMethod->credit_card() : nil;

  _editViewController = [[PaymentRequestEditViewController alloc] init];
  [_editViewController setDelegate:self];
  _mediator =
      [[CreditCardEditMediator alloc] initWithPaymentRequest:_paymentRequest
                                                  creditCard:_creditCard];
  [_mediator setConsumer:_editViewController];
  [_editViewController setDataSource:_mediator];
  [_editViewController setValidatorDelegate:_mediator];
  [_editViewController loadModel];

  self.viewController = [[PaymentRequestNavigationController alloc]
      initWithRootViewController:self.editViewController];
  self.viewController.modalPresentationStyle = UIModalPresentationFormSheet;
  self.viewController.modalTransitionStyle =
      UIModalTransitionStyleCoverVertical;
  self.viewController.navigationBarHidden = YES;

  [[self baseViewController] presentViewController:self.viewController
                                          animated:YES
                                        completion:nil];
}

- (void)stop {
  [[self.viewController presentingViewController]
      dismissViewControllerAnimated:YES
                         completion:nil];
  [self.addressEditCoordinator stop];
  self.addressEditCoordinator = nil;
  [self.billingAddressSelectionCoordinator stop];
  self.billingAddressSelectionCoordinator = nil;
  self.editViewController = nil;
  self.viewController = nil;
}

#pragma mark - PaymentRequestEditViewControllerDelegate

- (void)paymentRequestEditViewController:
            (PaymentRequestEditViewController*)controller
                          didSelectField:(EditorField*)field {
  if (field.autofillUIType == AutofillUITypeCreditCardBillingAddress) {
    if (_paymentRequest->billing_profiles().empty()) {
      self.addressEditCoordinator = [[AddressEditCoordinator alloc]
          initWithBaseViewController:_viewController];
      [self.addressEditCoordinator setPaymentRequest:_paymentRequest];
      [self.addressEditCoordinator setDelegate:self];
      [self.addressEditCoordinator start];
      return;
    } else {
      self.billingAddressSelectionCoordinator =
          [[BillingAddressSelectionCoordinator alloc]
              initWithBaseViewController:self.editViewController];
      [self.billingAddressSelectionCoordinator
          setPaymentRequest:self.paymentRequest];
      [self.billingAddressSelectionCoordinator
          setSelectedBillingProfile:self.mediator.billingProfile];
      [self.billingAddressSelectionCoordinator setDelegate:self];
      [self.billingAddressSelectionCoordinator start];
    }
  }
}

- (void)paymentRequestEditViewController:
            (PaymentRequestEditViewController*)controller
                  didFinishEditingFields:(NSArray<EditorField*>*)fields {
  BOOL saveCreditCard = NO;
  // Create an empty credit card. If a credit card is being edited, copy over
  // the information.
  autofill::CreditCard creditCard =
      _creditCard ? *_creditCard : autofill::CreditCard();

  // Set the origin, or override it if the card is being edited.
  creditCard.set_origin(autofill::kSettingsOrigin);

  for (EditorField* field in fields) {
    if (field.autofillUIType == AutofillUITypeCreditCardExpDate) {
      NSArray<NSString*>* fieldComponents =
          [field.value componentsSeparatedByString:@" / "];
      NSString* expMonth = fieldComponents[0];
      creditCard.SetInfo(
          autofill::AutofillType(autofill::CREDIT_CARD_EXP_MONTH),
          base::SysNSStringToUTF16(expMonth),
          _paymentRequest->GetApplicationLocale());
      NSString* expYear = fieldComponents[1];
      creditCard.SetInfo(
          autofill::AutofillType(autofill::CREDIT_CARD_EXP_4_DIGIT_YEAR),
          base::SysNSStringToUTF16(expYear),
          _paymentRequest->GetApplicationLocale());
    } else if (field.autofillUIType == AutofillUITypeCreditCardSaveToChrome) {
      saveCreditCard = [field.value boolValue];
    } else if (field.autofillUIType == AutofillUITypeCreditCardBillingAddress) {
      creditCard.set_billing_address_id(base::SysNSStringToUTF8(field.value));
    } else {
      creditCard.SetInfo(autofill::AutofillType(AutofillTypeFromAutofillUIType(
                             field.autofillUIType)),
                         base::SysNSStringToUTF16(field.value),
                         _paymentRequest->GetApplicationLocale());
    }
  }

  if (!_creditCard) {
    // Add the credit card to the list of payment methods in |_paymentRequest|.
    if (saveCreditCard) {
      _paymentMethod =
          _paymentRequest->CreateAndAddAutofillPaymentInstrument(creditCard);
    }
  } else {
    // Update the original credit card instance that is being edited.
    *_creditCard = creditCard;
    _paymentRequest->UpdateAutofillPaymentInstrument(creditCard);
  }

  [_delegate creditCardEditCoordinator:self
         didFinishEditingPaymentMethod:_paymentMethod];
}

- (void)paymentRequestEditViewControllerDidCancel:
    (PaymentRequestEditViewController*)controller {
  [_delegate creditCardEditCoordinatorDidCancel:self];
}

#pragma mark - BillingAddressSelectionCoordinatorDelegate

- (void)billingAddressSelectionCoordinator:
            (BillingAddressSelectionCoordinator*)coordinator
                   didSelectBillingAddress:
                       (autofill::AutofillProfile*)billingAddress {
  // Update view controller's data source with the selection and reload the view
  // controller.
  DCHECK(billingAddress);
  [self.mediator setBillingProfile:billingAddress];
  [self.editViewController loadModel];
  [self.editViewController.collectionView reloadData];

  [self.billingAddressSelectionCoordinator stop];
  self.billingAddressSelectionCoordinator = nil;
}

- (void)billingAddressSelectionCoordinatorDidReturn:
    (BillingAddressSelectionCoordinator*)coordinator {
  [self.billingAddressSelectionCoordinator stop];
  self.billingAddressSelectionCoordinator = nil;
}

#pragma mark - AddressEditCoordinatorDelegate

- (void)addressEditCoordinator:(AddressEditCoordinator*)coordinator
       didFinishEditingAddress:(autofill::AutofillProfile*)address {
  // Update view controller's data source with the selection and reload the view
  // controller.
  DCHECK(address);
  [self.mediator setBillingProfile:address];
  [self.editViewController loadModel];
  [self.editViewController.collectionView reloadData];

  [self.addressEditCoordinator stop];
  self.addressEditCoordinator = nil;
}

- (void)addressEditCoordinatorDidCancel:(AddressEditCoordinator*)coordinator {
  [self.addressEditCoordinator stop];
  self.addressEditCoordinator = nil;
}

@end
