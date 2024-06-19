// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_AUTOFILL_CREDIT_CARD_UI_TYPE_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_AUTOFILL_CREDIT_CARD_UI_TYPE_H_

#import <Foundation/Foundation.h>

// Each one of the following credit card types with the exception of
// kExpDate, kBillingAddress, and kSaveToChrome corresponds to an
// autofill::FieldType.
enum class AutofillCreditCardUIType {
  kUnknown,
  kNumber,
  kFullName,
  kExpMonth,
  kExpYear,
  kExpDate,
  kBillingAddress,
  kSaveToChrome
};

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_AUTOFILL_CREDIT_CARD_UI_TYPE_H_
