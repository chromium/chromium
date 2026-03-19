// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_BROWSER_WEB_APP_URL_CONFIG_H_
#define COMPONENTS_WEBAPPS_BROWSER_WEB_APP_URL_CONFIG_H_

#include <string>

#include "base/functional/callback_helpers.h"

class GURL;

namespace webapps {

// Returns true if `url` is eligible for web app features (installation,
// banners, etc.). Returns false for:
// - Invalid URLs (including empty URLs and those with bad encoding)
// - URLs with inner URLs (e.g., blob: or filesystem:)
// - about:blank
// - WebUI URLs (chrome://, chrome-untrusted://, devtools://) except for
//   chrome://password-manager and test-registered hosts
// Returns true for:
// - HTTP and HTTPS URLs
// - chrome-extension:// URLs (on platforms where extension apps are allowed)
// - chrome://password-manager
bool IsUrlEligibleForWebApp(const GURL& url);

// Adds chrome://`host` as an origin that IsUrlEligibleForWebApp will consider
// valid. The returned ScopedClosureRunner undoes this registration.
base::ScopedClosureRunner AddValidChromeUrlHostForTesting(
    const std::string& host);

}  // namespace webapps

#endif  // COMPONENTS_WEBAPPS_BROWSER_WEB_APP_URL_CONFIG_H_
