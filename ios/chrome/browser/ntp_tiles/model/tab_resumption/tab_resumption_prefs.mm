// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp_tiles/model/tab_resumption/tab_resumption_prefs.h"

#import "base/metrics/histogram_macros.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_metrics_constants.h"
#import "url/gurl.h"

namespace tab_resumption_prefs {

// The maximum length of the last distant tab URL.
size_t kMaxLengthTabURL = 2 * 1024;

const char kTabResumptioDisabledPref[] = "tab_resumption.disabled";
const char kTabResumptionLastOpenedTabURLPref[] =
    "tab_resumption.last_opened_tab_url";

void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kTabResumptioDisabledPref, false);
}

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterStringPref(kTabResumptionLastOpenedTabURLPref,
                               std::string());
}

bool IsTabResumptionDisabled(PrefService* prefs) {
  if (IsHomeCustomizationEnabled()) {
    return !prefs->GetBoolean(
        prefs::kHomeCustomizationMagicStackTabResumptionEnabled);
  }
  return prefs->GetBoolean(kTabResumptioDisabledPref);
}

void DisableTabResumption(PrefService* prefs) {
  UMA_HISTOGRAM_ENUMERATION(kMagicStackModuleDisabledHistogram,
                            ContentSuggestionsModuleType::kTabResumption);
  if (IsHomeCustomizationEnabled()) {
    prefs->SetBoolean(prefs::kHomeCustomizationMagicStackTabResumptionEnabled,
                      false);
  } else {
    prefs->SetBoolean(kTabResumptioDisabledPref, true);
  }
}

bool IsLastOpenedURL(GURL URL, PrefService* prefs) {
  std::string url_without_ref = URL.GetWithoutRef().spec();
  if (url_without_ref.length() > kMaxLengthTabURL) {
    url_without_ref = url_without_ref.substr(0, kMaxLengthTabURL);
  }

  return prefs->GetString(kTabResumptionLastOpenedTabURLPref) ==
         url_without_ref;
}

void SetTabResumptionLastOpenedTabURL(GURL URL, PrefService* prefs) {
  std::string url_without_ref = URL.GetWithoutRef().spec();
  if (url_without_ref.length() > kMaxLengthTabURL) {
    url_without_ref = url_without_ref.substr(0, kMaxLengthTabURL);
  }

  prefs->SetString(kTabResumptionLastOpenedTabURLPref, url_without_ref);
}

}  // namespace tab_resumption_prefs
