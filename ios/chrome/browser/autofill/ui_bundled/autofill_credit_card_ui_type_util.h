// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_AUTOFILL_CREDIT_CARD_UI_TYPE_UTIL_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_AUTOFILL_CREDIT_CARD_UI_TYPE_UTIL_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import "components/autofill/core/browser/field_types.h"
#import "ios/chrome/browser/autofill/ui_bundled/autofill_credit_card_ui_type.h"

// Returns the autofill::FieldType equivalent to `type`.
autofill::FieldType AutofillTypeFromAutofillUITypeForCard(
    AutofillCreditCardUIType type);

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_AUTOFILL_CREDIT_CARD_UI_TYPE_UTIL_H_
