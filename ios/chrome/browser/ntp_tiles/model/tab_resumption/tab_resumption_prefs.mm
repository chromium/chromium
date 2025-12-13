// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp_tiles/model/tab_resumption/tab_resumption_prefs.h"

#import "base/metrics/histogram_macros.h"
#import "components/ntp_tiles/pref_names.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/content_suggestions_constants.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/content_suggestions_metrics_constants.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "url/gurl.h"

namespace tab_resumption_prefs {

// The maximum length of the last distant tab URL.
size_t kMaxLengthTabURL = 2 * 1024;

const char kTabResumptionDisabledPref[] = "tab_resumption.disabled";
const char kTabResumptionLastOpenedTabURLPref[] =
    "tab_resumption.last_opened_tab_url";
const char kTabResumptionRegularUrlImpressions[] =
    "tab_resumption.regular.url_impressions";
const char kTabResumptionWithPriceDropUrlImpressions[] =
    "tab_resumption.price_drop.url_impressions";
const char kTabResumptionWithPriceTrackableUrlImpressions[] =
    "tab_resumption.price_trackable.url_impressions";

void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  // TODO(crbug.com/395840133): Remove `kTabResumptionDisabledPref` registration
  // from local-state Prefs after successfully migrating to profile Prefs.
  registry->RegisterBooleanPref(kTabResumptionDisabledPref, false);
}

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterStringPref(kTabResumptionLastOpenedTabURLPref,
                               std::string());
  // Added 02/2025
  registry->RegisterBooleanPref(kTabResumptionDisabledPref, false);
  registry->RegisterDictionaryPref(kTabResumptionRegularUrlImpressions);
  registry->RegisterDictionaryPref(kTabResumptionWithPriceDropUrlImpressions);
  registry->RegisterDictionaryPref(
      kTabResumptionWithPriceTrackableUrlImpressions);
}

bool IsTabResumptionDisabled(PrefService* prefs) {
  return !prefs->GetBoolean(ntp_tiles::prefs::kTabResumptionHomeModuleEnabled);
}

void DisableTabResumption(PrefService* prefs) {
  UMA_HISTOGRAM_ENUMERATION(kMagicStackModuleDisabledHistogram,
                            ContentSuggestionsModuleType::kTabResumption);
  prefs->SetBoolean(ntp_tiles::prefs::kTabResumptionHomeModuleEnabled, false);
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
