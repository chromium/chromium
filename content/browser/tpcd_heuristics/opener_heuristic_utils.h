// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_TPCD_HEURISTICS_OPENER_HEURISTIC_UTILS_H_
#define CONTENT_BROWSER_TPCD_HEURISTICS_OPENER_HEURISTIC_UTILS_H_

#include <map>
#include <set>
#include <string>
#include <utility>

#include "base/types/optional_ref.h"
#include "content/common/content_export.h"

class GURL;

namespace content {

class BtmRedirectContext;
struct CookieAccessDetails;

// Common identity providers that open pop-ups, to help estimate the impact of
// third-party cookie blocking and prioritize mitigations. These values are
// emitted in metrics and should not be renumbered.
enum class PopupProvider {
  kUnknown = 0,
  kGoogle = 1,
};

CONTENT_EXPORT PopupProvider GetPopupProvider(const GURL& popup_url);

// These values are emitted in metrics and should not be renumbered. This one
// type is used for both of the IsAdTagged and HasSameSiteIframe UKM enums.
enum class OptionalBool {
  kUnknown = 0,
  kFalse = 1,
  kTrue = 2,
};

inline OptionalBool ToOptionalBool(bool b) {
  return b ? OptionalBool::kTrue : OptionalBool::kFalse;
}

// Returns whether the provided cookie access was ad-tagged, based on the cookie
// settings overrides. Returns Unknown if kSkipTpcdMitigationsForAdsHeuristics
// is false and the override is not set regardless.
CONTENT_EXPORT OptionalBool
IsAdTaggedCookieForHeuristics(const CookieAccessDetails& details);

// Returns a map of (site, (url, has_current_interaction)) for all URLs in the
// current redirect chain that satisfy the redirect heuristic. This performs
// all checks except for the presence of a past interaction, which should be
// checked by the caller using the BTM database. If `allowed_sites` is present,
// only sites in `allowed_sites` should be included.
CONTENT_EXPORT std::map<std::string, std::pair<GURL, bool>>
GetRedirectHeuristicURLs(
    const BtmRedirectContext& committed_redirect_context,
    const GURL& first_party_url,
    base::optional_ref<std::set<std::string>> allowed_sites,
    bool require_current_interaction);

}  // namespace content

#endif  // CONTENT_BROWSER_TPCD_HEURISTICS_OPENER_HEURISTIC_UTILS_H_
