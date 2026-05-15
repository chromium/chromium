// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_HTTP_REQUEST_HEADERS_UPDATE_PARAMS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_HTTP_REQUEST_HEADERS_UPDATE_PARAMS_H_

#include <string>
#include <vector>

#include "base/component_export.h"
#include "net/http/http_request_headers.h"

namespace network {

// Represents modifications to the request headers of a network request,
// typically `network::ResourceRequest::headers` and
// `network::ResourceRequest::cors_exempt_headers`.
//
// Modification semantics:
// 1. First, the headers in `removed_headers` should be removed from both of
//    `headers` and `cors_exempt_headers`.
// 2. Then, `modified_headers` and `modified_cors_exempt_headers` should be
//    added to `headers` and `cors_exempt_headers`, respectively.
struct COMPONENT_EXPORT(NETWORK_CPP_BASE) HttpRequestHeadersUpdateParams final {
  HttpRequestHeadersUpdateParams();
  ~HttpRequestHeadersUpdateParams();

  // Move-only.
  HttpRequestHeadersUpdateParams(HttpRequestHeadersUpdateParams&&);
  HttpRequestHeadersUpdateParams& operator=(HttpRequestHeadersUpdateParams&&);
  HttpRequestHeadersUpdateParams(const HttpRequestHeadersUpdateParams&) =
      delete;
  HttpRequestHeadersUpdateParams& operator=(
      const HttpRequestHeadersUpdateParams&) = delete;

  std::vector<std::string> removed_headers;
  net::HttpRequestHeaders modified_headers;
  net::HttpRequestHeaders modified_cors_exempt_headers;
};

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_HTTP_REQUEST_HEADERS_UPDATE_PARAMS_H_
