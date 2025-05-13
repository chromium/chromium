// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LENS_OVERLAY_MODEL_LENS_OVERLAY_URL_UTILS_H_
#define IOS_CHROME_BROWSER_LENS_OVERLAY_MODEL_LENS_OVERLAY_URL_UTILS_H_

#import "ios/web/public/navigation/web_state_policy_decider.h"
#import "url/gurl.h"

namespace lens {

/// Whether the `url` is from a Google domain.
bool IsGoogleHostURL(const GURL& url);

/// Whether the `url` is a lens overlay SRP.
bool IsLensOverlaySRP(const GURL& url);

/// Whether the `url` is a lens multimodal SRP.
bool IsLensMultimodalSRP(const GURL& url);

/// Whether the `url` is a lens AIM SRP.
bool IsLensAIMSRP(const GURL& url);

/// Returns the search tearm of the lens overlay SRP.
std::string ExtractQueryFromLensOverlaySRP(const GURL& url);

/// Whether the `url` is a redirection initiated from Google URL.
bool IsGoogleRedirection(const GURL& url,
                         web::WebStatePolicyDecider::RequestInfo request_info);

/// Processes the `url` so it can be added to history.
GURL ProcessURLForHistory(const GURL& url);

}  // namespace lens

#endif  // IOS_CHROME_BROWSER_LENS_OVERLAY_MODEL_LENS_OVERLAY_URL_UTILS_H_
