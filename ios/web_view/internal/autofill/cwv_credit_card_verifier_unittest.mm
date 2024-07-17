// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

#include <string>

#include "base/base_paths.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/sys_string_conversions.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/payments/card_unmask_delegate.h"
#import "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "ios/web/public/test/web_task_environment.h"
#include "ios/web/public/thread/web_task_traits.h"
#include "ios/web/public/thread/web_thread.h"
#import "ios/web_view/internal/autofill/cwv_credit_card_internal.h"
#import "ios/web_view/internal/autofill/cwv_credit_card_verifier_internal.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/base/resource/resource_bundle.h"
#import "ui/base/resource/resource_scale_factor.h"

namespace ios_web_view {

// Fake unmask delegate used to handle unmask responses.
class FakeCardUnmaskDelegate : public autofill::CardUnmaskDelegate {
 public:
  FakeCardUnmaskDelegate() : weak_factory_(this) {}

  FakeCardUnmaskDelegate(const FakeCardUnmaskDelegate&) = delete;
  FakeCardUnmaskDelegate& operator=(const FakeCardUnmaskDelegate&) = delete;

  virtual ~FakeCardUnmaskDelegate() {}

  // CardUnmaskDelegate implementation.
  void OnUnmaskPromptAccepted(
      const UserProvidedUnmaskDetails& unmask_details) override {
    unmask_details_ = unmask_details;
    // Fake the actual verification and just respond with success.
    web::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(^{
          autofill::payments::PaymentsAutofillClient::PaymentsRpcResult result =
              autofill::payments::PaymentsAutofillClient::PaymentsRpcResult::
                  kSuccess;
          [credit_card_verifier_ didReceiveUnmaskVerificationResult:result];
        }));
  }
  void OnUnmaskPromptCancelled() override {}
  bool ShouldOfferFidoAuth() const override { return false; }

  base::WeakPtr<FakeCardUnmaskDelegate> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  void SetCreditCardVerifier(CWVCreditCardVerifier* credit_card_verifier) {
    credit_card_verifier_ = credit_card_verifier;
  }

  const UserProvidedUnmaskDetails& GetUserProvidedUnmaskDetails() {
    return unmask_details_;
  }

 private:
  // Used to pass fake verification result back.
  __weak CWVCreditCardVerifier* credit_card_verifier_;

  // Used to verify unmask response matches.
  UserProvidedUnmaskDetails unmask_details_;

  base::WeakPtrFactory<FakeCardUnmaskDelegate> weak_factory_;
};

class CWVCreditCardVerifierTest : public PlatformTest {
 protected:
  CWVCreditCardVerifierTest() {
    l10n_util::OverrideLocaleWithCocoaLocale();
    ui::ResourceBundle::InitSharedInstanceWithLocale(
        l10n_util::GetLocaleOverride(), /*delegate=*/nullptr,
        ui::ResourceBundle::DO_NOT_LOAD_COMMON_RESOURCES);
    ui::ResourceBundle& resource_bundle =
        ui::ResourceBundle::GetSharedInstance();

    // Don't load 100P resource since no @1x devices are supported.
    if (ui::IsScaleFactorSupported(ui::k200Percent)) {
      base::FilePath pak_file_200;
      base::PathService::Get(base::DIR_ASSETS, &pak_file_200);
      pak_file_200 =
          pak_file_200.Append(FILE_PATH_LITERAL("web_view_200_percent.pak"));
      resource_bundle.AddDataPackFromPath(pak_file_200, ui::k200Percent);
    }
    if (ui::IsScaleFactorSupported(ui::k300Percent)) {
      base::FilePath pak_file_300;
      base::PathService::Get(base::DIR_ASSETS, &pak_file_300);
      pak_file_300 =
          pak_file_300.Append(FILE_PATH_LITERAL("web_view_300_percent.pak"));
      resource_bundle.AddDataPackFromPath(pak_file_300, ui::k300Percent);
    }

    pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    autofill::CreditCard credit_card = autofill::test::GetMaskedServerCard();
    credit_card_verifier_ = [[CWVCreditCardVerifier alloc]
         initWithPrefs:pref_service_.get()
        isOffTheRecord:NO
            creditCard:credit_card
                reason:autofill::payments::PaymentsAutofillClient::
                           UnmaskCardReason::kAutofill
              delegate:card_unmask_delegate_.GetWeakPtr()];
    card_unmask_delegate_.SetCreditCardVerifier(credit_card_verifier_);
  }

