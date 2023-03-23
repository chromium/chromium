// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PRERENDER_PRERENDER_NAVIGATION_UTILS_H_
#define CONTENT_BROWSER_PRELOADING_PRERENDER_PRERENDER_NAVIGATION_UTILS_H_

#include "url/gurl.h"
#include "url/origin.h"

namespace content::prerender_navigation_utils {

// Returns true if the response code is disallowed for pre-rendering (e.g 404,
// etc), and false otherwise. This should be called only for the response of the
// main frame in a prerendered page.
bool IsDisallowedHttpResponseCode(int response_code);

// Returns true if `target_url` is in the same site as `origin`.
bool IsSameSite(const GURL& target_url, const url::Origin& origin);

// Returns true if `target_url` is in the same site cross origin as `origin`.
bool IsSameSiteCrossOrigin(const GURL& target_url, const url::Origin& origin);

// Returns true if `target_url` is not in the same site as `origin`.
bool IsCrossSite(const GURL& target_url, const url::Origin& origin);

}  // namespace content::prerender_navigation_utils

#endif  // CONTENT_BROWSER_PRELOADING_PRERENDER_PRERENDER_NAVIGATION_UTILS_H_
