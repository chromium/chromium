// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/stored_page.h"

#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_frame_proxy_host.h"

namespace content {

StoredPage::StoredPage(std::unique_ptr<RenderFrameHostImpl> rfh,
                       RenderFrameProxyHostMap proxy_hosts,
                       std::set<RenderViewHostImpl*> render_view_hosts)
    : render_frame_host(std::move(rfh)),
      proxy_hosts(std::move(proxy_hosts)),
      render_view_hosts(std::move(render_view_hosts)) {}

StoredPage::~StoredPage() = default;

}  // namespace content
