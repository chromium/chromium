// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_security_utils.h"

#include "base/command_line.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/origin_util.h"
#include "content/public/common/url_constants.h"
#include "net/cookies/site_for_cookies.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace content {
namespace service_worker_security_utils {

bool OriginCanRegisterServiceWorkerFromJavascript(const GURL& url) {
  // WebUI service workers are always registered in C++.
  if (url.SchemeIs(kChromeUIUntrustedScheme) || url.SchemeIs(kChromeUIScheme))
    return false;

  return OriginCanAccessServiceWorkers(url);
}

bool AllOriginsMatchAndCanAccessServiceWorkers(const std::vector<GURL>& urls) {
  // (A) Check if all origins can access service worker. Every URL must be
  // checked despite the same-origin check below in (B), because GetOrigin()
  // uses the inner URL for filesystem URLs so that https://foo/ and
  // filesystem:https://foo/ are considered equal, but filesystem URLs cannot
  // access service worker.
  for (const GURL& url : urls) {
    if (!OriginCanAccessServiceWorkers(url))
      return false;
  }

  // (B) Check if all origins are equal. Cross-origin access is permitted when
  // --disable-web-security is set.
  if (IsWebSecurityDisabled()) {
    return true;
  }
  const GURL& first = urls.front();
  for (const GURL& url : urls) {
    if (first.DeprecatedGetOriginAsURL() != url.DeprecatedGetOriginAsURL())
      return false;
  }
  return true;
}

bool IsWebSecurityDisabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableWebSecurity);
}

void CheckOnUpdateUrls(const GURL& url, const blink::StorageKey& key) {
#if DCHECK_IS_ON()
  const url::Origin origin_to_dcheck = url::Origin::Create(url);
  DCHECK((origin_to_dcheck.opaque() && key.origin().opaque()) ||
         origin_to_dcheck.IsSameOriginWith(key.origin()))
      << origin_to_dcheck << " and " << key.origin() << " should be equal.";
  // TODO(crbug.com/40251360): verify that `top_frame_origin` matches the
  // `top_level_site` of `storage_key`, in most cases.
  //
  // This is currently not the case if:
  //  - The storage key is not for the "real" top-level site, such as when the
  //    top-level site is actually an extension.
  //  - The storage key has a nonce, in which case its `top_level_site` will be
  //    for the frame that introduced the nonce (such as a fenced frame) and not
  //    the same as `top_level_site`.
  //  - The storage key was generated without third-party storage partitioning.
  //    This may be the case even when 3PSP is enabled, due to enterprise policy
  //    or deprecation trials.
  //
  // Consider adding a DHCECK here once the last of those conditions is
  // resolved. See
  // https://chromium-review.googlesource.com/c/chromium/src/+/4378900/4.
#endif
}

blink::StorageKey GetCorrectStorageKeyForWebSecurityState(
    const blink::StorageKey& key,
    const GURL& url) {
  if (IsWebSecurityDisabled()) {
    url::Origin other_origin = url::Origin::Create(url);

    if (key.origin() != other_origin) {
      return blink::StorageKey::CreateFirstParty(other_origin);
    }
  }

  return key;
}

net::SiteForCookies site_for_cookies(const blink::StorageKey& key) {
  // TODO(crbug.com/40737536): Once partitioning is on by default calling
  // ToNetSiteForCookies will be sufficient.
  return key.CopyWithForceEnabledThirdPartyStoragePartitioning()
      .ToNetSiteForCookies();
}

}  // namespace service_worker_security_utils
}  // namespace content
