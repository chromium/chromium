// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/manual_fill/address_form.h"

#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/geo/country_names.h"
#include "ios/chrome/browser/application_context.h"
#include "testing/platform_test.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using autofill::AutofillProfile;
using ManualFillAddressFormAutofilliOSTest = PlatformTest;

namespace {

void SetProfileFieldTypeValue(AutofillProfile* profile,
                              const autofill::ServerFieldType fieldType,
                              NSString* value) {
  const base::string16 v = base::SysNSStringToUTF16(value);
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
  NSString* phoneNumber = @"123-456-789";
  NSString* emailAddress = @"john@doe";

  autofill::CountryNames::SetLocaleString("en-US");

  AutofillProfile* profile = new AutofillProfile();
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
  EXPECT_TRUE([firstName isEqualToString:manualFillAddress.firstName]);
  EXPECT_TRUE(
      [middleName isEqualToString:manualFillAddress.middleNameOrInitial]);
  EXPECT_TRUE([lastName isEqualToString:manualFillAddress.lastName]);
  EXPECT_TRUE([company isEqualToString:manualFillAddress.company]);
  EXPECT_TRUE([line1 isEqualToString:manualFillAddress.line1]);
  EXPECT_TRUE([line2 isEqualToString:manualFillAddress.line2]);
  EXPECT_TRUE([zip isEqualToString:manualFillAddress.zip]);
  EXPECT_TRUE([city isEqualToString:manualFillAddress.city]);
  EXPECT_TRUE([state isEqualToString:manualFillAddress.state]);
  EXPECT_TRUE([@"United States" isEqualToString:manualFillAddress.country]);
  EXPECT_TRUE([phoneNumber isEqualToString:manualFillAddress.phoneNumber]);
  EXPECT_TRUE([emailAddress isEqualToString:manualFillAddress.emailAddress]);
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
  NSString* phoneNumber = @"123-456-789";
  NSString* emailAddress = @"john@doe";

  autofill::CountryNames::SetLocaleString("en-US");

  AutofillProfile* profile = new AutofillProfile();
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
  EXPECT_TRUE([firstName isEqualToString:manualFillAddress.firstName]);
  EXPECT_TRUE(
      [middleInitial isEqualToString:manualFillAddress.middleNameOrInitial]);
  EXPECT_TRUE([lastName isEqualToString:manualFillAddress.lastName]);
  EXPECT_TRUE([company isEqualToString:manualFillAddress.company]);
  EXPECT_TRUE([line1 isEqualToString:manualFillAddress.line1]);
  EXPECT_TRUE([line2 isEqualToString:manualFillAddress.line2]);
  EXPECT_TRUE([zip isEqualToString:manualFillAddress.zip]);
  EXPECT_TRUE([city isEqualToString:manualFillAddress.city]);
  EXPECT_TRUE([state isEqualToString:manualFillAddress.state]);
  EXPECT_TRUE([@"United States" isEqualToString:manualFillAddress.country]);
  EXPECT_TRUE([phoneNumber isEqualToString:manualFillAddress.phoneNumber]);
  EXPECT_TRUE([emailAddress isEqualToString:manualFillAddress.emailAddress]);
}
