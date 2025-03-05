// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_PROTECTED_AUDIENCE_NETWORK_UTIL_H_
#define CONTENT_BROWSER_INTEREST_GROUP_PROTECTED_AUDIENCE_NETWORK_UTIL_H_

#include <optional>
#include <string>

#include "content/public/browser/frame_tree_node_id.h"

namespace content {

class FrameTreeNode;

// Used to get a possible override to the user-agent string. Returns nullopt
// if no user agent override should be used.
//
// Logic is not ProtectedAudiences-specific, apart from being feature-gated, but
// not much else does this. If the caller already has a FrameTreeNode, the
// overload that takes that is marginally more performant.
std::optional<std::string> GetUserAgentOverrideForProtectedAudience(
    FrameTreeNode* frame_tree_node);
std::optional<std::string> GetUserAgentOverrideForProtectedAudience(
    FrameTreeNodeId frame_tree_node_id);

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_PROTECTED_AUDIENCE_NETWORK_UTIL_H_
