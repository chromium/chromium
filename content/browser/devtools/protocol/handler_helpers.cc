// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/handler_helpers.h"
#include "content/browser/renderer_host/frame_tree_node.h"

namespace content {

namespace protocol {

FrameTreeNode* FrameTreeNodeFromDevToolsFrameToken(
    FrameTreeNode* root,
    const std::string& devtools_frame_token) {
  if (root->devtools_frame_token().ToString() == devtools_frame_token) {
    return root;
  } else {
    for (FrameTreeNode* node : root->frame_tree()->SubtreeNodes(root)) {
      if (node->devtools_frame_token().ToString() == devtools_frame_token) {
        return node;
      }
    }
  }
  return nullptr;
}

}  // namespace protocol
}  // namespace content
