// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_PROTECTED_AUDIENCE_NETWORK_UTIL_H_
#define CONTENT_BROWSER_INTEREST_GROUP_PROTECTED_AUDIENCE_NETWORK_UTIL_H_

#include <optional>
#include <string>

#include "content/public/browser/frame_tree_node_id.h"
#include "services/network/public/mojom/client_security_state.mojom.h"
#include "services/network/public/mojom/ip_address_space.mojom.h"

namespace network {
struct ResourceRequest;
}

namespace content {

class FrameTreeNode;

// Returns a per-frame possible override to the user-agent string. Returns
// nullopt if no user agent override should be used. Only checks the override
// provided by NavigatorDelegate, not the devtools one, which can be retrieved
// through SetUpDevToolsForRequests.
//
// Logic is not ProtectedAudiences-specific, apart from being feature-gated, but
// not much else does this. If the caller already has a FrameTreeNode, the
// overload that takes that is marginally more performant.
std::optional<std::string> GetUserAgentOverrideForProtectedAudience(
    FrameTreeNode* frame_tree_node);
std::optional<std::string> GetUserAgentOverrideForProtectedAudience(
    FrameTreeNodeId frame_tree_node_id);

// Helper that sets up most devtools hooks for a request.
//
// In particular:
// * Sets up devtools network throttling.
// * Sets up devtools network request overrides, which may modify headers.
// * Sets up DevTools observer.
//
// Does not apply non-devtools User-Agent overrides that can be retrieved by
// GetUserAgentOverrideForProtectedAudience(), and which should be applied
// before calling this method. Nor does it invoke devtools_instrumentation
// browser-side network request status methods
// (OnAuctionWorkletNetworkRequestWillBeSent(), etc).
void SetUpDevtoolsForRequest(FrameTreeNode* frame_tree_node,
                             network::ResourceRequest& request);

// Creates a ClientSecurityState object for use with ProtectedAudience. Only
// contains the provided IPAddressSpace, and request policy, and identifies the
// context as secure. This matches the Protected Audience spec, and avoids
// leaks.
network::mojom::ClientSecurityStatePtr
CreateClientSecurityStateForProtectedAudience(
    network::mojom::IPAddressSpace ip_address_space);

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_PROTECTED_AUDIENCE_NETWORK_UTIL_H_
