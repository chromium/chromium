// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/autofill/cwv_autofill_profile_internal.h"

#include "base/strings/sys_string_conversions.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "ios/web_view/internal/app/application_context.h"
#import "ios/web_view/internal/utils/nsobject_description_utils.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface CWVAutofillProfile ()

// Sets |value| for |type| in |_internalProfile|.
- (void)setValue:(NSString*)value forType:(autofill::ServerFieldType)type;
// Gets |value| for |type| from |_internalProfile|.
- (NSString*)valueForType:(autofill::ServerFieldType)type;

@end

@implementation CWVAutofillProfile {
  autofill::AutofillProfile _internalProfile;
}

- (instancetype)initWithProfile:(const autofill::AutofillProfile&)profile {
  self = [super init];
  if (self) {
    _internalProfile = profile;
  }
  return self;
}

#pragma mark - Public Methods

- (NSString*)name {
  return [self valueForType:autofill::NAME_FULL];
}

- (void)setName:(NSString*)name {
  [self setValue:name forType:autofill::NAME_FULL];
}

- (NSString*)company {
  return [self valueForType:autofill::COMPANY_NAME];
}

- (void)setCompany:(NSString*)company {
  [self setValue:company forType:autofill::COMPANY_NAME];
}

- (NSString*)address1 {
  return [self valueForType:autofill::ADDRESS_HOME_LINE1];
}

- (void)setAddress1:(NSString*)address1 {
  [self setValue:address1 forType:autofill::ADDRESS_HOME_LINE1];
}

- (NSString*)address2 {
  return [self valueForType:autofill::ADDRESS_HOME_LINE2];
}

- (void)setAddress2:(NSString*)address2 {
  [self setValue:address2 forType:autofill::ADDRESS_HOME_LINE2];
}

- (NSString*)city {
  return [self valueForType:autofill::ADDRESS_HOME_CITY];
}

- (void)setCity:(NSString*)city {
  [self setValue:city forType:autofill::ADDRESS_HOME_CITY];
}

- (NSString*)state {
  return [self valueForType:autofill::ADDRESS_HOME_STATE];
}

- (void)setState:(NSString*)state {
  [self setValue:state forType:autofill::ADDRESS_HOME_STATE];
}

- (NSString*)zipcode {
  return [self valueForType:autofill::ADDRESS_HOME_ZIP];
}

- (void)setZipcode:(NSString*)zipcode {
  [self setValue:zipcode forType:autofill::ADDRESS_HOME_ZIP];
}

- (NSString*)country {
  return [self valueForType:autofill::ADDRESS_HOME_COUNTRY];
}

- (void)setCountry:(NSString*)country {
  [self setValue:country forType:autofill::ADDRESS_HOME_COUNTRY];
}

- (NSString*)phone {
  return [self valueForType:autofill::PHONE_HOME_WHOLE_NUMBER];
}

- (void)setPhone:(NSString*)phone {
  [self setValue:phone forType:autofill::PHONE_HOME_WHOLE_NUMBER];
}

- (NSString*)email {
  return [self valueForType:autofill::EMAIL_ADDRESS];
}

- (void)setEmail:(NSString*)email {
  [self setValue:email forType:autofill::EMAIL_ADDRESS];
}

#pragma mark - NSObject

- (NSString*)debugDescription {
  NSString* debugDescription = [super debugDescription];
  return [debugDescription
      stringByAppendingFormat:@"\n%@", CWVPropertiesDescription(self)];
}

#pragma mark - Internal Methods

- (autofill::AutofillProfile*)internalProfile {
  return &_internalProfile;
}

#pragma mark - Private Methods

- (void)setValue:(NSString*)value forType:(autofill::ServerFieldType)type {
  const std::string& locale =
      ios_web_view::ApplicationContext::GetInstance()->GetApplicationLocale();
  _internalProfile.SetInfo(type, base::SysNSStringToUTF16(value), locale);
}

- (NSString*)valueForType:(autofill::ServerFieldType)type {
  const std::string& locale =
      ios_web_view::ApplicationContext::GetInstance()->GetApplicationLocale();
  return base::SysUTF16ToNSString(_internalProfile.GetInfo(type, locale));
}

@end
