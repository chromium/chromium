// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_ISOLATED_WEB_APPS_PUBLIC_HEADER_UTILS_H_
#define COMPONENTS_WEBAPPS_ISOLATED_WEB_APPS_PUBLIC_HEADER_UTILS_H_

#include <optional>
#include <string>

#include "base/component_export.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/mojom/parsed_headers.mojom.h"
#include "url/gurl.h"

namespace web_app {

class IwaSourceWithMode;

namespace iwa {

COMPONENT_EXPORT(ISOLATED_WEB_APPS)
const std::string& GetDefaultContentSecurityPolicy();

// Header override is used to allow unsafe connections to ws:// in dev mode
// for HMR.
COMPONENT_EXPORT(ISOLATED_WEB_APPS)
std::optional<std::string> GetContentSecurityPolicyWithWebSocketOverride(
    const std::optional<IwaSourceWithMode>& source);

COMPONENT_EXPORT(ISOLATED_WEB_APPS)
void SetRequiredHeadersForIsolatedApp(const std::optional<IwaSourceWithMode>&,
                                      net::HttpResponseHeaders& headers);

COMPONENT_EXPORT(ISOLATED_WEB_APPS)
void SetRequiredParsedHeadersForIsolatedApp(
    const std::optional<IwaSourceWithMode>&,
    network::mojom::ParsedHeaders& parsed_headers,
    const GURL& url);

}  // namespace iwa
}  // namespace web_app

#endif  // COMPONENTS_WEBAPPS_ISOLATED_WEB_APPS_PUBLIC_HEADER_UTILS_H_
