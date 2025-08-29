// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tpcd_heuristics/opener_heuristic_utils.h"

#include "content/browser/btm/btm_bounce_detector.h"
#include "content/public/browser/cookie_access_details.h"
#include "services/network/public/cpp/features.h"
#include "url/gurl.h"

namespace content {

PopupProvider GetPopupProvider(const GURL& popup_url) {
  if (popup_url.DomainIs("google.com")) {
    return PopupProvider::kGoogle;
  }
  return PopupProvider::kUnknown;
}

OptionalBool IsAdTaggedCookieForHeuristics(const CookieAccessDetails& details) {
  if (!base::FeatureList::IsEnabled(
          network::features::kSkipTpcdMitigationsForAds) ||
      !network::features::kSkipTpcdMitigationsForAdsHeuristics.Get()) {
    return OptionalBool::kUnknown;
  }
  return ToOptionalBool(details.cookie_setting_overrides.Has(
      net::CookieSettingOverride::kSkipTPCDHeuristicsGrant));
}

std::map<std::string, std::pair<GURL, bool>> GetRedirectHeuristicURLs(
    const BtmRedirectContext& committed_redirect_context,
    const GURL& first_party_url,
    base::optional_ref<std::set<std::string>> allowed_sites,
    bool require_current_interaction) {
  std::map<std::string, std::pair<GURL, bool>>
      sites_to_url_and_current_interaction;

  std::set<std::string> sites_with_current_interaction =
      committed_redirect_context.AllSitesWithUserActivationOrAuthn();

  const std::string& first_party_site = GetSiteForBtm(first_party_url);
  for (size_t redirect_index = 0;
       redirect_index < committed_redirect_context.size(); redirect_index++) {
    const BtmRedirectInfo& redirect =
        committed_redirect_context[redirect_index];
    const GURL& url = redirect.redirector_url;
    const std::string& site = redirect.site;

    // The redirect heuristic does not apply for first-party cookie access.
    if (site == first_party_site) {
      continue;
    }

    // Check the list of allowed sites, if provided.
    if (allowed_sites.has_value() && !allowed_sites->contains(site)) {
      continue;
    }

    // Check for a current interaction, if the flag requires it.
    if (require_current_interaction &&
        !sites_with_current_interaction.contains(site)) {
      continue;
    }

    // Add the url to the map, but do not override a previous current
    // interaction.
    auto& [prev_url, had_current_interaction] =
        sites_to_url_and_current_interaction[site];
    if (prev_url.is_empty() || !had_current_interaction) {
      prev_url = url;
      had_current_interaction = sites_with_current_interaction.contains(site);
    }
  }

  return sites_to_url_and_current_interaction;
}

}  // namespace content
