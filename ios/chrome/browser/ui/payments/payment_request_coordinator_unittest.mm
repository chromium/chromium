// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/payments/payment_request_coordinator.h"

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/mac/foundation_util.h"
#include "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/payments/core/autofill_payment_app.h"
#include "components/payments/core/payment_address.h"
#include "components/payments/core/payment_app.h"
#include "components/payments/core/payment_request_data_util.h"
#include "components/payments/core/payment_shipping_option.h"
#include "components/payments/core/payments_test_util.h"
#include "ios/chrome/browser/payments/payment_request_test_util.h"
#import "ios/chrome/browser/payments/payment_request_unittest_base.h"
#import "ios/chrome/browser/ui/payments/payment_request_navigation_controller.h"
#import "ios/chrome/browser/ui/payments/payment_request_view_controller.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/testing/ocmock_complex_type_helper.h"
#include "testing/platform_test.h"
#include "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PaymentRequestCoordinatorDelegateMock<
    PaymentRequestCoordinatorDelegate>:OCMockComplexTypeHelper
@end

@implementation PaymentRequestCoordinatorDelegateMock

typedef void (^mock_coordinator_confirm)(PaymentRequestCoordinator*);
typedef void (^mock_coordinator_cancel)(PaymentRequestCoordinator*);
typedef void (^mock_coordinator_select_shipping_address)(
    PaymentRequestCoordinator*,
    const autofill::AutofillProfile&);
typedef void (^mock_coordinator_select_shipping_option)(
    PaymentRequestCoordinator*,
    const payments::PaymentShippingOption&);

- (void)paymentRequestCoordinatorDidConfirm:
    (PaymentRequestCoordinator*)coordinator {
  return static_cast<mock_coordinator_confirm>([self blockForSelector:_cmd])(
      coordinator);
}

- (void)paymentRequestCoordinatorDidCancel:
    (PaymentRequestCoordinator*)coordinator {
  return static_cast<mock_coordinator_cancel>([self blockForSelector:_cmd])(
      coordinator);
}

- (void)paymentRequestCoordinator:(PaymentRequestCoordinator*)coordinator
         didSelectShippingAddress:
             (const autofill::AutofillProfile&)shippingAddress {
  return static_cast<mock_coordinator_select_shipping_address>(
      [self blockForSelector:_cmd])(coordinator, shippingAddress);
}

- (void)paymentRequestCoordinator:(PaymentRequestCoordinator*)coordinator
          didSelectShippingOption:
              (const payments::PaymentShippingOption&)shippingOption {
  return static_cast<mock_coordinator_select_shipping_option>(
      [self blockForSelector:_cmd])(coordinator, shippingOption);
}

@end

class PaymentRequestCoordinatorTest : public PaymentRequestUnitTestBase,
                                      public PlatformTest {
 protected:
  // PlatformTest:
  void SetUp() override {
    PlatformTest::SetUp();
    DoSetUp();

    autofill::AutofillProfile profile = autofill::test::GetFullProfile();
    autofill::CreditCard card = autofill::test::GetCreditCard();  // Visa.
    card.set_billing_address_id(profile.guid());
    AddAutofillProfile(std::move(profile));
    AddCreditCard(std::move(card));

    CreateTestPaymentRequest();
  }

  // PlatformTest:
  void TearDown() override {
    DoTearDown();
    PlatformTest::TearDown();
  }
};

// Tests that invoking start and stop on the coordinator presents and
// dismisses
// the PaymentRequestViewController, respectively.
TEST_F(PaymentRequestCoordinatorTest, StartAndStop) {
  UIViewController* base_view_controller = [[UIViewController alloc] init];
  ScopedKeyWindow scoped_key_window_;
  [scoped_key_window_.Get() setRootViewController:base_view_controller];

  PaymentRequestCoordinator* coordinator = [[PaymentRequestCoordinator alloc]
      initWithBaseViewController:base_view_controller];
  [coordinator setPaymentRequest:payment_request()];
  [coordinator setBrowserState:browser_state()];

  [coordinator start];
  // Spin the run loop to trigger the animation.
  base::test::ios::SpinRunLoopWithMaxDelay(base::TimeDelta::FromSecondsD(1));
  EXPECT_TRUE([base_view_controller.presentedViewController
      isMemberOfClass:[PaymentRequestNavigationController class]]);
  PaymentRequestNavigationController* navigation_controller =
      base::mac::ObjCCastStrict<PaymentRequestNavigationController>(
          base_view_controller.presentedViewController);
  EXPECT_EQ(1u, navigation_controller.viewControllers.count);
  EXPECT_TRUE([navigation_controller.visibleViewController
      isMemberOfClass:[PaymentRequestViewController class]]);

  [coordinator stop];
  // Wait until the animation completes and the presented view controller is
  // dismissed.
  base::test::ios::WaitUntilCondition(^bool() {
    return !base_view_controller.presentedViewController;
  });
  EXPECT_EQ(nil, base_view_controller.presentedViewController);
}

