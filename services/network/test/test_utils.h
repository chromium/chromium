// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_TEST_TEST_UTILS_H_
#define SERVICES_NETWORK_TEST_TEST_UTILS_H_

#include <string>
#include <vector>

#include "net/http/http_status_code.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"

namespace network {
struct ResourceRequest;

// Helper method to read the upload bytes from a ResourceRequest with a body.
std::string GetUploadData(const network::ResourceRequest& request);

// Helper methods used to create a mojom::URLResponseHead with the given status.
// If |report_raw_headers| is true, the |raw_request_response_info| field of the
// returned mojom::URLResponseHead is also set to contain the raw headers as
// reported by the network stack (it's useful to report cookies, for example,
// as they are filtered out of the net::HttpResponseHeaders when serialized).
mojom::URLResponseHeadPtr CreateURLResponseHead(
    net::HttpStatusCode http_status,
    bool report_raw_headers = false);

// Adds cookies to the passed in mojom::URLResponseHead. If it was created with
// |report_raw_headers| true, the cookies are also added to the raw headers.
void AddCookiesToURLResponseHead(const std::vector<std::string>& cookies,
                                 mojom::URLResponseHead* response_header);

}  // namespace network

#endif  // SERVICES_NETWORK_TEST_TEST_UTILS_H_
