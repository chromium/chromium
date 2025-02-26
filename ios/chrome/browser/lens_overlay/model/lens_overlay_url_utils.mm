// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/model/lens_overlay_url_utils.h"

#import "components/google/core/common/google_util.h"
#import "components/lens/lens_url_utils.h"
#import "net/base/url_util.h"

namespace lens {

bool IsGoogleHostURL(GURL url) {
  return google_util::IsGoogleDomainUrl(
      url, google_util::DISALLOW_SUBDOMAIN,
      google_util::DISALLOW_NON_STANDARD_PORTS);
}

bool IsLensOverlaySRP(GURL url) {
  std::string search_term;
  bool hasSearchTerms = net::GetValueForKeyInQuery(url, "q", &search_term);
  std::string lens_surface;
  bool hasLensSurface = net::GetValueForKeyInQuery(
      url, lens::kLensSurfaceQueryParameter, &lens_surface);
  std::string request_id;
  bool hasLensParam = net::GetValueForKeyInQuery(
      url, lens::kLensRequestQueryParameter, &request_id);

  return hasSearchTerms && hasLensSurface && !hasLensParam &&
         lens_surface == "4";
}

bool IsLensMultimodalSRP(GURL url) {
  std::string search_term;
  bool has_search_terms = net::GetValueForKeyInQuery(url, "q", &search_term);
  std::string lens_surface;
  bool has_lens_surface = net::GetValueForKeyInQuery(
      url, lens::kLensSurfaceQueryParameter, &lens_surface);
  std::string request_id;
  bool has_lens_param = net::GetValueForKeyInQuery(
      url, lens::kLensRequestQueryParameter, &request_id);
  std::string udm;
  bool has_unified_drilldown_param = net::GetValueForKeyInQuery(
      url, lens::kUnifiedDrillDownQueryParameter, &udm);

  return has_search_terms && has_lens_surface && has_lens_param &&
         has_unified_drilldown_param && lens_surface == "4" && udm == "24";
}

std::string ExtractQueryFromLensOverlaySRP(GURL url) {
  std::string search_term = "";
  net::GetValueForKeyInQuery(url, "q", &search_term);
  return search_term;
}

bool IsGoogleRedirection(GURL url,
                         web::WebStatePolicyDecider::RequestInfo request_info) {
  return IsGoogleHostURL(url) &&
         (request_info.transition_type & ui::PAGE_TRANSITION_CLIENT_REDIRECT);
}

}  // namespace lens
