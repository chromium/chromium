// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/omnibox_position/omnibox_position_choice_util.h"

#import "components/prefs/pref_service.h"
#import "components/search_engines/search_engine_choice/search_engine_choice_service.h"
#import "components/search_engines/search_engine_choice/search_engine_choice_utils.h"
#import "ios/chrome/browser/search_engines/model/search_engine_choice_service_factory.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/utils/first_run_util.h"
#import "ios/chrome/browser/shared/public/features/features.h"

namespace {

/// The time delta for a user to be considered as a new user.
constexpr base::TimeDelta kNewUserTimeDelta = base::Days(7);

/// Whether the promos should be shown in the current region. The promo is not
/// shown where the SearchEngineChoiceScreen can be shown.
bool ShouldShowPromoInCurrentRegion(ChromeBrowserState* browser_state) {
  if (!base::FeatureList::IsEnabled(kBottomOmniboxPromoRegionFilter)) {
    return true;
  }

  search_engines::SearchEngineChoiceService* search_engine_choice_service =
      ios::SearchEngineChoiceServiceFactory::GetForBrowserState(browser_state);

  if (!search_engine_choice_service) {
    return false;
  }

  BOOL isChoiceCountry = search_engines::IsEeaChoiceCountry(
      search_engine_choice_service->GetCountryId());
  return !isChoiceCountry;
}

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

bool ShouldShowOmniboxPositionChoiceIPHPromo(
    ChromeBrowserState* browser_state) {
  CHECK(IsBottomOmniboxPromoFlagEnabled(BottomOmniboxPromoType::kAppLaunch));

  std::string feature_param = base::GetFieldTrialParamValueByFeature(
      kBottomOmniboxPromoAppLaunch, kBottomOmniboxPromoParam);
  if (feature_param == kBottomOmniboxPromoParamForced) {
    return true;
  }

  if (!ShouldShowPromoInCurrentRegion(browser_state)) {
    return false;
  }

  // Don't show the promo to new users.
  if (IsFirstRunRecent(kNewUserTimeDelta)) {
    return false;
  }

  PrefService* pref_service = browser_state->GetPrefs();
  return !pref_service->GetUserPrefValue(prefs::kBottomOmnibox);
}

bool ShouldShowOmniboxPositionChoiceInFRE(ChromeBrowserState* browser_state) {
  CHECK(IsBottomOmniboxPromoFlagEnabled(BottomOmniboxPromoType::kFRE));

  std::string feature_param = base::GetFieldTrialParamValueByFeature(
      kBottomOmniboxPromoFRE, kBottomOmniboxPromoParam);
  if (feature_param == kBottomOmniboxPromoParamForced) {
    return true;
  }

  return ShouldShowPromoInCurrentRegion(browser_state);
}
