// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PAYMENTS_CREDIT_CARD_EDIT_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_PAYMENTS_CREDIT_CARD_EDIT_COORDINATOR_H_

#import "ios/chrome/browser/ui/coordinators/chrome_coordinator.h"
#import "ios/chrome/browser/ui/payments/address_edit_coordinator.h"
#import "ios/chrome/browser/ui/payments/billing_address_selection_coordinator.h"
#import "ios/chrome/browser/ui/payments/payment_request_edit_view_controller.h"

namespace payments {
class AutofillPaymentApp;
}

namespace payments {
class PaymentRequest;
}  // namespace payments

@class CreditCardEditCoordinator;

// Delegate protocol for CreditCardEditCoordinator.
@protocol CreditCardEditCoordinatorDelegate<NSObject>

// Notifies the delegate that the user has finished editing or creating
// |paymentMethod|. |paymentMethod| will be a new payment method owned by the
// PaymentRequest object if no payment method instance was provided to the
// coordinator. Otherwise, it will be the same edited instance.
- (void)creditCardEditCoordinator:(CreditCardEditCoordinator*)coordinator
    didFinishEditingPaymentMethod:(payments::AutofillPaymentApp*)paymentMethod;

// Notifies the delegate that the user has chosen to cancel editing or creating
// a credit card and return to the previous screen.
- (void)creditCardEditCoordinatorDidCancel:
    (CreditCardEditCoordinator*)coordinator;

@end

// Coordinator responsible for creating and presenting a credit card editor view
// controller. This view controller will be presented by the view controller
// provided in the initializer.
@interface CreditCardEditCoordinator
    : ChromeCoordinator<AddressEditCoordinatorDelegate,
                        BillingAddressSelectionCoordinatorDelegate,
                        PaymentRequestEditViewControllerDelegate>

// The payment method to be edited, if any. This pointer is not owned by this
// class and should outlive it.
@property(nonatomic, assign) payments::AutofillPaymentApp* paymentMethod;

// The PaymentRequest object owning an instance of payments::WebPaymentRequest
// as provided by the page invoking the Payment Request API. This pointer is not
// owned by this class and should outlive it.
@property(nonatomic, assign) payments::PaymentRequest* paymentRequest;

// The delegate to be notified when the user returns or finishes creating or
// editing a credit card.
@property(nonatomic, weak) id<CreditCardEditCoordinatorDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_PAYMENTS_CREDIT_CARD_EDIT_COORDINATOR_H_
