// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/isolated_web_apps/url_loading/header_utils.h"

#include <optional>
#include <string>
#include <string_view>
#include <variant>

#include "base/strings/strcat.h"
#include "base/strings/to_string.h"
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
    "connect-src 'self' https: wss: blob: data:"
    // WebSocket URL override injected here
    ";"
    "script-src 'self' 'wasm-unsafe-eval';"
    "img-src 'self' https: blob: data:;"
    "media-src 'self' https: blob: data:;"
    "font-src 'self' blob: data:;"
    "style-src 'self' 'unsafe-inline';"
    "require-trusted-types-for 'script';"
    "frame-ancestors 'self';";

constexpr std::string_view kConnectSrcDirective =
    "connect-src 'self' https: wss: blob: data:";
constexpr size_t kConnectSrcPos =
    std::string_view(kIsolatedAppCspTemplate).find(kConnectSrcDirective);
static_assert(kConnectSrcPos != std::string_view::npos);
constexpr size_t kConnectSrcEnd = kConnectSrcPos + kConnectSrcDirective.size();

// Split the CSP template into two parts, with the prefix before the format
// string to be inserted and the suffix after the format string.
constexpr std::string_view kIsolatedAppCspPrefix =
    std::string_view(kIsolatedAppCspTemplate).substr(0, kConnectSrcEnd);
constexpr std::string_view kIsolatedAppCspSuffix =
    std::string_view(kIsolatedAppCspTemplate).substr(kConnectSrcEnd);

}  // namespace

namespace iwa {

std::string_view GetDefaultContentSecurityPolicy() {
  return kIsolatedAppCspTemplate;
}

std::optional<std::string> GetContentSecurityPolicyWithWebSocketOverride(
    const std::optional<IwaSourceWithMode>& source) {
  if (!source.has_value()) {
    return std::nullopt;
  }

  auto* proxy_source = std::get_if<IwaSourceProxy>(&source->variant());
  if (proxy_source && proxy_source->proxy_url().scheme() == "http") {
    url::Origin origin = proxy_source->proxy_url();
    return base::StrCat({kIsolatedAppCspPrefix, " ws://", origin.host(), ":",
                         base::ToString(origin.port()), kIsolatedAppCspSuffix});
  }

  return std::nullopt;
}

void SetRequiredHeadersForIsolatedApp(
    const std::optional<IwaSourceWithMode>& source,
    net::HttpResponseHeaders& headers) {
  // Apps could specify a more restrictive CSP than what we enforce, which
  // we don't want to overwrite. We add our CSP here so that existing CSPs
  // will still be enforced. Existing CO*P headers are replaced.
  std::optional<std::string> csp_override =
      GetContentSecurityPolicyWithWebSocketOverride(source);
  headers.AddHeader("Content-Security-Policy",
                    csp_override.has_value()
                        ? std::string_view(*csp_override)
                        : GetDefaultContentSecurityPolicy());
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

  // The data is being parsed in the browser process, however, the Rule of 2 is
  // not broken because the value comes solely from the local constant
  // `kIsolatedAppCspTemplate`, optionally with a WebSocket URL override
  // injected into 'connect-src' for the IWA proxy dev server (whose URL is
  // validated and saved to Chrome upon IWA installation).
  std::optional<std::string> csp_override =
      GetContentSecurityPolicyWithWebSocketOverride(source);
  std::vector<network::mojom::ContentSecurityPolicyPtr> parsed_csp =
      network::ParseContentSecurityPolicies(
          csp_override.has_value() ? std::string_view(*csp_override)
                                   : GetDefaultContentSecurityPolicy(),
          network::mojom::ContentSecurityPolicyType::kEnforce,
          // Otherwise with kMeta frame-ancestors is ignored.
          network::mojom::ContentSecurityPolicySource::kHTTP,
          url::Origin::Create(url).GetURL());

  CHECK(parsed_csp.size() == 1);
  parsed_headers.content_security_policy.push_back(std::move(parsed_csp[0]));
}

}  // namespace iwa
}  // namespace web_app
