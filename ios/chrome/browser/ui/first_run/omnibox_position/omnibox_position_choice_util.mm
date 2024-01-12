// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/omnibox_position/omnibox_position_choice_util.h"

#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/utils/first_run_util.h"
#import "ios/chrome/browser/shared/public/features/features.h"

namespace {

/// The time delta for a user to be considered as a new user.
constexpr base::TimeDelta kNewUserTimeDelta = base::Days(7);

}  // namespace

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

  // Don't show the promo to new users.
  if (IsFirstRunRecent(kNewUserTimeDelta)) {
    return false;
  }

  return !pref_service->GetUserPrefValue(prefs::kBottomOmnibox);
}
