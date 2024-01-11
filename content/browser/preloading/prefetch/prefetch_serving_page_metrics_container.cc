// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_serving_page_metrics_container.h"

#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/navigation_handle_user_data.h"
#include "content/public/browser/web_contents.h"

namespace content {

PrefetchServingPageMetricsContainer::PrefetchServingPageMetricsContainer(
    NavigationHandle& navigation_handle) {
  WebContents* initiator_web_contents =
      navigation_handle.GetInitiatorFrameToken()
          ? WebContents::FromRenderFrameHost(
                RenderFrameHostImpl::FromFrameToken(
                    navigation_handle.GetInitiatorProcessId(),
                    *navigation_handle.GetInitiatorFrameToken()))
          : nullptr;
  serving_page_metrics_.same_tab_as_prefetching_tab =
      initiator_web_contents &&
      initiator_web_contents == navigation_handle.GetWebContents();
}

PrefetchServingPageMetricsContainer::~PrefetchServingPageMetricsContainer() =
    default;

void PrefetchServingPageMetricsContainer::SetPrefetchStatus(
    PrefetchStatus prefetch_status) {
  serving_page_metrics_.prefetch_status = static_cast<int>(prefetch_status);
}

void PrefetchServingPageMetricsContainer::SetRequiredPrivatePrefetchProxy(
    bool required_private_prefetch_proxy) {
  serving_page_metrics_.required_private_prefetch_proxy =
      required_private_prefetch_proxy;
}

void PrefetchServingPageMetricsContainer::SetPrefetchHeaderLatency(
    const std::optional<base::TimeDelta>& prefetch_header_latency) {
  serving_page_metrics_.prefetch_header_latency = prefetch_header_latency;
}

void PrefetchServingPageMetricsContainer::SetProbeLatency(
    const base::TimeDelta& probe_latency) {
  serving_page_metrics_.probe_latency = probe_latency;
}

NAVIGATION_HANDLE_USER_DATA_KEY_IMPL(PrefetchServingPageMetricsContainer);

}  // namespace content
