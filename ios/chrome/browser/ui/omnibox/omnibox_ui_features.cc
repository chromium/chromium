// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/omnibox_ui_features.h"

#include "base/metrics/field_trial_params.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"

const base::Feature kEnableSuggestionsScrollingOnIPad{
    "EnableSuggestionsScrollingOnIPad", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kOmniboxPasteButton{"OmniboxPasteButton",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

const char kOmniboxPasteButtonParameterName[] = "PasteButtonVariant";
const char kOmniboxPasteButtonParameterBlueIconCapsule[] = "SuggestionIcon";
const char kOmniboxPasteButtonParameterBlueFullCapsule[] = "SuggestionTextIcon";

const base::Feature kOmniboxKeyboardPasteButton{
    "OmniboxKeyboardPasteButton", base::FEATURE_ENABLED_BY_DEFAULT};

bool IsOmniboxActionsEnabled() {
  return base::FeatureList::IsEnabled(kIOSOmniboxUpdatedPopupUI);
}

bool IsOmniboxActionsVisualTreatment1() {
  if (!IsOmniboxActionsEnabled()) {
    return false;
  }
  auto param = base::GetFieldTrialParamValueByFeature(
      kIOSOmniboxUpdatedPopupUI, kIOSOmniboxUpdatedPopupUIVariationName);
  return param == kIOSOmniboxUpdatedPopupUIVariation1 ||
         param == kIOSOmniboxUpdatedPopupUIVariation1UIKit;
}

bool IsOmniboxActionsVisualTreatment2() {
  if (!IsOmniboxActionsEnabled()) {
    return false;
  }
  return !IsOmniboxActionsVisualTreatment1();
}

bool IsSwiftUIPopupEnabled() {
  if (!IsOmniboxActionsEnabled()) {
    return false;
  }
  auto param = base::GetFieldTrialParamValueByFeature(
      kIOSOmniboxUpdatedPopupUI, kIOSOmniboxUpdatedPopupUIVariationName);
  return param == kIOSOmniboxUpdatedPopupUIVariation1 ||
         param == kIOSOmniboxUpdatedPopupUIVariation2;
}
