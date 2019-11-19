// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/payments/contact_info_edit_coordinator.h"

#include <memory>

#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/geo/test_region_data_loader.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "ios/chrome/browser/payments/payment_request_test_util.h"
#include "ios/chrome/browser/payments/payment_request_unittest_base.h"
#include "ios/chrome/browser/payments/test_payment_request.h"
#import "ios/chrome/browser/ui/payments/payment_request_edit_view_controller.h"
#import "ios/chrome/browser/ui/payments/payment_request_editor_field.h"
#import "ios/chrome/browser/ui/payments/payment_request_navigation_controller.h"
#import "ios/chrome/test/scoped_key_window.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

using ::testing::_;

class MockTestPaymentRequest : public payments::TestPaymentRequest {
 public:
  MockTestPaymentRequest(payments::WebPaymentRequest web_payment_request,
                         ios::ChromeBrowserState* browser_state,
                         web::WebState* web_state,
                         autofill::PersonalDataManager* personal_data_manager)
      : payments::TestPaymentRequest(web_payment_request,
                                     browser_state,
                                     web_state,
                                     personal_data_manager) {}
  MOCK_METHOD1(AddAutofillProfile,
               autofill::AutofillProfile*(const autofill::AutofillProfile&));
  MOCK_METHOD1(UpdateAutofillProfile, void(const autofill::AutofillProfile&));
};

MATCHER_P3(ProfileMatches, name, email, phone_number, "") {
  return arg.GetRawInfo(autofill::NAME_FULL) == base::ASCIIToUTF16(name) &&
         arg.GetRawInfo(autofill::EMAIL_ADDRESS) == base::ASCIIToUTF16(email) &&
         arg.GetRawInfo(autofill::PHONE_HOME_WHOLE_NUMBER) ==
             base::ASCIIToUTF16(phone_number);
}

NSArray<EditorField*>* GetEditorFields() {
  return @[
    [[EditorField alloc] initWithAutofillUIType:AutofillUITypeProfileFullName
                                      fieldType:EditorFieldTypeTextField
                                          label:@"Name"
                                          value:@"John Doe"
                                       required:YES],
    [[EditorField alloc]
        initWithAutofillUIType:AutofillUITypeProfileEmailAddress
                     fieldType:EditorFieldTypeTextField
                         label:@"Email"
                         value:@"john@doe.com"
                      required:YES],
    [[EditorField alloc]
        initWithAutofillUIType:AutofillUITypeProfileHomePhoneWholeNumber
                     fieldType:EditorFieldTypeTextField
                         label:@"Phone"
                         value:@"16502111111"
                      required:YES],
  ];
}
}  // namespace

class PaymentRequestContactInfoEditCoordinatorTest
    : public PaymentRequestUnitTestBase,
      public PlatformTest {
 protected:
  PaymentRequestContactInfoEditCoordinatorTest() {}

  // PlatformTest:
  void SetUp() override {
    PlatformTest::SetUp();
    DoSetUp();

    personal_data_manager_.SetPrefService(pref_service());

    payment_request_ = std::make_unique<MockTestPaymentRequest>(
        payment_request_test_util::CreateTestWebPaymentRequest(),
        browser_state(), web_state(), &personal_data_manager_);

    test_region_data_loader_.set_synchronous_callback(true);
    payment_request_->SetRegionDataLoader(&test_region_data_loader_);
  }

  // PlatformTest:
  void TearDown() override {
    personal_data_manager_.SetPrefService(nullptr);

    DoTearDown();
    PlatformTest::TearDown();
  }

  autofill::TestPersonalDataManager personal_data_manager_;
  autofill::TestRegionDataLoader test_region_data_loader_;
  std::unique_ptr<MockTestPaymentRequest> payment_request_;
};

