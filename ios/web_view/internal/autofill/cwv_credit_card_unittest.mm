// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

#import <string>

#import "base/base_paths.h"
#import "base/path_service.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/data_model/payments/credit_card.h"
#import "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#import "ios/web_view/internal/autofill/cwv_credit_card_internal.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "ui/base/resource/resource_bundle.h"
#import "ui/base/resource/resource_scale_factor.h"

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

// Tests CWVCreditCard properly wraps the internal card.
TEST_F(CWVCreditCardTest, ReadProperties) {
  autofill::CreditCard credit_card = autofill::test::GetCreditCard();
  CWVCreditCard* cwv_credit_card =
      [[CWVCreditCard alloc] initWithCreditCard:credit_card];

  std::string locale = l10n_util::GetLocaleOverride();
  NSString* card_holder_full_name = base::SysUTF16ToNSString(
      credit_card.GetInfo(autofill::CREDIT_CARD_NAME_FULL, locale));
  NSString* card_number = base::SysUTF16ToNSString(
      credit_card.GetInfo(autofill::CREDIT_CARD_NUMBER, locale));
  NSString* expiration_month = base::SysUTF16ToNSString(
      credit_card.GetInfo(autofill::CREDIT_CARD_EXP_MONTH, locale));
  NSString* expiration_year = base::SysUTF16ToNSString(
      credit_card.GetInfo(autofill::CREDIT_CARD_EXP_4_DIGIT_YEAR, locale));

  EXPECT_NSEQ(card_holder_full_name, cwv_credit_card.cardHolderFullName);
  EXPECT_NSEQ(card_number, cwv_credit_card.cardNumber);
  EXPECT_NSEQ(expiration_month, cwv_credit_card.expirationMonth);
  EXPECT_NSEQ(expiration_year, cwv_credit_card.expirationYear);
}

}  // namespace ios_web_view
