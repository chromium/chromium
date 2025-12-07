// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/request_header_to_enum.h"

#include "base/containers/fixed_flat_map.h"
#include "base/metrics/histogram_functions.h"

namespace network {

namespace {

constexpr auto kHeaderMap =
    base::MakeFixedFlatMap<std::string_view, RequestHeader>({
        {"accept", RequestHeader::kAccept},
        {"accept-encoding", RequestHeader::kAcceptEncoding},
        {"accept-language", RequestHeader::kAcceptLanguage},
        {"authorization", RequestHeader::kAuthorization},
        {"cache-control", RequestHeader::kCacheControl},
        {"client-version", RequestHeader::kClientVersion},
        {"content-encoding", RequestHeader::kContentEncoding},
        {"content-type", RequestHeader::kContentType},
        {"device-memory", RequestHeader::kDeviceMemory},
        {"downlink", RequestHeader::kDownlink},
        {"dpr", RequestHeader::kDpr},
        {"ect", RequestHeader::kEct},
        {"google-translate-element-mode",
         RequestHeader::kGoogleTranslateElementMode},
        {"if-modified-since", RequestHeader::kIfModifiedSince},
        {"if-none-match", RequestHeader::kIfNoneMatch},
        {"origin", RequestHeader::kOrigin},
        {"ping-from", RequestHeader::kPingFrom},
        {"ping-to", RequestHeader::kPingTo},
        {"pragma", RequestHeader::kPragma},
        {"range", RequestHeader::kRange},
        {"request-id", RequestHeader::kRequestId},
        {"rtt", RequestHeader::kRtt},
        {"sec-browsing-topics", RequestHeader::kSecBrowsingTopics},
        {"sec-ch-device-memory", RequestHeader::kSecChDeviceMemory},
        {"sec-ch-dpr", RequestHeader::kSecChDpr},
        {"sec-ch-prefers-color-scheme",
         RequestHeader::kSecChPrefersColorScheme},
        {"sec-ch-ua", RequestHeader::kSecChUa},
        {"sec-ch-ua-arch", RequestHeader::kSecChUaArch},
        {"sec-ch-ua-bitness", RequestHeader::kSecChUaBitness},
        {"sec-ch-ua-form-factors", RequestHeader::kSecChUaFormFactors},
        {"sec-ch-ua-full-version", RequestHeader::kSecChUaFullVersion},
        {"sec-ch-ua-full-version-list", RequestHeader::kSecChUaFullVersionList},
        {"sec-ch-ua-mobile", RequestHeader::kSecChUaMobile},
        {"sec-ch-ua-model", RequestHeader::kSecChUaModel},
        {"sec-ch-ua-platform", RequestHeader::kSecChUaPlatform},
        {"sec-ch-ua-platform-version", RequestHeader::kSecChUaPlatformVersion},
        {"sec-ch-ua-wow64", RequestHeader::kSecChUaWow64},
        {"sec-ch-viewport-height", RequestHeader::kSecChViewportHeight},
        {"sec-ch-viewport-width", RequestHeader::kSecChViewportWidth},
        {"sec-purpose", RequestHeader::kSecPurpose},
        {"service-worker", RequestHeader::kServiceWorker},
        {"service-worker-navigation-preload",
         RequestHeader::kServiceWorkerNavigationPreload},
        {"sid", RequestHeader::kSID},
        {"traceparent", RequestHeader::kTraceparent},
        {"upgrade-insecure-requests", RequestHeader::kUpgradeInsecureRequests},
        {"user-agent", RequestHeader::kUserAgent},
        {"viewport-width", RequestHeader::kViewportWidth},
        {"x-chrome-connected", RequestHeader::kXChromeConnected},
        {"x-chrome-id-consistency-request",
         RequestHeader::kXChromeIDConsistencyRequest},
        {"x-client-data", RequestHeader::kXClientData},
        {"x-developer-key", RequestHeader::kXDeveloperKey},
        {"x-goog-api-key", RequestHeader::kXGoogApiKey},
        {"x-goog-encode-response-if-executable",
         RequestHeader::kXGoogEncodeResponseIfExecutable},
        {"x-goog-ext-174067345-bin", RequestHeader::kXGoogExt174067345Bin},
        {"x-goog-update-appid", RequestHeader::kXGoogUpdateAppId},
        {"x-goog-update-interactivity",
         RequestHeader::kXGoogUpdateInteractivity},
        {"x-goog-update-updater", RequestHeader::kXGoogUpdateUpdater},
        {"x-http-method-override", RequestHeader::kXHTTPMethodOverride},
        {"x-oauth-client-id", RequestHeader::kXOAuthClientID},
        {"x-requested-with", RequestHeader::kXRequestedWith},
        {"x-server-timeout", RequestHeader::kXServerTimeout},
        {"x-use-alt-service", RequestHeader::kXUseAltService},
        {"x-webchannel-content-type", RequestHeader::kXWebChannelContentType},
    });

}  // namespace

RequestHeader LowerCaseRequestHeaderToEnum(std::string_view name) {
  auto it = kHeaderMap.find(name);
  if (it != kHeaderMap.end()) {
    return it->second;
  }
  return RequestHeader::kOther;
}

void LogLowerCaseRequestHeaderToUma(std::string_view histogram_name,
                                    std::string_view header_name) {
  base::UmaHistogramEnumeration(histogram_name,
                                LowerCaseRequestHeaderToEnum(header_name));
}

}  // namespace network
