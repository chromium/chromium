// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/autofill/cwv_credit_card_internal.h"

#import <UIKit/UIKit.h>
#include <string>

#include "base/base_paths.h"
#include "base/path_service.h"
#include "base/strings/sys_string_conversions.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/base/resource/resource_bundle.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios_web_view {

class CWVCreditCardTest : public PlatformTest {
 protected:
  CWVCreditCardTest() {
    l10n_util::OverrideLocaleWithCocoaLocale();
    ui::ResourceBundle::InitSharedInstanceWithLocale(
        l10n_util::GetLocaleOverride(), /*delegate=*/nullptr,
        ui::ResourceBundle::DO_NOT_LOAD_COMMON_RESOURCES);
    ui::ResourceBundle& resource_bundle =
        ui::ResourceBundle::GetSharedInstance();

    // Don't load 100P resource since no @1x devices are supported.
    if (ui::ResourceBundle::IsScaleFactorSupported(ui::SCALE_FACTOR_200P)) {
      base::FilePath pak_file_200;
      base::PathService::Get(base::DIR_MODULE, &pak_file_200);
      pak_file_200 =
          pak_file_200.Append(FILE_PATH_LITERAL("web_view_200_percent.pak"));
      resource_bundle.AddDataPackFromPath(pak_file_200, ui::SCALE_FACTOR_200P);
    }
    if (ui::ResourceBundle::IsScaleFactorSupported(ui::SCALE_FACTOR_300P)) {
      base::FilePath pak_file_300;
      base::PathService::Get(base::DIR_MODULE, &pak_file_300);
      pak_file_300 =
          pak_file_300.Append(FILE_PATH_LITERAL("web_view_300_percent.pak"));
      resource_bundle.AddDataPackFromPath(pak_file_300, ui::SCALE_FACTOR_300P);
    }
  }

  ~CWVCreditCardTest() override { ui::ResourceBundle::CleanupSharedInstance(); }
};

// Tests CWVCreditCard initialization.
TEST_F(CWVCreditCardTest, Initialization) {
  autofill::CreditCard credit_card = autofill::test::GetCreditCard();
  CWVCreditCard* cwv_credit_card =
      [[CWVCreditCard alloc] initWithCreditCard:credit_card];
  EXPECT_EQ(credit_card, *[cwv_credit_card internalCard]);

  // It is not sufficient to simply test for networkIcon != nil because
  // ui::ResourceBundle will return a placeholder image at @1x scale if the
  // underlying resource id is not found. Since no @1x devices are supported
  // anymore, check to make sure the UIImage scale matches that of the UIScreen.
  EXPECT_TRUE(cwv_credit_card.networkIcon.scale == UIScreen.mainScreen.scale);
}

// Tests CWVCreditCard updates properties.
TEST_F(CWVCreditCardTest, ModifyProperties) {
  autofill::CreditCard credit_card = autofill::test::GetCreditCard();
  CWVCreditCard* cwv_credit_card =
      [[CWVCreditCard alloc] initWithCreditCard:credit_card];

  std::string locale = l10n_util::GetLocaleOverride();
  autofill::CreditCard new_credit_card = autofill::test::GetCreditCard2();
  NSString* new_card_holder_full_name = base::SysUTF16ToNSString(
      new_credit_card.GetInfo(autofill::CREDIT_CARD_NAME_FULL, locale));
  NSString* new_card_number = base::SysUTF16ToNSString(
      new_credit_card.GetInfo(autofill::CREDIT_CARD_NUMBER, locale));
  NSString* new_expiration_month = base::SysUTF16ToNSString(
      new_credit_card.GetInfo(autofill::CREDIT_CARD_EXP_MONTH, locale));
  NSString* new_expiration_year = base::SysUTF16ToNSString(
      new_credit_card.GetInfo(autofill::CREDIT_CARD_EXP_4_DIGIT_YEAR, locale));
  cwv_credit_card.cardHolderFullName = new_card_holder_full_name;
  cwv_credit_card.cardNumber = new_card_number;
  cwv_credit_card.expirationMonth = new_expiration_month;
  cwv_credit_card.expirationYear = new_expiration_year;

  EXPECT_NSEQ(new_card_holder_full_name, cwv_credit_card.cardHolderFullName);
  EXPECT_NSEQ(new_card_number, cwv_credit_card.cardNumber);
  EXPECT_NSEQ(new_expiration_month, cwv_credit_card.expirationMonth);
  EXPECT_NSEQ(new_expiration_year, cwv_credit_card.expirationYear);
}

}  // namespace ios_web_view
