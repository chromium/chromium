// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/features.h"
#import "base/metrics/field_trial_params.h"
#import "ui/base/device_form_factor.h"

namespace autofill::features {

BASE_FEATURE(kAutofillBrandingIOS,
             "AutofillBrandingIOS",
             base::FEATURE_ENABLED_BY_DEFAULT);

const char kAutofillBrandingIOSParamFrequencyTypeTwice[] = "twice";
const char kAutofillBrandingIOSParamFrequencyTypeUntilInteracted[] =
    "until-interacted";
const char kAutofillBrandingIOSParamFrequencyTypeDismissWhenInteracted[] =
    "dismiss-when-interacted";
const char kAutofillBrandingIOSParamFrequencyTypeAlwaysShowAndDismiss[] =
    "always-show-and-dismiss";
const char kAutofillBrandingIOSParamFrequencyTypeAlways[] = "always";

const char kAutofillBrandingIOSParamFrequencyTypePhone[] =
    "ios-autofill-branding-frequency-type-phone";
const char kAutofillBrandingIOSParamFrequencyTypeTablet[] =
    "ios-autofill-branding-frequency-type-tablet";

constexpr base::FeatureParam<AutofillBrandingFrequencyType>::Option
    kAutofillBrandingFrequencyTypeOptions[] = {
        {AutofillBrandingFrequencyType::kTwice,
         kAutofillBrandingIOSParamFrequencyTypeTwice},
        {AutofillBrandingFrequencyType::kUntilInteracted,
         kAutofillBrandingIOSParamFrequencyTypeUntilInteracted},
        {AutofillBrandingFrequencyType::kDismissWhenInteracted,
         kAutofillBrandingIOSParamFrequencyTypeDismissWhenInteracted},
        {AutofillBrandingFrequencyType::kAlwaysShowAndDismiss,
         kAutofillBrandingIOSParamFrequencyTypeAlwaysShowAndDismiss},
        {AutofillBrandingFrequencyType::kAlways,
         kAutofillBrandingIOSParamFrequencyTypeAlways},
};

constexpr base::FeatureParam<AutofillBrandingFrequencyType>
    kAutofillBrandingFrequencyPhone{
        &kAutofillBrandingIOS, kAutofillBrandingIOSParamFrequencyTypePhone,
        /*default=*/AutofillBrandingFrequencyType::kTwice,
        &kAutofillBrandingFrequencyTypeOptions};
constexpr base::FeatureParam<AutofillBrandingFrequencyType>
    kAutofillBrandingFrequencyTablet{
        &kAutofillBrandingIOS, kAutofillBrandingIOSParamFrequencyTypeTablet,
        /*default=*/AutofillBrandingFrequencyType::kTwice,
        &kAutofillBrandingFrequencyTypeOptions};

AutofillBrandingFrequencyType GetAutofillBrandingFrequencyType() {
  if (base::FeatureList::IsEnabled(kAutofillBrandingIOS)) {
    return ui::GetDeviceFormFactor() ==
                   ui::DeviceFormFactor::DEVICE_FORM_FACTOR_PHONE
               ? kAutofillBrandingFrequencyPhone.Get()
               : kAutofillBrandingFrequencyTablet.Get();
  }
  return AutofillBrandingFrequencyType::kNever;
}

bool ShouldAutofillBrandingDismissWithAnimation() {
  switch (GetAutofillBrandingFrequencyType()) {
    case AutofillBrandingFrequencyType::kDismissWhenInteracted:
    case AutofillBrandingFrequencyType::kAlwaysShowAndDismiss:
      return true;
    case AutofillBrandingFrequencyType::kNever:
    case AutofillBrandingFrequencyType::kTwice:
    case AutofillBrandingFrequencyType::kUntilInteracted:
    case AutofillBrandingFrequencyType::kAlways:
      return false;
  }
}

}  // namespace autofill::features
