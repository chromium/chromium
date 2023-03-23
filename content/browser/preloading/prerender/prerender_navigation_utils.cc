// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prerender/prerender_navigation_utils.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"

namespace content::prerender_navigation_utils {

// https://wicg.github.io/nav-speculation/prerendering.html#no-bad-navs
// > If browsingContext is a top level prerendering browsing context, and any of
// the following hold:
bool IsDisallowedHttpResponseCode(int response_code) {
  // > - response’s status is 204 or 205,
  if (response_code == 204 || response_code == 205) {
    return true;
  }
  return response_code < 100 || response_code > 399;
}

bool IsSameSite(const GURL& target_url, const url::Origin& origin) {
  return target_url.scheme() == origin.scheme() &&
         net::registry_controlled_domains::SameDomainOrHost(
             target_url, origin,
             net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
}

bool IsSameSiteCrossOrigin(const GURL& target_url, const url::Origin& origin) {
  if (!IsSameSite(target_url, origin)) {
    return false;
  }
  if (origin.IsSameOriginWith(target_url)) {
    return false;
  }
  return true;
}

bool IsCrossSite(const GURL& target_url, const url::Origin& origin) {
  return !IsSameSite(target_url, origin);
}

}  // namespace content::prerender_navigation_utils
