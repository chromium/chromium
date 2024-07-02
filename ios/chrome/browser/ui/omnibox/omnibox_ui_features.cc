// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/omnibox_ui_features.h"

#import "base/metrics/field_trial_params.h"
#import "components/omnibox/common/omnibox_features.h"
#import "ui/base/device_form_factor.h"

BASE_FEATURE(kOmniboxLockIconEnabled,
             "OmniboxLockIconEnabled",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kOmniboxActionsInSuggest,
             "OmniboxIOSActionsInSuggest",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsRichAutocompletionEnabled() {
  return base::FeatureList::IsEnabled(omnibox::kRichAutocompletion);
}

const char kRichAutocompletionParam[] = "RichAutocompletionParam";
const char kRichAutocompletionParamLabel[] = "Label";
const char kRichAutocompletionParamTextField[] = "TextField";
const char kRichAutocompletionParamNoAdditionalText[] = "NoAdditionalText";

bool IsRichAutocompletionEnabled(RichAutocompletionImplementation type) {
  if (!IsRichAutocompletionEnabled()) {
    return false;
  }

  if (type == RichAutocompletionImplementation::kAny) {
    return true;
  }

  std::string featureParam = base::GetFieldTrialParamValueByFeature(
      omnibox::kRichAutocompletion, kRichAutocompletionParam);
  if (type == RichAutocompletionImplementation::kLabel) {
    return featureParam == kRichAutocompletionParamLabel;
  } else if (type == RichAutocompletionImplementation::kNoAdditionalText) {
    return featureParam == kRichAutocompletionParamNoAdditionalText;
  }

  // TextField is the default.
  return featureParam == kRichAutocompletionParamTextField ||
         featureParam.empty();
}
