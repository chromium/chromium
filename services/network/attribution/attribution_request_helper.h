// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_ATTRIBUTION_ATTRIBUTION_REQUEST_HELPER_H_
#define SERVICES_NETWORK_ATTRIBUTION_ATTRIBUTION_REQUEST_HELPER_H_

#include "net/http/http_request_headers.h"

namespace network {

struct ResourceRequest;

// Computes the Attribution Reporting request headers on attribution eligible
// requests. See https://github.com/WICG/attribution-reporting-api.
net::HttpRequestHeaders ComputeAttributionReportingHeaders(
    const ResourceRequest&);

}  // namespace network

#endif  // SERVICES_NETWORK_ATTRIBUTION_ATTRIBUTION_REQUEST_HELPER_H_
