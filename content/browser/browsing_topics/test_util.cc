// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browsing_topics/test_util.h"

#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_request.h"

namespace content {

IframeBrowsingTopicsAttributeWatcher::~IframeBrowsingTopicsAttributeWatcher() =
    default;

void IframeBrowsingTopicsAttributeWatcher::OnDidStartNavigation(
    NavigationHandle* navigation_handle) {
  FrameTreeNode* frame_tree_node =
      static_cast<NavigationRequest*>(navigation_handle)->frame_tree_node();

  last_navigation_has_iframe_browsing_topics_attribute_ =
      frame_tree_node->browsing_topics();

  content::TestNavigationObserver::OnDidStartNavigation(navigation_handle);
}

}  // namespace content
