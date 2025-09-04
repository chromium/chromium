// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/public/omnibox_ui_features.h"

#import "base/metrics/field_trial_params.h"
#import "components/omnibox/common/omnibox_features.h"
#import "ui/base/device_form_factor.h"

bool IsRichAutocompletionEnabled() {
  return base::FeatureList::IsEnabled(omnibox::kRichAutocompletion);
}

BASE_FEATURE(kBeginCursorAtPointTentativeFix,
             base::FEATURE_DISABLED_BY_DEFAULT);
