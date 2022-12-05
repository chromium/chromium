// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/scoped_active_url.h"

#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_frame_proxy_host.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/common/content_client.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

ScopedActiveURL::ScopedActiveURL(const GURL& active_url,
                                 const url::Origin& top_origin) {
  GetContentClient()->SetActiveURL(active_url, top_origin.GetDebugString());
}

ScopedActiveURL::ScopedActiveURL(RenderFrameHost* frame)
    : ScopedActiveURL(
          static_cast<RenderFrameHostImpl*>(frame)->frame_tree_node()) {}

ScopedActiveURL::ScopedActiveURL(RenderFrameProxyHost* proxy)
    : ScopedActiveURL(proxy->frame_tree_node()) {}

ScopedActiveURL::ScopedActiveURL(RenderViewHost* view)
    : ScopedActiveURL(
          static_cast<RenderViewHostImpl*>(view)->frame_tree()->root()) {}

ScopedActiveURL::ScopedActiveURL(FrameTreeNode* node)
    : ScopedActiveURL(node->current_url(),
                      node->frame_tree().root()->current_origin()) {}

ScopedActiveURL::~ScopedActiveURL() {
  GetContentClient()->SetActiveURL(GURL(), "");
}

}  // namespace content
