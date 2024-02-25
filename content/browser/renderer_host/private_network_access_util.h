// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#ifndef CONTENT_BROWSER_RENDERER_HOST_PRIVATE_NETWORK_ACCESS_UTIL_H_
#define CONTENT_BROWSER_RENDERER_HOST_PRIVATE_NETWORK_ACCESS_UTIL_H_

#include "content/common/content_export.h"
#include "services/network/public/mojom/client_security_state.mojom-forward.h"
#include "services/network/public/mojom/ip_address_space.mojom-forward.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"
#include "url/gurl.h"

namespace content {

class ContentBrowserClient;
struct PolicyContainerPolicies;

enum class PrivateNetworkRequestContext {
  kSubresource,  // Subresource fetches initiated by documents.
  kWorker,  // Worker script fetches/updates or fetches within worker scripts.
  kNavigation,  // Navigation fetches
};

// Returns the policy to use for private network requests fetched by a client
// with the given context properties.
//
// `ip_address_space` identifies the IP address space of the request client.
// `is_web_secure_context` specifies whether the request client is a secure
// context or not.
// `private_network_request_context` specifies what this request is about. For
// example, requests made from workers can have different policies from normal
// subresource requests.
network::mojom::PrivateNetworkRequestPolicy CONTENT_EXPORT
DerivePrivateNetworkRequestPolicy(
    network::mojom::IPAddressSpace ip_address_space,
    bool is_web_secure_context,
    PrivateNetworkRequestContext private_network_request_context);

// Convenience overload to directly compute this from the client's `policies`.
network::mojom::PrivateNetworkRequestPolicy CONTENT_EXPORT
DerivePrivateNetworkRequestPolicy(
    const PolicyContainerPolicies& policies,
    PrivateNetworkRequestContext private_network_request_context);

network::mojom::ClientSecurityStatePtr CONTENT_EXPORT DeriveClientSecurityState(
    const PolicyContainerPolicies& policies,
    PrivateNetworkRequestContext private_network_request_context);

// Determines the IP address space that should be associated to execution
// contexts instantiated from a resource loaded from this `url` and the given
// response.
//
// This differs from `network::CalculateClientAddressSpace()` in that it takes
// into account special schemes that are only known to the embedder, for which
// the embedder decides the IP address space.
//
// `url` the response URL.
// `response_head` identifies the response headers received which may be
// nullptr. `client` exposes the embedder API which may be nullptr.
network::mojom::IPAddressSpace CalculateIPAddressSpace(
    const GURL& url,
    network::mojom::URLResponseHead* response_head,
    ContentBrowserClient* client);

network::mojom::PrivateNetworkRequestPolicy OverrideBlockWithWarn(
    network::mojom::PrivateNetworkRequestPolicy);

}  // namespace content

#endif  //  CONTENT_BROWSER_RENDERER_HOST_PRIVATE_NETWORK_ACCESS_UTIL_H_
