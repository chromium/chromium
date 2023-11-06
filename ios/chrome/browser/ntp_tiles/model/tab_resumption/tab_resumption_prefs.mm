// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp_tiles/model/tab_resumption/tab_resumption_prefs.h"

#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/pref_service.h"
#import "url/gurl.h"

namespace tab_resumption_prefs {

// The maximum length of the last distant tab URL.
size_t kMaxLengthTabURL = 2 * 1024;

const char kTabResumptioDisabledPref[] = "tab_resumption.disabled";
const char kTabResumptionLastOpenedTabURLPref[] =
    "tab_resumption.last_opened_tab_url";

void RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kTabResumptioDisabledPref, false);
  registry->RegisterStringPref(kTabResumptionLastOpenedTabURLPref,
                               std::string());
}

bool IsTabResumptionDisabled(PrefService* prefs) {
  return prefs->GetBoolean(kTabResumptioDisabledPref);
}

void DisableTabResumption(PrefService* prefs) {
  prefs->SetBoolean(kTabResumptioDisabledPref, true);
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