  ~CWVCreditCardVerifierTest() override {
    ui::ResourceBundle::CleanupSharedInstance();
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  FakeCardUnmaskDelegate card_unmask_delegate_;
  CWVCreditCardVerifier* credit_card_verifier_;
};

// Tests CWVCreditCardVerifier properties.
TEST_F(CWVCreditCardVerifierTest, Properties) {
  EXPECT_TRUE(credit_card_verifier_.creditCard);
  EXPECT_TRUE(credit_card_verifier_.navigationTitle);
  EXPECT_TRUE(credit_card_verifier_.instructionMessage);
  EXPECT_TRUE(credit_card_verifier_.confirmButtonLabel);
  // It is not sufficient to simply test for CVCHintImage != nil because
  // ui::ResourceBundle will return a placeholder image at @1x scale if the
  // underlying resource id is not found. Since no @1x devices are supported
  // anymore, check to make sure the UIImage scale matches that of the UIScreen.
  EXPECT_EQ(UIScreen.mainScreen.scale,
            credit_card_verifier_.CVCHintImage.scale);
  EXPECT_GT(credit_card_verifier_.expectedCVCLength, 0);
  EXPECT_FALSE(credit_card_verifier_.shouldRequestUpdateForExpirationDate);
  [credit_card_verifier_ requestUpdateForExpirationDate];
  EXPECT_TRUE(credit_card_verifier_.shouldRequestUpdateForExpirationDate);
}

// Tests CWVCreditCardVerifier's |isCVCValid| method.
TEST_F(CWVCreditCardVerifierTest, IsCVCValid) {
  EXPECT_FALSE([credit_card_verifier_ isCVCValid:@"1"]);
  EXPECT_FALSE([credit_card_verifier_ isCVCValid:@"12"]);
  EXPECT_FALSE([credit_card_verifier_ isCVCValid:@"1234"]);
  EXPECT_TRUE([credit_card_verifier_ isCVCValid:@"123"]);
}

// Tests CWVCreditCardVerifier's |isExpirationDateValidForMonth:year:| method.
TEST_F(CWVCreditCardVerifierTest, IsExpirationDateValid) {
  EXPECT_FALSE(
      [credit_card_verifier_ isExpirationDateValidForMonth:@"1" year:@"2"]);
  EXPECT_FALSE(
      [credit_card_verifier_ isExpirationDateValidForMonth:@"11" year:@"2"]);
  EXPECT_TRUE(
      [credit_card_verifier_ isExpirationDateValidForMonth:@"1" year:@"26"]);
  EXPECT_TRUE(
      [credit_card_verifier_ isExpirationDateValidForMonth:@"11" year:@"2226"]);
}

// Tests CWVCreditCardVerifier's verification method handles success case.
TEST_F(CWVCreditCardVerifierTest, VerifyCardSucceeded) {
  NSString* cvc = @"123";
  [credit_card_verifier_ loadRiskData:base::DoNothing()];
  __block BOOL completionCalled = NO;
  __block NSError* completionError;
  [credit_card_verifier_
          verifyWithCVC:cvc
        expirationMonth:@""  // Expiration dates are ignored here because
         expirationYear:@""  // |needsUpdateForExpirationDate| is NO.
               riskData:@"dummy-risk-data"
      completionHandler:^(NSError* error) {
        completionCalled = YES;
        completionError = error;
      }];

  const FakeCardUnmaskDelegate::UserProvidedUnmaskDetails& unmask_details_ =
      card_unmask_delegate_.GetUserProvidedUnmaskDetails();
  EXPECT_NSEQ(cvc, base::SysUTF16ToNSString(unmask_details_.cvc));

  [credit_card_verifier_ didReceiveUnmaskVerificationResult:
                             autofill::payments::PaymentsAutofillClient::
                                 PaymentsRpcResult::kSuccess];
  EXPECT_TRUE(completionCalled);
  EXPECT_TRUE(completionError == nil);
}

// Tests CWVCreditCardVerifier's verification method handles failure case.
TEST_F(CWVCreditCardVerifierTest, VerifyCardFailed) {
  NSString* cvc = @"123";
  [credit_card_verifier_ loadRiskData:base::DoNothing()];
  __block NSError* completionError;
  [credit_card_verifier_
          verifyWithCVC:cvc
        expirationMonth:@""  // Expiration dates are ignored here because
         expirationYear:@""  // |needsUpdateForExpirationDate| is NO.
               riskData:@"dummy-risk-data"
      completionHandler:^(NSError* error) {
        completionError = error;
      }];

  const FakeCardUnmaskDelegate::UserProvidedUnmaskDetails& unmask_details_ =
      card_unmask_delegate_.GetUserProvidedUnmaskDetails();
  EXPECT_NSEQ(cvc, base::SysUTF16ToNSString(unmask_details_.cvc));

  [credit_card_verifier_ didReceiveUnmaskVerificationResult:
                             autofill::payments::PaymentsAutofillClient::
                                 PaymentsRpcResult::kTryAgainFailure];
  ASSERT_TRUE(completionError != nil);
  EXPECT_EQ(CWVCreditCardVerifierErrorDomain, completionError.domain);
  EXPECT_EQ(CWVCreditCardVerificationErrorTryAgainFailure,
            completionError.code);
  EXPECT_TRUE([completionError.userInfo[CWVCreditCardVerifierRetryAllowedKey]
      boolValue]);
}

}  // namespace ios_web_view
