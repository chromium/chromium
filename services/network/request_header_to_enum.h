// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_REQUEST_HEADER_TO_ENUM_H_
#define SERVICES_NETWORK_REQUEST_HEADER_TO_ENUM_H_

#include <string_view>

namespace network {

// Request header names that have been observed sent from Chrome on major sites
// and also appear in the source code. This does not include request headers
// that are added by CorsURLLoader or within //net, such as
// "Access-Control-Request-Method" or "Connection".

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(RequestHeader)
enum class RequestHeader {
  kOther = 0,
  kAccept = 1,
  kAcceptEncoding = 2,
  kAcceptLanguage = 3,
  kAuthorization = 4,
  kCacheControl = 5,
  kClientVersion = 6,
  kContentEncoding = 7,
  kContentType = 8,
  kDeviceMemory = 9,
  kDownlink = 10,
  kDpr = 11,
  kEct = 12,
  kGoogleTranslateElementMode = 13,
  kIfModifiedSince = 14,
  kIfNoneMatch = 15,
  kOrigin = 16,
  kPingFrom = 17,
  kPingTo = 18,
  kPragma = 19,
  kRange = 20,
  kRequestId = 21,
  kRtt = 22,
  kSecBrowsingTopics = 23,
  kSecChDeviceMemory = 24,
  kSecChDpr = 25,
  kSecChPrefersColorScheme = 26,
  kSecChUa = 27,
  kSecChUaArch = 28,
  kSecChUaBitness = 29,
  kSecChUaFormFactors = 30,
  kSecChUaFullVersion = 31,
  kSecChUaFullVersionList = 32,
  kSecChUaMobile = 33,
  kSecChUaModel = 34,
  kSecChUaPlatform = 35,
  kSecChUaPlatformVersion = 36,
  kSecChUaWow64 = 37,
  kSecChViewportHeight = 38,
  kSecChViewportWidth = 39,
  kSecPurpose = 40,
  kServiceWorker = 41,
  kServiceWorkerNavigationPreload = 42,
  kSID = 43,
  kTraceparent = 44,
  kUpgradeInsecureRequests = 45,
  kUserAgent = 46,
  kViewportWidth = 47,
  kXChromeConnected = 48,
  kXChromeIDConsistencyRequest = 49,
  kXClientData = 50,
  kXDeveloperKey = 51,
  kXGoogApiKey = 52,
  kXGoogEncodeResponseIfExecutable = 53,
  kXGoogExt174067345Bin = 54,
  kXGoogUpdateAppId = 55,
  kXGoogUpdateInteractivity = 56,
  kXGoogUpdateUpdater = 57,
  kXHTTPMethodOverride = 58,
  kXOAuthClientID = 59,
  kXRequestedWith = 60,
  kXServerTimeout = 61,
  kXUseAltService = 62,
  kXWebChannelContentType = 63,

  // If you need to add a new header, add it here. Do not attempt to preserve
  // alphabetic order.
  kMaxValue = kXWebChannelContentType,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/network/enums.xml:RequestHeader)

// Returns the corresponding enum value if `name` is a match
// for one of the headers in the RequestHeader enum, or RequestHeader::kOther
// otherwise. `name` must be in lower-case.
RequestHeader LowerCaseRequestHeaderToEnum(std::string_view name);

// Logs the request header to UMA with the specific histogram name.
// `header_name` must be in lower-case.
void LogLowerCaseRequestHeaderToUma(std::string_view histogram_name,
                                    std::string_view header_name);

}  // namespace network

#endif  // SERVICES_NETWORK_REQUEST_HEADER_TO_ENUM_H_
