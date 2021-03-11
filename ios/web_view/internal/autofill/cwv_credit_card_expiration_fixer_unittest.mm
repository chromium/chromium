// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/autofill/cwv_credit_card_expiration_fixer_internal.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/strings/sys_string_conversions.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#import "ios/web_view/internal/autofill/cwv_credit_card_internal.h"
#include "ios/web_view/test/test_with_locale_and_resources.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios_web_view {

class CWVCreditCardExpirationFixerTest : public TestWithLocaleAndResources {
 public:
  void AcceptExpiration(const std::u16string& month,
                        const std::u16string& year) {
    _accepted_month = base::SysUTF16ToNSString(month);
    _accepted_year = base::SysUTF16ToNSString(year);
  }
  NSString* _accepted_month;
  NSString* _accepted_year;
};

// Tests CWVCreditCardExpirationFixer properties.
TEST_F(CWVCreditCardExpirationFixerTest, Properties) {
  autofill::CreditCard card = autofill::test::GetCreditCard();
  CWVCreditCardExpirationFixer* fixer = [[CWVCreditCardExpirationFixer alloc]
      initWithCreditCard:card
                callback:base::DoNothing()];
  EXPECT_EQ(card, *fixer.card.internalCard);
  EXPECT_TRUE(fixer.titleText);
  EXPECT_TRUE(fixer.saveButtonLabel);
  EXPECT_TRUE(fixer.cardLabel);
  EXPECT_TRUE(fixer.cancelButtonLabel);
  EXPECT_TRUE(fixer.inputLabel);
  EXPECT_TRUE(fixer.dateSeparator);
  EXPECT_TRUE(fixer.invalidDateErrorMessage);
}

// Tests CWVCreditCardExpirationFixer properly accepts a valid expiration.
TEST_F(CWVCreditCardExpirationFixerTest, AcceptValidExpiration) {
  autofill::CreditCard card = autofill::test::GetCreditCard();
  base::OnceCallback<void(const std::u16string&, const std::u16string&)>
      callback =
          base::BindOnce(&CWVCreditCardExpirationFixerTest::AcceptExpiration,
                         base::Unretained(this));
  CWVCreditCardExpirationFixer* fixer = [[CWVCreditCardExpirationFixer alloc]
      initWithCreditCard:card
                callback:std::move(callback)];

  NSString* valid_month = @"12";
  NSString* valid_year = @"2099";
  EXPECT_TRUE([fixer acceptWithMonth:valid_month year:valid_year]);
  EXPECT_NSEQ(valid_month, _accepted_month);
  EXPECT_NSEQ(valid_year, _accepted_year);
}

// Tests CWVCreditCardExpirationFixer properly rejects invalid expirations.
TEST_F(CWVCreditCardExpirationFixerTest, RejectsInvalidExpirations) {
  autofill::CreditCard card = autofill::test::GetCreditCard();
  base::OnceCallback<void(const std::u16string&, const std::u16string&)>
      callback =
          base::BindOnce(&CWVCreditCardExpirationFixerTest::AcceptExpiration,
                         base::Unretained(this));
  CWVCreditCardExpirationFixer* fixer = [[CWVCreditCardExpirationFixer alloc]
      initWithCreditCard:card
                callback:std::move(callback)];

  // Date values which are out of range should not be accepted.
  EXPECT_FALSE([fixer acceptWithMonth:@"13" year:@"2099"]);
  EXPECT_FALSE([fixer acceptWithMonth:@"12" year:@"-2099"]);

  // Date values which are alphanumeric should not be accepted.
  EXPECT_FALSE([fixer acceptWithMonth:@"December" year:@"2099"]);
  EXPECT_FALSE([fixer acceptWithMonth:@"12" year:@"Twenty Ninety Nine"]);

  // No expirations should have been accepted.
  EXPECT_FALSE(_accepted_month);
  EXPECT_FALSE(_accepted_year);
}

}  // namespace ios_web_view
