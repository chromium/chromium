// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/payments/billing_address_selection_coordinator.h"

#include "base/mac/foundation_util.h"
#import "base/test/ios/wait_util.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/geo/test_region_data_loader.h"
#include "ios/chrome/browser/payments/payment_request_test_util.h"
#import "ios/chrome/browser/payments/payment_request_unittest_base.h"
#import "ios/chrome/browser/ui/payments/payment_request_selector_view_controller.h"
#include "testing/platform_test.h"
#include "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

class PaymentRequestBillingAddressSelectionCoordinatorTest
    : public PaymentRequestUnitTestBase,
      public PlatformTest {
 protected:
  // PlatformTest:
  void SetUp() override {
    PlatformTest::SetUp();
    DoSetUp();

    // Add testing profiles to the database. Make the less frequently used one
    // incomplete.
    autofill::AutofillProfile profile = autofill::test::GetFullProfile();
    profile.set_use_count(10U);
    AddAutofillProfile(std::move(profile));

    autofill::AutofillProfile profile2 = autofill::test::GetFullProfile2();
    profile2.set_use_count(5U);
    profile2.SetInfo(autofill::AutofillType(autofill::PHONE_HOME_WHOLE_NUMBER),
                     base::string16(), "en-US");
    AddAutofillProfile(std::move(profile2));

    CreateTestPaymentRequest();

    test_region_data_loader_.set_synchronous_callback(true);
    payment_request()->SetRegionDataLoader(&test_region_data_loader_);

    // Create the controller and coordinator under test.
    UIViewController* base_view_controller = [[UIViewController alloc] init];
    navigation_controller_ = [[UINavigationController alloc]
        initWithRootViewController:base_view_controller];

    coordinator_ = [[BillingAddressSelectionCoordinator alloc]
        initWithBaseViewController:base_view_controller];
    [coordinator_ setPaymentRequest:payment_request()];
  }

  // PlatformTest:
  void TearDown() override {
    DoTearDown();
    PlatformTest::TearDown();
  }

  UINavigationController* GetNavigationController() {
    return navigation_controller_;
  }

  BillingAddressSelectionCoordinator* GetCoordinator() { return coordinator_; }

 private:
  autofill::TestRegionDataLoader test_region_data_loader_;
  UINavigationController* navigation_controller_;
  BillingAddressSelectionCoordinator* coordinator_;
};

// Tests that invoking start and stop on the coordinator presents and dismisses
// the PaymentRequestSelectorViewController, respectively.
TEST_F(PaymentRequestBillingAddressSelectionCoordinatorTest, StartAndStop) {
  EXPECT_EQ(1u, GetNavigationController().viewControllers.count);

  [GetCoordinator() start];
  // Spin the run loop to trigger the animation.
  base::test::ios::SpinRunLoopWithMaxDelay(base::TimeDelta::FromSecondsD(1.0));
  EXPECT_EQ(2u, GetNavigationController().viewControllers.count);

  UIViewController* view_controller =
      GetNavigationController().visibleViewController;
  EXPECT_TRUE([view_controller
      isMemberOfClass:[PaymentRequestSelectorViewController class]]);

  [GetCoordinator() stop];
  // Spin the run loop to trigger the animation.
  base::test::ios::SpinRunLoopWithMaxDelay(base::TimeDelta::FromSecondsD(1.0));
  EXPECT_EQ(1u, GetNavigationController().viewControllers.count);
}

// Tests that calling the view controller delegate method which notifies the
// delegate about selection of a billing address invokes the corresponding
// coordinator delegate method, only if the payment method is complete.
TEST_F(PaymentRequestBillingAddressSelectionCoordinatorTest,
       SelectedBillingAddress) {
  // Mock the coordinator delegate.
  id delegate = [OCMockObject
      mockForProtocol:@protocol(BillingAddressSelectionCoordinatorDelegate)];
  [[delegate expect]
      billingAddressSelectionCoordinator:GetCoordinator()
                 didSelectBillingAddress:payment_request()
                                             ->billing_profiles()[0]];
  [[delegate reject]
      billingAddressSelectionCoordinator:GetCoordinator()
                 didSelectBillingAddress:payment_request()
                                             ->billing_profiles()[1]];
  [GetCoordinator() setDelegate:delegate];

  EXPECT_EQ(1u, GetNavigationController().viewControllers.count);

  [GetCoordinator() start];
  // Spin the run loop to trigger the animation.
  base::test::ios::SpinRunLoopWithMaxDelay(base::TimeDelta::FromSecondsD(1.0));
  EXPECT_EQ(2u, GetNavigationController().viewControllers.count);

  // Call the controller delegate method for both selectable items.
  PaymentRequestSelectorViewController* view_controller =
      base::mac::ObjCCastStrict<PaymentRequestSelectorViewController>(
          GetNavigationController().visibleViewController);
  EXPECT_TRUE([GetCoordinator()
      paymentRequestSelectorViewController:view_controller
                      didSelectItemAtIndex:0]);
  // Wait for the coordinator delegate to be notified.
  base::test::ios::SpinRunLoopWithMinDelay(base::TimeDelta::FromSecondsD(0.5));
  EXPECT_FALSE([GetCoordinator()
      paymentRequestSelectorViewController:view_controller
                      didSelectItemAtIndex:1]);

  EXPECT_OCMOCK_VERIFY(delegate);
}

// Tests that calling the view controller delegate method which notifies the
// delegate that the user has chosen to return without making a selection
// invokes the corresponding coordinator delegate method.
TEST_F(PaymentRequestBillingAddressSelectionCoordinatorTest, DidReturn) {
  // Mock the coordinator delegate.
  id delegate = [OCMockObject
      mockForProtocol:@protocol(BillingAddressSelectionCoordinatorDelegate)];
  [[delegate expect]
      billingAddressSelectionCoordinatorDidReturn:GetCoordinator()];
  [GetCoordinator() setDelegate:delegate];

  EXPECT_EQ(1u, GetNavigationController().viewControllers.count);

  [GetCoordinator() start];
  // Spin the run loop to trigger the animation.
  base::test::ios::SpinRunLoopWithMaxDelay(base::TimeDelta::FromSecondsD(1.0));
  EXPECT_EQ(2u, GetNavigationController().viewControllers.count);

  // Call the controller delegate method.
  PaymentRequestSelectorViewController* view_controller =
      base::mac::ObjCCastStrict<PaymentRequestSelectorViewController>(
          GetNavigationController().visibleViewController);
  [GetCoordinator()
      paymentRequestSelectorViewControllerDidFinish:view_controller];

  EXPECT_OCMOCK_VERIFY(delegate);
}
