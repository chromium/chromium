// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

#import <memory>
#import <vector>

#import "base/functional/bind.h"
#import "base/run_loop.h"
#import "base/test/bind.h"
#import "base/test/ios/wait_util.h"
#import "components/autofill/core/browser/payments/legal_message_line.h"
#import "components/autofill/core/browser/payments/payments_autofill_client.h"
#import "components/autofill/core/browser/payments/test_legal_message_line.h"
#import "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web_view/internal/autofill/cwv_credit_card_internal.h"
#import "ios/web_view/internal/autofill/cwv_vcn_enrollment_manager_internal.h"
#import "ios/web_view/test/test_with_locale_and_resources.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"

using base::test::ios::kWaitForActionTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

namespace ios_web_view {

class CWVVCNEnrollmentManagerTest : public TestWithLocaleAndResources {
 protected:
  void SetUp() override {
    TestWithLocaleAndResources::SetUp();
    accept_callback_called_ = false;
    decline_callback_called_ = false;
    credit_card_ = autofill::test::GetCreditCard();
    legal_message_lines = {autofill::TestLegalMessageLine(
        "Test line 1",
        {autofill::LegalMessageLine::Link(5, 9, "http://www.chromium.org/")})};

    // This manager instance is used by most tests.
    manager = [[CWVVCNEnrollmentManager alloc]
        initWithCreditCard:credit_card_
         legalMessageLines:legal_message_lines
            enrollCallback:base::BindLambdaForTesting(
                               [&]() { accept_callback_called_ = true; })
           declineCallback:base::BindLambdaForTesting(
                               [&]() { decline_callback_called_ = true; })];
  }

  void TearDown() override {
    manager = nil;
    TestWithLocaleAndResources::TearDown();
  }

  web::WebTaskEnvironment task_environment_;
  bool accept_callback_called_;
  bool decline_callback_called_;
  autofill::CreditCard credit_card_;
  autofill::LegalMessageLines legal_message_lines;
  CWVVCNEnrollmentManager* manager;
};

// Tests CWVCreditCardSaver properly initializes.
TEST_F(CWVVCNEnrollmentManagerTest, Initialization) {
  ASSERT_NE(nil, manager);
  EXPECT_NE(nil, manager.creditCard);
  EXPECT_EQ(legal_message_lines.size(), manager.legalMessages.count);
  EXPECT_FALSE(accept_callback_called_);
  EXPECT_FALSE(decline_callback_called_);
}

// Tests that calling `enrollWithCompletionHandler:` invokes the accept
// callback.
TEST_F(CWVVCNEnrollmentManagerTest, TestEnroll) {
  [manager enrollWithCompletionHandler:^(BOOL enrolled){
  }];

  EXPECT_TRUE(accept_callback_called_);
  EXPECT_FALSE(decline_callback_called_);
}

// Tests that calling `decline` invokes the decline callback.
TEST_F(CWVVCNEnrollmentManagerTest, TestDecline) {
  [manager decline];

  EXPECT_TRUE(decline_callback_called_);
  EXPECT_FALSE(accept_callback_called_);
}

// Tests that if no decision is made, `dealloc` invokes the decline callback.
TEST_F(CWVVCNEnrollmentManagerTest, TestDeallocDeclines) {
  // Ensure flags are in a known state before creating a local manager instance
  // to test its deallocation behavior.
  accept_callback_called_ = false;
  decline_callback_called_ = false;

  @autoreleasepool {
    CWVVCNEnrollmentManager* localManager = [[CWVVCNEnrollmentManager alloc]
        initWithCreditCard:credit_card_
         legalMessageLines:legal_message_lines
            enrollCallback:base::BindLambdaForTesting(
                               [&]() { accept_callback_called_ = true; })
           declineCallback:base::BindLambdaForTesting(
                               [&]() { decline_callback_called_ = true; })];
    ASSERT_NE(nil, localManager);
  }

  EXPECT_TRUE(decline_callback_called_);
  EXPECT_FALSE(accept_callback_called_);
}

// Tests that the enrollment completion handler is correctly invoked.
TEST_F(CWVVCNEnrollmentManagerTest, TestEnrollmentCompletionHandler) {
  __block BOOL enrollment_result = NO;
  __block int completion_handler_called_count = 0;

  [manager enrollWithCompletionHandler:^(BOOL enrolled) {
    enrollment_result = enrolled;
    completion_handler_called_count++;
  }];

  [manager handleCreditCardVCNEnrollmentCompleted:YES];

  EXPECT_TRUE(enrollment_result);
  EXPECT_EQ(1, completion_handler_called_count);

  [manager handleCreditCardVCNEnrollmentCompleted:NO];
  EXPECT_EQ(1, completion_handler_called_count);
}

// Tests that calling `decline` after `enroll...` triggers a DCHECK.
TEST_F(CWVVCNEnrollmentManagerTest, TestDeclineAfterEnroll) {
  [manager enrollWithCompletionHandler:^(BOOL enrolled){
  }];
  ASSERT_DEATH_IF_SUPPORTED([manager decline], "");
}

// Tests that calling `enroll...` after `decline` triggers a DCHECK.
TEST_F(CWVVCNEnrollmentManagerTest, TestEnrollAfterDecline) {
  [manager decline];
  ASSERT_DEATH_IF_SUPPORTED(
      [manager enrollWithCompletionHandler:^(BOOL enrolled){
      }],
      "");
}

// Tests that calling `enroll...` twice triggers a DCHECK.
TEST_F(CWVVCNEnrollmentManagerTest, TestEnrollTwice) {
  [manager enrollWithCompletionHandler:^(BOOL enrolled){
  }];
  ASSERT_DEATH_IF_SUPPORTED(
      [manager enrollWithCompletionHandler:^(BOOL enrolled){
      }],
      "");
}

// Tests that calling `decline` twice triggers a DCHECK.
TEST_F(CWVVCNEnrollmentManagerTest, TestDeclineTwice) {
  [manager decline];
  ASSERT_DEATH_IF_SUPPORTED([manager decline], "");
}

}  // namespace ios_web_view