// Tests that calling the ShippingAddressSelectionCoordinator delegate method
// which notifies the coordinator about selection of a shipping address invokes
// the corresponding coordinator delegate method with the expected information.
TEST_F(PaymentRequestCoordinatorTest, DidSelectShippingAddress) {
  UIViewController* base_view_controller = [[UIViewController alloc] init];
  ScopedKeyWindow scoped_key_window_;
  [scoped_key_window_.Get() setRootViewController:base_view_controller];

  PaymentRequestCoordinator* coordinator = [[PaymentRequestCoordinator alloc]
      initWithBaseViewController:base_view_controller];
  [coordinator setPaymentRequest:payment_request()];

  // Mock the coordinator delegate.
  id delegate = [OCMockObject
      mockForProtocol:@protocol(PaymentMethodSelectionCoordinatorDelegate)];
  id delegate_mock([[PaymentRequestCoordinatorDelegateMock alloc]
      initWithRepresentedObject:delegate]);
  SEL selector = @selector(paymentRequestCoordinator:didSelectShippingAddress:);
  [delegate_mock
                onSelector:selector
      callBlockExpectation:^(PaymentRequestCoordinator* callerCoordinator,
                             const autofill::AutofillProfile& shippingAddress) {
        EXPECT_EQ(*profiles().back(), shippingAddress);
        EXPECT_EQ(coordinator, callerCoordinator);
      }];
  [coordinator setDelegate:delegate_mock];

  // Call the ShippingAddressSelectionCoordinator delegate method.
  [coordinator shippingAddressSelectionCoordinator:nil
                          didSelectShippingAddress:profiles().back()];
}

// Tests that calling the ShippingOptionSelectionCoordinator delegate method
// which notifies the coordinator about selection of a shipping option invokes
// the corresponding coordinator delegate method with the expected information.
TEST_F(PaymentRequestCoordinatorTest, DidSelectShippingOption) {
  UIViewController* base_view_controller = [[UIViewController alloc] init];
  ScopedKeyWindow scoped_key_window_;
  [scoped_key_window_.Get() setRootViewController:base_view_controller];

  PaymentRequestCoordinator* coordinator = [[PaymentRequestCoordinator alloc]
      initWithBaseViewController:base_view_controller];
  [coordinator setPaymentRequest:payment_request()];

  payments::PaymentShippingOption shipping_option;
  shipping_option.id = "123456";
  shipping_option.label = "1-Day";
  shipping_option.amount->value = "0.99";
  shipping_option.amount->currency = "USD";

  // Mock the coordinator delegate.
  id delegate = [OCMockObject
      mockForProtocol:@protocol(PaymentMethodSelectionCoordinatorDelegate)];
  id delegate_mock([[PaymentRequestCoordinatorDelegateMock alloc]
      initWithRepresentedObject:delegate]);
  SEL selector = @selector(paymentRequestCoordinator:didSelectShippingOption:);
  [delegate_mock onSelector:selector
       callBlockExpectation:^(
           PaymentRequestCoordinator* callerCoordinator,
           const payments::PaymentShippingOption& shippingOption) {
         EXPECT_EQ(shipping_option, shippingOption);
         EXPECT_EQ(coordinator, callerCoordinator);
       }];
  [coordinator setDelegate:delegate_mock];

  // Call the ShippingOptionSelectionCoordinator delegate method.
  [coordinator shippingOptionSelectionCoordinator:nil
                          didSelectShippingOption:&shipping_option];
}