// Tests that invoking start and stop on the coordinator presents and dismisses
// the contact info edit view controller, respectively.
TEST_F(PaymentRequestContactInfoEditCoordinatorTest, StartAndStop) {
  UIViewController* base_view_controller = [[UIViewController alloc] init];
  ScopedKeyWindow scoped_key_window_;
  [scoped_key_window_.Get() setRootViewController:base_view_controller];

  ContactInfoEditCoordinator* coordinator = [[ContactInfoEditCoordinator alloc]
      initWithBaseViewController:base_view_controller];
  [coordinator setPaymentRequest:payment_request_.get()];

  EXPECT_EQ(nil, base_view_controller.presentedViewController);

  [coordinator start];
  // Spin the run loop to trigger the animation.
  base::test::ios::SpinRunLoopWithMaxDelay(base::TimeDelta::FromSecondsD(1.0));
  EXPECT_TRUE([base_view_controller.presentedViewController
      isMemberOfClass:[PaymentRequestNavigationController class]]);
  PaymentRequestNavigationController* navigation_controller =
      base::mac::ObjCCastStrict<PaymentRequestNavigationController>(
          base_view_controller.presentedViewController);
  EXPECT_TRUE([navigation_controller.visibleViewController
      isMemberOfClass:[PaymentRequestEditViewController class]]);

  [coordinator stop];
  // Wait until the animation completes and the presented view controller is
  // dismissed.
  base::test::ios::WaitUntilCondition(^bool() {
    return !base_view_controller.presentedViewController;
  });
  EXPECT_EQ(nil, base_view_controller.presentedViewController);
}

// Tests that calling the view controller delegate method which signals that the
// user has finished creating a new profile, causes the profile to be added to
// the PaymentRequest instance and the corresponding coordinator delegate method
// to get called. The new profile should also get added to the
// PersonalDataManager.
TEST_F(PaymentRequestContactInfoEditCoordinatorTest, DidFinishCreating) {
  UIViewController* base_view_controller = [[UIViewController alloc] init];
  ScopedKeyWindow scoped_key_window_;
  [scoped_key_window_.Get() setRootViewController:base_view_controller];

  ContactInfoEditCoordinator* coordinator = [[ContactInfoEditCoordinator alloc]
      initWithBaseViewController:base_view_controller];
  [coordinator setPaymentRequest:payment_request_.get()];

  // Mock the coordinator delegate.
  id delegate = [OCMockObject
      mockForProtocol:@protocol(ContactInfoEditCoordinatorDelegate)];
  [[delegate expect]
      contactInfoEditCoordinator:coordinator
         didFinishEditingProfile:static_cast<autofill::AutofillProfile*>(
                                     [OCMArg anyPointer])];
  [coordinator setDelegate:delegate];

  EXPECT_EQ(nil, base_view_controller.presentedViewController);

  [coordinator start];
  // Spin the run loop to trigger the animation.
  base::test::ios::SpinRunLoopWithMaxDelay(base::TimeDelta::FromSecondsD(1.0));
  EXPECT_NE(nil, base_view_controller.presentedViewController);

  // Expect an autofill profile to be added to the PaymentRequest.
  EXPECT_CALL(*payment_request_,
              AddAutofillProfile(
                  ProfileMatches("John Doe", "john@doe.com", "1 650-211-1111")))
      .Times(1);
  // No autofill profile should get updated in the PaymentRequest.
  EXPECT_CALL(*payment_request_, UpdateAutofillProfile(_)).Times(0);

  // Call the controller delegate method.
  EXPECT_TRUE([base_view_controller.presentedViewController
      isMemberOfClass:[PaymentRequestNavigationController class]]);
  PaymentRequestNavigationController* navigation_controller =
      base::mac::ObjCCastStrict<PaymentRequestNavigationController>(
          base_view_controller.presentedViewController);
  PaymentRequestEditViewController* view_controller =
      base::mac::ObjCCastStrict<PaymentRequestEditViewController>(
          navigation_controller.visibleViewController);
  [coordinator paymentRequestEditViewController:view_controller
                         didFinishEditingFields:GetEditorFields()];

  EXPECT_OCMOCK_VERIFY(delegate);
}

