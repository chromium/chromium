// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/omnibox_ui_features.h"

#include "base/metrics/field_trial_params.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ui/base/device_form_factor.h"

BASE_FEATURE(kEnableSuggestionsScrollingOnIPad,
             "EnableSuggestionsScrollingOnIPad",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnablePopoutOmniboxIpad,
             "EnablePopoutOmniboxIpad",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kOmniboxPasteButton,
             "OmniboxPasteButton",
             base::FEATURE_DISABLED_BY_DEFAULT);

const char kOmniboxPasteButtonParameterName[] = "PasteButtonVariant";
const char kOmniboxPasteButtonParameterBlueIconCapsule[] = "SuggestionIcon";
const char kOmniboxPasteButtonParameterBlueFullCapsule[] = "SuggestionTextIcon";

BASE_FEATURE(kOmniboxKeyboardPasteButton,
             "OmniboxKeyboardPasteButton",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kOmniboxCarouselDynamicSpacing,
             "OmniboxCarouselDynamicSpacing",
             base::FEATURE_ENABLED_BY_DEFAULT);

bool IsOmniboxActionsEnabled() {
  return base::FeatureList::IsEnabled(kIOSOmniboxUpdatedPopupUI);
}

bool IsOmniboxActionsVisualTreatment1() {
  return base::FeatureList::IsEnabled(kIOSOmniboxUpdatedPopupUI);
}

bool IsOmniboxActionsVisualTreatment2() {
  return false;
}

bool IsSwiftUIPopupEnabled() {
  return false;
}

bool IsIpadPopoutOmniboxEnabled() {
  return base::FeatureList::IsEnabled(kEnablePopoutOmniboxIpad) &&
         ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET;
}
