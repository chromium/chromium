// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/model/lens_overlay_url_utils.h"

#import "components/lens/lens_url_utils.h"
#import "net/base/url_util.h"

namespace lens {

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

std::string ExtractQueryFromLensOverlaySRP(GURL url) {
  std::string search_term = "";
  net::GetValueForKeyInQuery(url, "q", &search_term);
  return search_term;
}

}  // namespace lens
