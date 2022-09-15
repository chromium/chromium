// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/stored_page.h"

#include "base/trace_event/typed_macros.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_frame_proxy_host.h"

namespace content {
namespace {
using perfetto::protos::pbzero::ChromeTrackEvent;
}

StoredPage::StoredPage(std::unique_ptr<RenderFrameHostImpl> rfh,
                       RenderFrameProxyHostMap proxy_hosts,
                       std::set<RenderViewHostImpl*> render_view_hosts)
    : render_frame_host(std::move(rfh)),
      proxy_hosts(std::move(proxy_hosts)),
      render_view_hosts(std::move(render_view_hosts)) {
  for (const auto& pair : this->proxy_hosts) {
    TRACE_EVENT_INSTANT("navigation", "StoredPage::StoredPage_Proxy",
                        ChromeTrackEvent::kRenderFrameProxyHost, *pair.second);
  }
}

StoredPage::~StoredPage() = default;

}  // namespace content
