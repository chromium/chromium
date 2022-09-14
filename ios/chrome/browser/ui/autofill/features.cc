// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/features.h"
#import "base/metrics/field_trial_params.h"

namespace autofill::features {

const base::Feature kAutofillEnableNewCardUnmaskPromptView{
    "AutofillEnableNewCardUnmaskPromptView", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kAutofillBrandingIOS{"AutofillBrandingIOS",
                                         base::FEATURE_DISABLED_BY_DEFAULT};
const char kAutofillBrandingIOSParam[] = "ios-autofill-branding-monotones";

AutofillBrandingType GetAutofillBrandingType() {
  if (base::FeatureList::IsEnabled(kAutofillBrandingIOS)) {
    return base::GetFieldTrialParamByFeatureAsBool(
               kAutofillBrandingIOS, kAutofillBrandingIOSParam, false)
               ? AutofillBrandingType::kMonotone
               : AutofillBrandingType::kFullColor;
  }
  return AutofillBrandingType::kDisabled;
}

}  // namespace autofill::features
