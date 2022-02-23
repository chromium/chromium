// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_STORED_PAGE_H_
#define CONTENT_BROWSER_RENDERER_HOST_STORED_PAGE_H_

#include <unordered_map>

#include "content/browser/site_instance_group.h"
#include "content/public/browser/site_instance.h"
#include "third_party/blink/public/mojom/page/page.mojom.h"

namespace content {
class RenderFrameHostImpl;
class RenderFrameProxyHost;
class RenderViewHostImpl;

// StoredPage contains a page which is not tied to a FrameTree. It holds the
// main RenderFrameHost together with RenderViewHosts and main document's
// proxies. It's used for storing pages in back/forward cache or when preparing
// prerendered pages for activation.
struct StoredPage {
  using RenderFrameProxyHostMap =
      std::unordered_map<SiteInstanceGroupId,
                         std::unique_ptr<RenderFrameProxyHost>,
                         SiteInstanceGroupId::Hasher>;

  StoredPage(std::unique_ptr<RenderFrameHostImpl> rfh,
             RenderFrameProxyHostMap proxy_hosts,
             std::set<RenderViewHostImpl*> render_view_hosts);
  ~StoredPage();

  // The main document being stored.
  std::unique_ptr<RenderFrameHostImpl> render_frame_host;

  // Proxies of the main document as seen by other processes.
  // Currently, we only store proxies for SiteInstanceGroups of all subframes on
  // the page, because pages using window.open and nested WebContents are
  // not cached.
  RenderFrameProxyHostMap proxy_hosts;

  // RenderViewHosts belonging to the main frame, and its proxies (if any).
  //
  // While RenderViewHostImpl(s) are in the BackForwardCache, they aren't
  // reused for pages outside the cache. This prevents us from having two
  // main frames, (one in the cache, one live), associated with a single
  // RenderViewHost.
  //
  // Keeping these here also prevents RenderFrameHostManager code from
  // unwittingly iterating over RenderViewHostImpls that are in the cache.
  std::set<RenderViewHostImpl*> render_view_hosts;

  // Additional parameters to send with SetPageLifecycleState calls when
  // we're restoring a page from the back-forward cache.
  blink::mojom::PageRestoreParamsPtr page_restore_params;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_STORED_PAGE_H_
