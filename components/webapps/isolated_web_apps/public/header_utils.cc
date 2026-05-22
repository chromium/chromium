// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/isolated_web_apps/public/header_utils.h"

#include <optional>
#include <string>
#include <variant>

#include "base/no_destructor.h"
#include "base/strings/stringprintf.h"
#include "components/webapps/isolated_web_apps/types/source.h"
#include "services/network/public/cpp/content_security_policy/content_security_policy.h"
#include "url/origin.h"

namespace web_app {

namespace {

inline constexpr char kIsolatedAppCspTemplate[] =
    "base-uri 'none';"
    "default-src 'self';"
    "object-src 'none';"
    "frame-src 'self' https: blob: data:;"
    "connect-src 'self' https: wss: blob: data:%s;"
    "script-src 'self' 'wasm-unsafe-eval';"
    "img-src 'self' https: blob: data:;"
    "media-src 'self' https: blob: data:;"
    "font-src 'self' blob: data:;"
    "style-src 'self' 'unsafe-inline';"
    "require-trusted-types-for 'script';"
    "frame-ancestors 'self';";

}  // namespace

namespace iwa {

const std::string& GetDefaultContentSecurityPolicy() {
  static const base::NoDestructor<std::string> default_csp(
      [] { return base::StringPrintf(kIsolatedAppCspTemplate, ""); }());
  return *default_csp;
}

std::optional<std::string> GetContentSecurityPolicyWithWebSocketOverride(
    const std::optional<IwaSourceWithMode>& source) {
  if (!source.has_value()) {
    return std::nullopt;
  }

  auto* proxy_source = std::get_if<IwaSourceProxy>(&source->variant());
  if (proxy_source && proxy_source->proxy_url().scheme() == "http") {
    url::Origin origin = proxy_source->proxy_url();
    std::string proxy_ws_url =
        base::StringPrintf(" ws://%s:%i", origin.host().c_str(), origin.port());
    return base::StringPrintf(kIsolatedAppCspTemplate, proxy_ws_url.c_str());
  }

  return std::nullopt;
}

void SetRequiredHeadersForIsolatedApp(
    const std::optional<IwaSourceWithMode>& source,
    net::HttpResponseHeaders& headers) {
  // Apps could specify a more restrictive CSP than what we enforce, which
  // we don't want to overwrite. We add our CSP here so that existing CSPs
  // will still be enforced. Existing CO*P headers are replaced.
  headers.AddHeader(
      "Content-Security-Policy",
      GetContentSecurityPolicyWithWebSocketOverride(source).value_or(
          GetDefaultContentSecurityPolicy()));
  headers.SetHeader("Cross-Origin-Opener-Policy", "same-origin");
  headers.SetHeader("Cross-Origin-Embedder-Policy", "require-corp");
  headers.SetHeader("Cross-Origin-Resource-Policy", "same-origin");
}

void SetRequiredParsedHeadersForIsolatedApp(
    const std::optional<IwaSourceWithMode>& source,
    network::mojom::ParsedHeaders& parsed_headers,
    const GURL& url) {
  parsed_headers.cross_origin_opener_policy.value =
      network::mojom::CrossOriginOpenerPolicyValue::kSameOrigin;
  parsed_headers.cross_origin_embedder_policy.value =
      network::mojom::CrossOriginEmbedderPolicyValue::kRequireCorp;

  // The data is being parsed in browser process, however,
  // the rule of 2 is not broken because the data can be
  // only value from local string kIsolatedAppCspTemplate with
  // single placeholder for 'connect-src' data%s where %s will be
  // evaluated to url to IWA proxy dev server, the url is validated and
  // saved to chrome on installation of IWA.
  std::vector<network::mojom::ContentSecurityPolicyPtr> parsed_csp =
      network::ParseContentSecurityPolicies(
          GetContentSecurityPolicyWithWebSocketOverride(source).value_or(
              GetDefaultContentSecurityPolicy()),
          network::mojom::ContentSecurityPolicyType::kEnforce,
          // Otherwise with kMeta frame-ancestors is ignored.
          network::mojom::ContentSecurityPolicySource::kHTTP,
          url::Origin::Create(url).GetURL());

  CHECK(parsed_csp.size() == 1);
  parsed_headers.content_security_policy.push_back(std::move(parsed_csp[0]));
}

}  // namespace iwa
}  // namespace web_app
