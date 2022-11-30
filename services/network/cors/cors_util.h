// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_CORS_CORS_UTIL_H_
#define SERVICES_NETWORK_CORS_CORS_UTIL_H_

#include <string>
#include <vector>

#include "base/component_export.h"
#include "net/http/http_request_headers.h"

namespace network {

namespace cors {

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

}  // namespace cors

}  // namespace network

#endif  // SERVICES_NETWORK_CORS_CORS_UTIL_H_