// Tests that calling the view controller delegate method which signals that the
// user has finished editing a profile, causes the corresponding coordinator
// delegate method to get called. The profile should not get re-added to the
// PaymentRequest instance or the PersonalDataManager. However, it is expected
// to get updated in the PersonalDataManager.
TEST_F(PaymentRequestContactInfoEditCoordinatorTest, DidFinishEditing) {
  UIViewController* base_view_controller = [[UIViewController alloc] init];
  ScopedKeyWindow scoped_key_window_;
  [scoped_key_window_.Get() setRootViewController:base_view_controller];

  ContactInfoEditCoordinator* coordinator = [[ContactInfoEditCoordinator alloc]
      initWithBaseViewController:base_view_controller];
  [coordinator setPaymentRequest:payment_request_.get()];

  // Set the profile to be edited.
  autofill::AutofillProfile profile;
  [coordinator setProfile:&profile];

  // Mock the coordinator delegate.
  id delegate = [OCMockObject
      mockForProtocol:@protocol(ContactInfoEditCoordinatorDelegate)];
  [[delegate expect]
      contactInfoEditCoordinator:coordinator
         didFinishEditingProfile:static_cast<autofill::AutofillProfile*>(
                                     [OCMArg anyPointer])];
  [coordinator setDelegate:delegate];

  EXPECT_EQ(nil, base_view_controller.presentedViewController);

  [coordinator start];
  // Spin the run loop to trigger the animation.
  base::test::ios::SpinRunLoopWithMaxDelay(base::TimeDelta::FromSecondsD(1.0));
  EXPECT_NE(nil, base_view_controller.presentedViewController);

  // No autofill profile should get added to the PaymentRequest.
  EXPECT_CALL(*payment_request_, AddAutofillProfile(_)).Times(0);
  // Expect an autofill profile to be updated in the PaymentRequest.
  EXPECT_CALL(*payment_request_,
              UpdateAutofillProfile(
                  ProfileMatches("John Doe", "john@doe.com", "1 650-211-1111")))
      .Times(1);

  // Call the controller delegate method.
  EXPECT_TRUE([base_view_controller.presentedViewController
      isMemberOfClass:[PaymentRequestNavigationController class]]);
  PaymentRequestNavigationController* navigation_controller =
      base::mac::ObjCCastStrict<PaymentRequestNavigationController>(
          base_view_controller.presentedViewController);
  PaymentRequestEditViewController* view_controller =
      base::mac::ObjCCastStrict<PaymentRequestEditViewController>(
          navigation_controller.visibleViewController);
  [coordinator paymentRequestEditViewController:view_controller
                         didFinishEditingFields:GetEditorFields()];

  EXPECT_OCMOCK_VERIFY(delegate);
}

// Tests that calling the view controller delegate method which signals that the
// user has chosen to cancel creating/editing a profile, causes the
// corresponding coordinator delegate method to get called.
TEST_F(PaymentRequestContactInfoEditCoordinatorTest, DidCancel) {
  UIViewController* base_view_controller = [[UIViewController alloc] init];
  ScopedKeyWindow scoped_key_window_;
  [scoped_key_window_.Get() setRootViewController:base_view_controller];

  ContactInfoEditCoordinator* coordinator = [[ContactInfoEditCoordinator alloc]
      initWithBaseViewController:base_view_controller];
  [coordinator setPaymentRequest:payment_request_.get()];

  // Mock the coordinator delegate.
  id delegate = [OCMockObject
      mockForProtocol:@protocol(ContactInfoEditCoordinatorDelegate)];
  [[delegate expect] contactInfoEditCoordinatorDidCancel:coordinator];
  [coordinator setDelegate:delegate];

  EXPECT_EQ(nil, base_view_controller.presentedViewController);

  [coordinator start];
  // Spin the run loop to trigger the animation.
  base::test::ios::SpinRunLoopWithMaxDelay(base::TimeDelta::FromSecondsD(1.0));
  EXPECT_NE(nil, base_view_controller.presentedViewController);

  // Call the controller delegate method.
  EXPECT_TRUE([base_view_controller.presentedViewController
      isMemberOfClass:[PaymentRequestNavigationController class]]);
  PaymentRequestNavigationController* navigation_controller =
      base::mac::ObjCCastStrict<PaymentRequestNavigationController>(
          base_view_controller.presentedViewController);
  PaymentRequestEditViewController* view_controller =
      base::mac::ObjCCastStrict<PaymentRequestEditViewController>(
          navigation_controller.visibleViewController);
  [coordinator paymentRequestEditViewControllerDidCancel:view_controller];

  EXPECT_OCMOCK_VERIFY(delegate);
}
