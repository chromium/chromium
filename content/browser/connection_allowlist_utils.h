// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CONNECTION_ALLOWLIST_UTILS_H_
#define CONTENT_BROWSER_CONNECTION_ALLOWLIST_UTILS_H_

#include "services/network/public/cpp/connection_allowlist.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"

class GURL;
namespace net {
class HttpResponseHeaders;
}

namespace content {

struct PolicyContainerPolicies;

// Returns true if the parsed response headers contains a valid
// "Connection-Allowlist" or "Connection-Allowlist-Report-Only" header.
bool ResponseContainsConnectionAllowlist(
    const network::mojom::URLResponseHead* response_head);

// Returns true if the response enables connection allowlist origin trial.
bool ResponseEnablesConnectionAllowlistsOriginTrial(
    const GURL& request_url,
    const net::HttpResponseHeaders* response_headers);

// Returns true if the initiator policies enforce connection allowlist.
bool EnforcesConnectionAllowlist(
    const PolicyContainerPolicies& initiator_policies);

// Returns true if the connection allowlist allows redirect.
bool IsRedirectAllowedByConnectionAllowlist(
    const PolicyContainerPolicies& initiator_policies);

// Returns true if the connection allowlist enforced by `policies` allows `url`.
// If the URL is blocked, handles reporting (TODO) and returns false.
// If the feature is disabled or there is no enforced allowlist in policies,
// this function returns true.
bool ConnectionAllowlistAllowsUrlAndReportIfNeeded(
    const PolicyContainerPolicies& policies,
    const GURL& url);

// Evaluates the response and returns the ConnectionAllowlists that should apply
// to the worker. Handles local scheme inheritance from creator_policies and
// validates the Connection-Allowlist Origin Trial for network responses.
network::ConnectionAllowlists GetConnectionAllowlistsForWorker(
    const GURL& response_url,
    const network::mojom::URLResponseHead* response_head,
    const PolicyContainerPolicies* creator_policies,
    bool inherit_from_creator);

}  // namespace content

#endif  // CONTENT_BROWSER_CONNECTION_ALLOWLIST_UTILS_H_
