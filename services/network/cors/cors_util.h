// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_CORS_CORS_UTIL_H_
#define SERVICES_NETWORK_CORS_CORS_UTIL_H_

#include <optional>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "net/http/http_request_headers.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace network::cors {

class OriginAccessList;

// Checks whether the request is allowed to set forbidden request headers.
//   - `origin_access_list`: contains the list of allowed origins.
//   - `request_initiator`: is the initiator origin of the request.
//   - `url`: is the target URL of the request.
//
// Returns `true` if `kBypassRequestForbiddenHeadersCheck` feature is
// enabled and `request_initiator` is allowed to access `url` according to
// `origin_access_list`.
COMPONENT_EXPORT(NETWORK_SERVICE)
bool ShouldAllowUnsafeHeaders(
    const OriginAccessList& origin_access_list,
    const std::optional<url::Origin>& request_initiator,
    const GURL& url);

// https://fetch.spec.whatwg.org/#cors-unsafe-request-header-names
// Returns header names which are not CORS-safelisted AND not forbidden.
// `headers` must not contain multiple headers for the same name.
// When `is_revalidating` is true, "if-modified-since", "if-none-match", and
// "cache-control" are also exempted.
// The returned list is NOT sorted.
// The returned list consists of lower-cased names.
COMPONENT_EXPORT(NETWORK_SERVICE)
std::vector<std::string> CorsUnsafeNotForbiddenRequestHeaderNames(
    const net::HttpRequestHeaders::HeaderVector& headers,
    bool is_revalidating);

}  // namespace network::cors

#endif  // SERVICES_NETWORK_CORS_CORS_UTIL_H_
