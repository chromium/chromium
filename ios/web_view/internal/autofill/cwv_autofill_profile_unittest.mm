// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/autofill/cwv_autofill_profile_internal.h"

#import <Foundation/Foundation.h>
#include <string>

#include "base/strings/sys_string_conversions.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/base/resource/resource_bundle.h"

namespace ios_web_view {

class CWVAutofillProfileTest : public PlatformTest {
 protected:
  CWVAutofillProfileTest() {
    l10n_util::OverrideLocaleWithCocoaLocale();
    ui::ResourceBundle::InitSharedInstanceWithLocale(
        l10n_util::GetLocaleOverride(), /*delegate=*/nullptr,
        ui::ResourceBundle::DO_NOT_LOAD_COMMON_RESOURCES);
  }

  ~CWVAutofillProfileTest() override {
    ui::ResourceBundle::CleanupSharedInstance();
  }
};

// Tests CWVAutofillProfile initialization.
TEST_F(CWVAutofillProfileTest, Initialization) {
  autofill::AutofillProfile profile = autofill::test::GetFullProfile();
  CWVAutofillProfile* cwv_profile =
      [[CWVAutofillProfile alloc] initWithProfile:profile];
  EXPECT_EQ(profile, *[cwv_profile internalProfile]);
}

// Tests CWVAutofillProfile updates properties.
TEST_F(CWVAutofillProfileTest, ModifyProperties) {
  autofill::AutofillProfile profile = autofill::test::GetFullProfile();
  CWVAutofillProfile* cwv_profile =
      [[CWVAutofillProfile alloc] initWithProfile:profile];

  std::string locale = l10n_util::GetLocaleOverride();
  autofill::AutofillProfile new_profile = autofill::test::GetFullProfile2();
  NSString* new_name = base::SysUTF16ToNSString(
      new_profile.GetInfo(autofill::NAME_FULL, locale));
  NSString* new_company = base::SysUTF16ToNSString(
      new_profile.GetInfo(autofill::COMPANY_NAME, locale));
  NSString* new_address1 = base::SysUTF16ToNSString(
      new_profile.GetInfo(autofill::ADDRESS_HOME_LINE1, locale));
  NSString* new_address2 = base::SysUTF16ToNSString(
      new_profile.GetInfo(autofill::ADDRESS_HOME_LINE2, locale));
  NSString* new_city = base::SysUTF16ToNSString(
      new_profile.GetInfo(autofill::ADDRESS_HOME_CITY, locale));
  NSString* new_state = base::SysUTF16ToNSString(
      new_profile.GetInfo(autofill::ADDRESS_HOME_STATE, locale));
  NSString* new_zipcode = base::SysUTF16ToNSString(
      new_profile.GetInfo(autofill::ADDRESS_HOME_ZIP, locale));
  NSString* new_country = base::SysUTF16ToNSString(
      new_profile.GetInfo(autofill::ADDRESS_HOME_COUNTRY, locale));
  NSString* new_phone = base::SysUTF16ToNSString(
      new_profile.GetInfo(autofill::PHONE_HOME_WHOLE_NUMBER, locale));
  NSString* new_email = base::SysUTF16ToNSString(
      new_profile.GetInfo(autofill::EMAIL_ADDRESS, locale));
  cwv_profile.name = new_name;
  cwv_profile.company = new_company;
  cwv_profile.address1 = new_address1;
  cwv_profile.address2 = new_address2;
  cwv_profile.city = new_city;
  cwv_profile.state = new_state;
  cwv_profile.zipcode = new_zipcode;
  cwv_profile.country = new_country;
  cwv_profile.phone = new_phone;
  cwv_profile.email = new_email;

  EXPECT_NSEQ(new_name, cwv_profile.name);
  EXPECT_NSEQ(new_company, cwv_profile.company);
  EXPECT_NSEQ(new_address1, cwv_profile.address1);
  EXPECT_NSEQ(new_address2, cwv_profile.address2);
  EXPECT_NSEQ(new_city, cwv_profile.city);
  EXPECT_NSEQ(new_state, cwv_profile.state);
  EXPECT_NSEQ(new_zipcode, cwv_profile.zipcode);
  EXPECT_NSEQ(new_country, cwv_profile.country);
  EXPECT_NSEQ(new_phone, cwv_profile.phone);
  EXPECT_NSEQ(new_email, cwv_profile.email);
}

}  // namespace ios_web_view
