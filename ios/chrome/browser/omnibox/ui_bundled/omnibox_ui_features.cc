// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/ui_bundled/omnibox_ui_features.h"

#import "base/metrics/field_trial_params.h"
#import "components/omnibox/common/omnibox_features.h"
#import "ui/base/device_form_factor.h"

BASE_FEATURE(kOmniboxActionsInSuggest,
             "OmniboxIOSActionsInSuggest",
             base::FEATURE_ENABLED_BY_DEFAULT);

bool IsRichAutocompletionEnabled() {
  return base::FeatureList::IsEnabled(omnibox::kRichAutocompletion);
}
