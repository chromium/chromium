// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/public/features.h"

#include "base/command_line.h"
#include "base/metrics/field_trial_params.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

const base::Feature kToolbarNewTabButton{"ToolbarNewTabButton",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kIconForSearchButtonFeature{
    "IconForSearchButtonFeature", base::FEATURE_DISABLED_BY_DEFAULT};

const char kIconForSearchButtonFeatureParameterName[] = "icon";

const char kIconForSearchButtonParameterGrey[] = "grey";
const char kIconForSearchButtonParameterColorful[] = "colorful";
const char kIconForSearchButtonParameterMagnifying[] = "magnifying";

ToolbarSearchButtonIcon IconForSearchButton() {
  if (base::FeatureList::IsEnabled(kIconForSearchButtonFeature)) {
    std::string parameter = base::GetFieldTrialParamValueByFeature(
        kIconForSearchButtonFeature, kIconForSearchButtonFeatureParameterName);
    if (parameter == kIconForSearchButtonParameterGrey) {
      return ToolbarSearchButtonIconGrey;
    } else if (parameter == kIconForSearchButtonParameterColorful) {
      return ToolbarSearchButtonIconColorful;
    } else if (parameter == kIconForSearchButtonParameterMagnifying) {
      return ToolbarSearchButtonIconMagnifying;
    }
  }
  return ToolbarSearchButtonIconMagnifying;
}
