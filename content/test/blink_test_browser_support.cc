// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/blink_test_browser_support.h"

#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/render_frame_host.h"
#include "third_party/blink/public/common/unique_name/unique_name_helper.h"

namespace content {

std::string GetFrameNameFromBrowserForWebTests(
    RenderFrameHost* render_frame_host) {
  RenderFrameHostImpl* render_frame_host_impl =
      static_cast<RenderFrameHostImpl*>(render_frame_host);
  FrameTreeNode* frame_tree_node = render_frame_host_impl->frame_tree_node();
  return blink::UniqueNameHelper::ExtractStableNameForTesting(
      frame_tree_node->unique_name());
}

}  // namespace content
