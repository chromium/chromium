// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CONNECTION_ALLOWLIST_GATING_H_
#define CONTENT_BROWSER_CONNECTION_ALLOWLIST_GATING_H_

#include "services/network/public/mojom/url_response_head.mojom-forward.h"

class GURL;
namespace net {
class HttpResponseHeaders;
}

namespace content {

// Returns true if the parsed response headers contains a valid
// "Connection-Allowlist" or "Connection-Allowlist-Report-Only" header.
bool ResponseContainsConnectionAllowlist(
    const network::mojom::URLResponseHead* response_head);

// Returns true if the response enables connection allowlist origin trial.
bool ResponseEnablesConnectionAllowlistsOriginTrial(
    const GURL& request_url,
    const net::HttpResponseHeaders* response_headers);

}  // namespace content

#endif  // CONTENT_BROWSER_CONNECTION_ALLOWLIST_GATING_H_
