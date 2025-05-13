// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/model/lens_overlay_url_utils.h"

#import <optional>

#import "components/google/core/common/google_util.h"
#import "components/lens/lens_url_utils.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "net/base/url_util.h"

namespace lens {

bool IsGoogleHostURL(const GURL& url) {
  // Only available for debug builds.
  if (experimental_flags::GetLensResultPanelGwsURL() != nil) {
    return google_util::IsGoogleDomainUrl(
        url, google_util::ALLOW_SUBDOMAIN,
        google_util::ALLOW_NON_STANDARD_PORTS);
  }

  return google_util::IsGoogleDomainUrl(
      url, google_util::DISALLOW_SUBDOMAIN,
      google_util::DISALLOW_NON_STANDARD_PORTS);
}

bool IsLensOverlaySRP(const GURL& url) {
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

bool IsLensMultimodalSRP(const GURL& url) {
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

bool IsLensAIMSRP(const GURL& url) {
  if (!IsGoogleHostURL(url)) {
    return false;
  }
  std::string udm;
  bool has_unified_drilldown_param = net::GetValueForKeyInQuery(
      url, lens::kUnifiedDrillDownQueryParameter, &udm);
  return has_unified_drilldown_param && udm == "50";
}

std::string ExtractQueryFromLensOverlaySRP(const GURL& url) {
  std::string search_term = "";
  net::GetValueForKeyInQuery(url, "q", &search_term);
  return search_term;
}

bool IsGoogleRedirection(const GURL& url,
                         web::WebStatePolicyDecider::RequestInfo request_info) {
  return IsGoogleHostURL(url) &&
         (request_info.transition_type & ui::PAGE_TRANSITION_CLIENT_REDIRECT);
}

GURL ProcessURLForHistory(const GURL& url) {
  if (IsGoogleHostURL(url)) {
    std::string lens_surface;
    bool has_lens_surface = net::GetValueForKeyInQuery(
        url, lens::kLensSurfaceQueryParameter, &lens_surface);
    if (has_lens_surface) {
      return net::AppendOrReplaceQueryParameter(
          url, lens::kLensSurfaceQueryParameter, std::nullopt);
    }
  }
  return url;
}

}  // namespace lens
