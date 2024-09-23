// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_SECURITY_UTILS_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_SECURITY_UTILS_H_

#include <vector>

#include "content/common/content_export.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/gurl.h"

namespace blink {
class StorageKey;
}  // namespace blink

namespace content {
namespace service_worker_security_utils {

// Returns true if |url| can register service workers from Javascript. This
// includes checking if |url| can access Service Workers.
CONTENT_EXPORT bool OriginCanRegisterServiceWorkerFromJavascript(
    const GURL& url);

// Returns true if all members of |urls| have the same origin, and
// OriginCanAccessServiceWorkers is true for this origin.
// If --disable-web-security is enabled, the same origin check is
// not performed.
CONTENT_EXPORT bool AllOriginsMatchAndCanAccessServiceWorkers(
    const std::vector<GURL>& urls);

CONTENT_EXPORT bool IsWebSecurityDisabled();

// Check origins etc. on setting URLs.
CONTENT_EXPORT void CheckOnUpdateUrls(const GURL& url,
                                      const blink::StorageKey& key);

// This function returns the correct StorageKey depending on the state of the
// "disable-web-security" flag.
//
// If web security is disabled then it's possible for the `url` to be
// cross-origin from `this`'s origin. In that case we need to make a new key
// with the `url`'s origin, otherwise we might access the wrong storage
// partition.
CONTENT_EXPORT blink::StorageKey GetCorrectStorageKeyForWebSecurityState(
    const blink::StorageKey& key,
    const GURL& url);

// This returns the first party for cookies as derived from the storage key.
// For information on how this may differ from the SiteForCookies in the frame
// context please see the comments above StorageKey::ToNetSiteForCookies.
// For service worker execution contexts, site_for_cookies() always
// corresponds to the service worker script URL.
CONTENT_EXPORT net::SiteForCookies site_for_cookies(
    const blink::StorageKey& key);

}  // namespace service_worker_security_utils
}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_SECURITY_UTILS_H_
