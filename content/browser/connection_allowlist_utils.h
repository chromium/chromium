// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CONNECTION_ALLOWLIST_UTILS_H_
#define CONTENT_BROWSER_CONNECTION_ALLOWLIST_UTILS_H_

#include <optional>

#include "base/unguessable_token.h"
#include "content/common/content_export.h"
#include "net/base/network_anonymization_key.h"
#include "services/network/public/cpp/connection_allowlist.h"
#include "services/network/public/mojom/network_context.mojom-forward.h"
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

// Returns true if the initiator policies have any active connection allowlists
// (either enforced or report-only).
bool HasActiveConnectionAllowlists(
    const PolicyContainerPolicies& initiator_policies);

// Returns true if the connection allowlist allows redirect. If the redirect is
// not allowed, a report will be generated, and this function returns false.
//
// TODO(482728970): This method name should probably shift to match the
// "AndReportIfNeeded" pattern.
CONTENT_EXPORT bool IsRedirectAllowedByConnectionAllowlist(
    const PolicyContainerPolicies& initiator_policies,
    const GURL& original_url,
    network::mojom::NetworkContext* network_context,
    const net::NetworkAnonymizationKey& network_anonymization_key,
    const std::optional<base::UnguessableToken>& reporting_source);

// Returns true if the connection allowlist enforced by `policies` allows `url`.
// If the URL is blocked, handles reporting and returns false.
// If the feature is disabled or there is no enforced allowlist in policies,
// this function returns true.
CONTENT_EXPORT bool ConnectionAllowlistAllowsUrlAndReportIfNeeded(
    const PolicyContainerPolicies& policies,
    const GURL& url,
    network::mojom::NetworkContext* network_context,
    const net::NetworkAnonymizationKey& network_anonymization_key,
    const std::optional<base::UnguessableToken>& reporting_source);

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
