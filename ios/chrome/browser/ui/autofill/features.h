// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_FEATURES_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_FEATURES_H_

#include "base/feature_list.h"

namespace autofill::features {

// Feature flag and variatns to add the Chrome logo inide form input accessory
// bar.
BASE_DECLARE_FEATURE(kAutofillBrandingIOS);
extern const char kAutofillBrandingIOSParam[];

// Autofill branding options.
enum class AutofillBrandingType {
  // Autofill branding enabled with full color Chrome logo.
  kFullColor = 0,
  // Autofill branding enabled with monotone Chrome logo.
  kMonotone,
  // Autofill branding not enabled.
  kDisabled,
};

// Returns the current AutofillBrandingType according to the feature flag and
// experiment "AutofillBrandingIOS".
AutofillBrandingType GetAutofillBrandingType();

}  // namespace autofill::features

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_FEATURES_H_
