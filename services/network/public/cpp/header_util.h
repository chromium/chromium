// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_HEADER_UTIL_H_
#define SERVICES_NETWORK_PUBLIC_CPP_HEADER_UTIL_H_

#include <string_view>

#include "base/component_export.h"
#include "services/network/public/mojom/referrer_policy.mojom.h"

class GURL;
namespace net {
class HttpRequestHeaders;
class HttpResponseHeaders;
}  // namespace net

namespace network {
namespace mojom {
class URLResponseHead;
}  // namespace mojom

// Checks if a single request header is safe to send.
COMPONENT_EXPORT(NETWORK_CPP)
bool IsRequestHeaderSafe(std::string_view key, std::string_view value);

// Checks if any single header in a set of request headers is not safe to send.
// When adding sets of headers together, it's safe to call this on each set
// individually.
COMPONENT_EXPORT(NETWORK_CPP)
bool AreRequestHeadersSafe(const net::HttpRequestHeaders& request_headers);

// Parses the referrer policy header if present. Returns
// mojom::ReferrerPolicy::kDefault if the header is absent.
COMPONENT_EXPORT(NETWORK_CPP)
mojom::ReferrerPolicy ParseReferrerPolicy(
    const net::HttpResponseHeaders& request_headers);

// Checks whether mime type sniffing should be enabled, considering response
// headers, current mime type and URL scheme.
COMPONENT_EXPORT(NETWORK_CPP)
bool ShouldSniffContent(const GURL& url,
                        const mojom::URLResponseHead& response);

// https://fetch.spec.whatwg.org/#ok-status aka a successful 2xx status code,
// https://www.rfc-editor.org/rfc/rfc9110#status.2xx.
COMPONENT_EXPORT(NETWORK_CPP) bool IsSuccessfulStatus(int status);

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_HEADER_UTIL_H_
