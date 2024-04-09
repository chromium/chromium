// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/omnibox_ui_features.h"

#import "base/metrics/field_trial_params.h"
#import "components/omnibox/common/omnibox_features.h"
#import "ui/base/device_form_factor.h"

BASE_FEATURE(kEnableSuggestionsScrollingOnIPad,
             "EnableSuggestionsScrollingOnIPad",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnablePopoutOmniboxIpad,
             "EnablePopoutOmniboxIpad",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kOmniboxKeyboardPasteButton,
             "OmniboxKeyboardPasteButton",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kOmniboxSuggestionsRTLImprovements,
             "OmniboxSuggestionsRTLImprovements",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kOmniboxLockIconEnabled,
             "OmniboxLockIconEnabled",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kOmniboxPopupRowContentConfiguration,
             "OmniboxPopupRowContentConfiguration",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsIpadPopoutOmniboxEnabled() {
  return base::FeatureList::IsEnabled(kEnablePopoutOmniboxIpad) &&
         ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET;
}

bool IsRichAutocompletionEnabled() {
  return base::FeatureList::IsEnabled(omnibox::kRichAutocompletion);
}

const char kRichAutocompletionParam[] = "RichAutocompletionParam";
const char kRichAutocompletionParamLabel[] = "Label";
const char kRichAutocompletionParamTextField[] = "TextField";

bool IsRichAutocompletionEnabled(RichAutocompletionImplementation type) {
  if (!IsRichAutocompletionEnabled()) {
    return false;
  }

  if (type == RichAutocompletionImplementation::kAny) {
    return true;
  }

  std::string featureParam = base::GetFieldTrialParamValueByFeature(
      omnibox::kRichAutocompletion, kRichAutocompletionParam);
  if (type == RichAutocompletionImplementation::kTextField) {
    return featureParam == kRichAutocompletionParamTextField;
  }

  // Label is the default.
  return true;
}
