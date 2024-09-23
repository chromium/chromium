// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_address+AutofillProfile.h"

#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/autofill/core/browser/data_model/autofill_i18n_api.h"
#import "components/autofill/core/browser/data_model/autofill_profile.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

using autofill::AutofillProfile;
using ManualFillAddressFormAutofilliOSTest = PlatformTest;

namespace {

void SetProfileFieldTypeValue(AutofillProfile* profile,
                              const autofill::FieldType fieldType,
                              NSString* value) {
  const std::u16string v = base::SysNSStringToUTF16(value);
  const std::string& app_locale =
      GetApplicationContext()->GetApplicationLocale();
  profile->SetInfo(fieldType, v, app_locale);
}

}  // namespace

// Tests the creation of an address from an autofill::AutofillProfile.
TEST_F(ManualFillAddressFormAutofilliOSTest, CreationWithMiddleName) {
  NSString* firstName = @"First";
  NSString* middleName = @"Middle";
  NSString* lastName = @"Last";
  NSString* company = @"Google";
  NSString* line1 = @"10 Main Street";
  NSString* line2 = @"Appt 16";
  NSString* zip = @"12345";
  NSString* city = @"Springfield";
  NSString* state = @"State";
  NSString* country = @"US";
  NSString* phoneNumber = @"6502345678";
  NSString* emailAddress = @"john@doe";

  AutofillProfile* profile = new AutofillProfile(
      autofill::i18n_model_definition::kLegacyHierarchyCountryCode);
  SetProfileFieldTypeValue(profile, autofill::NAME_FIRST, firstName);
  SetProfileFieldTypeValue(profile, autofill::NAME_MIDDLE, middleName);
  SetProfileFieldTypeValue(profile, autofill::NAME_LAST, lastName);
  SetProfileFieldTypeValue(profile, autofill::COMPANY_NAME, company);
  SetProfileFieldTypeValue(profile, autofill::ADDRESS_HOME_LINE1, line1);
  SetProfileFieldTypeValue(profile, autofill::ADDRESS_HOME_LINE2, line2);
  SetProfileFieldTypeValue(profile, autofill::ADDRESS_HOME_ZIP, zip);
  SetProfileFieldTypeValue(profile, autofill::ADDRESS_HOME_CITY, city);
  SetProfileFieldTypeValue(profile, autofill::ADDRESS_HOME_STATE, state);
  SetProfileFieldTypeValue(profile, autofill::ADDRESS_HOME_COUNTRY, country);
  SetProfileFieldTypeValue(profile, autofill::PHONE_HOME_WHOLE_NUMBER,
                           phoneNumber);
  SetProfileFieldTypeValue(profile, autofill::EMAIL_ADDRESS, emailAddress);

  ManualFillAddress* manualFillAddress =
      [[ManualFillAddress alloc] initWithProfile:*profile];

  EXPECT_TRUE(manualFillAddress);
  EXPECT_NSEQ(firstName, manualFillAddress.firstName);
  EXPECT_NSEQ(middleName, manualFillAddress.middleNameOrInitial);
  EXPECT_NSEQ(lastName, manualFillAddress.lastName);
  EXPECT_NSEQ(company, manualFillAddress.company);
  EXPECT_NSEQ(line1, manualFillAddress.line1);
  EXPECT_NSEQ(line2, manualFillAddress.line2);
  EXPECT_NSEQ(zip, manualFillAddress.zip);
  EXPECT_NSEQ(city, manualFillAddress.city);
  EXPECT_NSEQ(state, manualFillAddress.state);
  EXPECT_NSEQ(@"United States", manualFillAddress.country);
  EXPECT_NSEQ(phoneNumber, manualFillAddress.phoneNumber);
  EXPECT_NSEQ(emailAddress, manualFillAddress.emailAddress);
}

// Tests the creation of an address from an autofill::AutofillProfile.
TEST_F(ManualFillAddressFormAutofilliOSTest, CreationWithMiddleInitial) {
  NSString* firstName = @"First";
  NSString* middleInitial = @"M";
  NSString* lastName = @"Last";
  NSString* company = @"Google";
  NSString* line1 = @"10 Main Street";
  NSString* line2 = @"Appt 16";
  NSString* zip = @"12345";
  NSString* city = @"Springfield";
  NSString* state = @"State";
  NSString* country = @"US";
  NSString* phoneNumber = @"6502345678";
  NSString* emailAddress = @"john@doe";

  AutofillProfile* profile = new AutofillProfile(
      autofill::i18n_model_definition::kLegacyHierarchyCountryCode);
  SetProfileFieldTypeValue(profile, autofill::NAME_FIRST, firstName);
  SetProfileFieldTypeValue(profile, autofill::NAME_MIDDLE_INITIAL,
                           middleInitial);
  SetProfileFieldTypeValue(profile, autofill::NAME_LAST, lastName);
  SetProfileFieldTypeValue(profile, autofill::COMPANY_NAME, company);
  SetProfileFieldTypeValue(profile, autofill::ADDRESS_HOME_LINE1, line1);
  SetProfileFieldTypeValue(profile, autofill::ADDRESS_HOME_LINE2, line2);
  SetProfileFieldTypeValue(profile, autofill::ADDRESS_HOME_ZIP, zip);
  SetProfileFieldTypeValue(profile, autofill::ADDRESS_HOME_CITY, city);
  SetProfileFieldTypeValue(profile, autofill::ADDRESS_HOME_STATE, state);
  SetProfileFieldTypeValue(profile, autofill::ADDRESS_HOME_COUNTRY, country);
  SetProfileFieldTypeValue(profile, autofill::PHONE_HOME_WHOLE_NUMBER,
                           phoneNumber);
  SetProfileFieldTypeValue(profile, autofill::EMAIL_ADDRESS, emailAddress);

  ManualFillAddress* manualFillAddress =
      [[ManualFillAddress alloc] initWithProfile:*profile];

  EXPECT_TRUE(manualFillAddress);
  EXPECT_NSEQ(firstName, manualFillAddress.firstName);
  EXPECT_NSEQ(middleInitial, manualFillAddress.middleNameOrInitial);
  EXPECT_NSEQ(lastName, manualFillAddress.lastName);
  EXPECT_NSEQ(company, manualFillAddress.company);
  EXPECT_NSEQ(line1, manualFillAddress.line1);
  EXPECT_NSEQ(line2, manualFillAddress.line2);
  EXPECT_NSEQ(zip, manualFillAddress.zip);
  EXPECT_NSEQ(city, manualFillAddress.city);
  EXPECT_NSEQ(state, manualFillAddress.state);
  EXPECT_NSEQ(@"United States", manualFillAddress.country);
  EXPECT_NSEQ(phoneNumber, manualFillAddress.phoneNumber);
  EXPECT_NSEQ(emailAddress, manualFillAddress.emailAddress);
}
