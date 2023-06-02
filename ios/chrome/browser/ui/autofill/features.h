// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_FEATURES_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_FEATURES_H_

#include "base/feature_list.h"

namespace autofill::features {

// Feature flag to add the Chrome logo inide form input accessory bar.
BASE_DECLARE_FEATURE(kAutofillBrandingIOS);

// Available values for autofill branding frequency type key.
extern const char kAutofillBrandingIOSParamFrequencyTypeTwice[];
extern const char kAutofillBrandingIOSParamFrequencyTypeUntilInteracted[];
extern const char kAutofillBrandingIOSParamFrequencyTypeDismissWhenInteracted[];
extern const char kAutofillBrandingIOSParamFrequencyTypeAlwaysShowAndDismiss[];
extern const char kAutofillBrandingIOSParamFrequencyTypeAlways[];

// Variation param key that specifies the frequency type of the autofill
// branding. Default value is `kAutofillBrandingIOSParamFrequencyTypeTwice`.
extern const char kAutofillBrandingIOSParamFrequencyTypePhone[];
extern const char kAutofillBrandingIOSParamFrequencyTypeTablet[];

// Number of times autofill branding should be shown.
enum class AutofillBrandingFrequencyType {
  // Autofill branding should never be shown.
  kNever = 0,
  // Autofill branding should be shown for two times.
  kTwice,
  // Autofill branding should be shown until the user interacts with
  // keyboard accessory items. The branding would stay on the keyboard
  // accessories view until keyboard dismissal, but would not show again on
  // keyboard reappearance.
  kUntilInteracted,
  // Autofill branding should be shown until the user interacts with
  // keyboard accessory items. The branding would be dismissed with animation
  // upon user interaction.
  kDismissWhenInteracted,
  // Autofill branding should always be show and be dismissed with animation
  // immediately afterwards.
  kAlwaysShowAndDismiss,
  // Autofill branding should always be visible.
  kAlways,
};

// Returns the current AutofillBrandingFrequencyType according to the feature
// flag and experiment "AutofillBrandingIOS".
AutofillBrandingFrequencyType GetAutofillBrandingFrequencyType();

// Returns whether the autofill branding should be dismissed by animating to the
// leading edge of the device.
bool ShouldAutofillBrandingDismissWithAnimation();

}  // namespace autofill::features

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_FEATURES_H_
