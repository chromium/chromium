// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/manual_fill/address_form.h"

#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "ios/chrome/browser/application_context.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Takes in an autofill profile and an autofill field type and returns the
// corresponding field value.
NSString* FieldValueOfTypeOnProfile(const autofill::AutofillProfile& profile,
                                    autofill::ServerFieldType fieldType) {
  return base::SysUTF16ToNSString(
      profile.GetInfo(autofill::AutofillType(fieldType),
                      GetApplicationContext()->GetApplicationLocale()));
}

}  // namespace

@implementation ManualFillAddress (AutofillProfile)

- (instancetype)initWithProfile:(const autofill::AutofillProfile&)profile {
  NSString* firstName =
      FieldValueOfTypeOnProfile(profile, autofill::NAME_FIRST);
  NSString* middleNameOrInitial =
      FieldValueOfTypeOnProfile(profile, autofill::NAME_MIDDLE);
  if (!middleNameOrInitial || middleNameOrInitial.length == 0) {
    middleNameOrInitial =
        FieldValueOfTypeOnProfile(profile, autofill::NAME_MIDDLE_INITIAL);
  }
  NSString* lastName = FieldValueOfTypeOnProfile(profile, autofill::NAME_LAST);
  NSString* company =
      FieldValueOfTypeOnProfile(profile, autofill::COMPANY_NAME);
  NSString* line1 =
      FieldValueOfTypeOnProfile(profile, autofill::ADDRESS_HOME_LINE1);
  NSString* line2 =
      FieldValueOfTypeOnProfile(profile, autofill::ADDRESS_HOME_LINE2);
  NSString* zip =
      FieldValueOfTypeOnProfile(profile, autofill::ADDRESS_HOME_ZIP);
  NSString* city =
      FieldValueOfTypeOnProfile(profile, autofill::ADDRESS_HOME_CITY);
  NSString* state =
      FieldValueOfTypeOnProfile(profile, autofill::ADDRESS_HOME_STATE);
  NSString* country =
      FieldValueOfTypeOnProfile(profile, autofill::ADDRESS_HOME_COUNTRY);
  NSString* phoneNumber =
      FieldValueOfTypeOnProfile(profile, autofill::PHONE_HOME_WHOLE_NUMBER);
  NSString* emailAddress =
      FieldValueOfTypeOnProfile(profile, autofill::EMAIL_ADDRESS);

  return [self initWithFirstName:firstName
             middleNameOrInitial:middleNameOrInitial
                        lastName:lastName
                         company:company
                           line1:line1
                           line2:line2
                             zip:zip
                            city:city
                           state:state
                         country:country
                     phoneNumber:phoneNumber
                    emailAddress:emailAddress];
}

+ (NSArray<ManualFillAddress*>*)manualFillAddressesFromProfiles:
    (std::vector<autofill::AutofillProfile*>)profiles {
  NSMutableArray<ManualFillAddress*>* manualFillAddresses =
      [[NSMutableArray alloc] initWithCapacity:profiles.size()];
  for (autofill::AutofillProfile* profile : profiles) {
    [manualFillAddresses
        addObject:[[ManualFillAddress alloc] initWithProfile:*profile]];
  }
  return manualFillAddresses;
}

@end
