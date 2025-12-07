// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/search_engines/model/search_engines_util.h"

#include "base/feature_list.h"
#include "components/country_codes/country_codes.h"
#include "components/prefs/pref_service.h"
#include "components/regional_capabilities/regional_capabilities_prefs.h"
#include "components/regional_capabilities/regional_capabilities_switches.h"
#include "components/search_engines/template_url_service.h"

namespace search_engines {

void UpdateSearchEngineCountryCodeIfNeeded(PrefService* preferences) {
  // Do not update `kCountryIDAtInstall` preference when
  // `kDynamicProfileCountry` is on to avoid situation when multiple codepaths
  // are affecting regional capabilities service results by updating prefs.
  if (base::FeatureList::IsEnabled(switches::kDynamicProfileCountry)) {
    return;
  }

  if (!preferences->HasPrefPath(
          regional_capabilities::prefs::kCountryIDAtInstall)) {
    // No search engines were ever installed, just return.
    return;
  }
  int old_country_id = preferences->GetInteger(
      regional_capabilities::prefs::kCountryIDAtInstall);
  int country_id = country_codes::GetCurrentCountryID().Serialize();
  if (country_id == old_country_id) {
    // User's locale did not change, just return.
    return;
  }
  // Update the search engine country code. The new country code will be picked
  // up by SearchEngineChoiceService on next startup.
  preferences->SetInteger(regional_capabilities::prefs::kCountryIDAtInstall,
                          country_id);
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
