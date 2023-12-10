// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/omnibox_position/omnibox_position_choice_util.h"

#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"

ToolbarType DefaultSelectedOmniboxPosition() {
  CHECK(IsBottomOmniboxPromoFlagEnabled(BottomOmniboxPromoType::kAny));
  std::string featureParam = base::GetFieldTrialParamValueByFeature(
      kBottomOmniboxPromoDefaultPosition,
      kBottomOmniboxPromoDefaultPositionParam);
  if (featureParam == kBottomOmniboxPromoDefaultPositionParamTop) {
    return ToolbarType::kPrimary;
  } else if (featureParam == kBottomOmniboxPromoDefaultPositionParamBottom) {
    return ToolbarType::kSecondary;
  }
  return ToolbarType::kPrimary;
}

bool ShouldShowOmniboxPositionChoiceIPHPromo(PrefService* pref_service) {
  CHECK(IsBottomOmniboxPromoFlagEnabled(BottomOmniboxPromoType::kAppLaunch));

  std::string feature_param = base::GetFieldTrialParamValueByFeature(
      kBottomOmniboxPromoAppLaunch, kBottomOmniboxPromoParam);
  if (feature_param == kBottomOmniboxPromoParamForced) {
    return true;
  }

  return !pref_service->GetUserPrefValue(prefs::kBottomOmnibox);
}
