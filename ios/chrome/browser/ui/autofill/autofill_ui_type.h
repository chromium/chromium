// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_UI_TYPE_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_UI_TYPE_H_

#import <Foundation/Foundation.h>

// Each one of the following types with the exception of
// AutofillUITypeCreditCardExpDate, AutofillUITypeCreditCardBillingAddress,
// and AutofillUITypeCreditCardSaveToChrome corresponds to an
// autofill::ServerFieldType.
typedef NS_ENUM(NSInteger, AutofillUIType) {
  AutofillUITypeUnknown,
  AutofillUITypeCreditCardNumber,
  AutofillUITypeCreditCardHolderFullName,
  AutofillUITypeCreditCardExpMonth,
  AutofillUITypeCreditCardExpYear,
  AutofillUITypeCreditCardExpDate,
  AutofillUITypeCreditCardBillingAddress,
  AutofillUITypeCreditCardSaveToChrome,
  AutofillUITypeProfileHonorificPrefix,
  AutofillUITypeProfileFullName,
  AutofillUITypeProfileCompanyName,
  AutofillUITypeProfileHomeAddressStreet,
  AutofillUITypeProfileHomeAddressLine1,
  AutofillUITypeProfileHomeAddressLine2,
  AutofillUITypeProfileHomeAddressDependentLocality,
  AutofillUITypeProfileHomeAddressCity,
  AutofillUITypeProfileHomeAddressAdminLevel2,
  AutofillUITypeProfileHomeAddressState,
  AutofillUITypeProfileHomeAddressZip,
  AutofillUITypeProfileHomeAddressSortingCode,
  AutofillUITypeProfileHomeAddressCountry,
  AutofillUITypeProfileHomePhoneWholeNumber,
  AutofillUITypeProfileEmailAddress,
  AutofillUITypeNameFullWithHonorificPrefix,
  AutofillUITypeAddressHomeAddress
};

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_UI_TYPE_H_
