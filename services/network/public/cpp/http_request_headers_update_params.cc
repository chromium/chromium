// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/http_request_headers_update_params.h"

namespace network {

HttpRequestHeadersUpdateParams::HttpRequestHeadersUpdateParams() = default;
HttpRequestHeadersUpdateParams::~HttpRequestHeadersUpdateParams() = default;
HttpRequestHeadersUpdateParams::HttpRequestHeadersUpdateParams(
    HttpRequestHeadersUpdateParams&&) = default;
HttpRequestHeadersUpdateParams& HttpRequestHeadersUpdateParams::operator=(
    HttpRequestHeadersUpdateParams&&) = default;

void HttpRequestHeadersUpdateParams::Apply(
    net::HttpRequestHeaders& headers,
    net::HttpRequestHeaders& cors_exempt_headers) const {
  for (const auto& removed_header : removed_headers) {
    headers.RemoveHeader(removed_header);
    cors_exempt_headers.RemoveHeader(removed_header);
  }
  headers.MergeFrom(modified_headers);
  cors_exempt_headers.MergeFrom(modified_cors_exempt_headers);
}

}  // namespace network