// Tests that calling the view controller delegate method which notifies the
// coordinator about cancellation of the PaymentRequest invokes the
// corresponding coordinator delegate method.
TEST_F(PaymentRequestCoordinatorTest, DidCancel) {
  UIViewController* base_view_controller = [[UIViewController alloc] init];
  ScopedKeyWindow scoped_key_window_;
  [scoped_key_window_.Get() setRootViewController:base_view_controller];

  PaymentRequestCoordinator* coordinator = [[PaymentRequestCoordinator alloc]
      initWithBaseViewController:base_view_controller];
  [coordinator setPaymentRequest:payment_request()];

  // Mock the coordinator delegate.
  id delegate = [OCMockObject
      mockForProtocol:@protocol(PaymentMethodSelectionCoordinatorDelegate)];
  id delegate_mock([[PaymentRequestCoordinatorDelegateMock alloc]
      initWithRepresentedObject:delegate]);
  SEL selector = @selector(paymentRequestCoordinatorDidCancel:);
  [delegate_mock onSelector:selector
       callBlockExpectation:^(PaymentRequestCoordinator* callerCoordinator) {
         EXPECT_EQ(coordinator, callerCoordinator);
       }];
  [coordinator setDelegate:delegate_mock];
  [coordinator setBrowserState:browser_state()];

  [coordinator start];
  // Spin the run loop to trigger the animation.
  base::test::ios::SpinRunLoopWithMaxDelay(base::TimeDelta::FromSecondsD(1));

  // Call the controller delegate method.
  [coordinator paymentRequestViewControllerDidCancel:nil];
}

// Tests that calling the view controller delegate method which notifies the
// coordinator about confirmation of the PaymentRequest invokes the
// corresponding coordinator delegate method.
TEST_F(PaymentRequestCoordinatorTest, DidConfirm) {
  UIViewController* base_view_controller = [[UIViewController alloc] init];
  ScopedKeyWindow scoped_key_window_;
  [scoped_key_window_.Get() setRootViewController:base_view_controller];

  PaymentRequestCoordinator* coordinator = [[PaymentRequestCoordinator alloc]
      initWithBaseViewController:base_view_controller];
  [coordinator setPaymentRequest:payment_request()];

  // Mock the coordinator delegate.
  id delegate = [OCMockObject
      mockForProtocol:@protocol(PaymentMethodSelectionCoordinatorDelegate)];
  id delegate_mock([[PaymentRequestCoordinatorDelegateMock alloc]
      initWithRepresentedObject:delegate]);
  SEL selector = @selector(paymentRequestCoordinatorDidConfirm:);
  [delegate_mock onSelector:selector
       callBlockExpectation:^(PaymentRequestCoordinator* callerCoordinator) {
         EXPECT_EQ(coordinator, callerCoordinator);
       }];
  [coordinator setDelegate:delegate_mock];
  [coordinator setBrowserState:browser_state()];

  [coordinator start];
  // Spin the run loop to trigger the animation.
  base::test::ios::SpinRunLoopWithMaxDelay(base::TimeDelta::FromSecondsD(1));

  // Call the controller delegate method.
  [coordinator paymentRequestViewControllerDidConfirm:nil];
}

// Tests that calling the PaymentItemsDisplayCoordinatorDelegate delegate method
// which notifies the coordinator about confirmation of the PaymentRequest
// invokes the corresponding coordinator delegate method..
TEST_F(PaymentRequestCoordinatorTest,
       PaymentItemsDisplayCoordinatorDidConfirm) {
  UIViewController* base_view_controller = [[UIViewController alloc] init];
  ScopedKeyWindow scoped_key_window_;
  [scoped_key_window_.Get() setRootViewController:base_view_controller];

  PaymentRequestCoordinator* coordinator = [[PaymentRequestCoordinator alloc]
      initWithBaseViewController:base_view_controller];
  [coordinator setPaymentRequest:payment_request()];

  // Mock the coordinator delegate.
  id delegate = [OCMockObject
      mockForProtocol:@protocol(PaymentMethodSelectionCoordinatorDelegate)];
  id delegate_mock([[PaymentRequestCoordinatorDelegateMock alloc]
      initWithRepresentedObject:delegate]);
  SEL selector = @selector(paymentRequestCoordinatorDidConfirm:);
  [delegate_mock onSelector:selector
       callBlockExpectation:^(PaymentRequestCoordinator* callerCoordinator) {
         EXPECT_EQ(coordinator, callerCoordinator);
       }];
  [coordinator setDelegate:delegate_mock];

  // Call the PaymentItemsDisplayCoordinatorDelegate delegate method.
  [coordinator paymentItemsDisplayCoordinatorDidConfirm:nil];
}
