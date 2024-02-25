// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/search_engines/model/search_engines_util.h"

#include "components/country_codes/country_codes.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/template_url_service.h"

namespace search_engines {

void UpdateSearchEngineCountryCodeIfNeeded(PrefService* preferences) {
  if (!preferences->HasPrefPath(country_codes::kCountryIDAtInstall)) {
    // No search engines were ever installed, just return.
    return;
  }
  int old_country_id =
      preferences->GetInteger(country_codes::kCountryIDAtInstall);
  int country_id = country_codes::GetCurrentCountryID();
  if (country_id == old_country_id) {
    // User's locale did not change, just return.
    return;
  }
  // Update the search engine country code. The new country code will be picked
  // up by SearchEngineChoiceService on next startup.
  preferences->SetInteger(country_codes::kCountryIDAtInstall, country_id);
}

bool SupportsSearchByImage(TemplateURLService* service) {
  if (!service) {
    return false;
  }
  const TemplateURL* default_url = service->GetDefaultSearchProvider();
  return default_url && !default_url->image_url().empty() &&
         default_url->image_url_ref().IsValid(service->search_terms_data());
}

bool SupportsSearchImageWithLens(TemplateURLService* service) {
  if (!service) {
    return false;
  }
  const TemplateURL* default_url = service->GetDefaultSearchProvider();
  return default_url &&
         default_url->url_ref().HasGoogleBaseURLs(service->search_terms_data());
}

}  // namespace search_engines
