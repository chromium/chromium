// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_UI_TYPE_UTIL_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_UI_TYPE_UTIL_H_

#include "components/autofill/core/browser/field_types.h"
#import "ios/chrome/browser/ui/autofill/autofill_ui_type.h"

// Returns the AutofillUIType equivalent to |type|.
AutofillUIType AutofillUITypeFromAutofillType(autofill::ServerFieldType type);

// Returns the autofill::ServerFieldType equivalent to |type|.
autofill::ServerFieldType AutofillTypeFromAutofillUIType(AutofillUIType type);

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_UI_TYPE_UTIL_H_
