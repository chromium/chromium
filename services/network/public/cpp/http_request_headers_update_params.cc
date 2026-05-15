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

}  // namespace network
