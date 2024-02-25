// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/prefetch_metrics.h"

#include "content/browser/preloading/prefetch/prefetch_document_manager.h"
#include "content/browser/preloading/prefetch/prefetch_serving_page_metrics_container.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"

namespace content {

// static
std::optional<PrefetchReferringPageMetrics>
PrefetchReferringPageMetrics::GetForCurrentDocument(RenderFrameHost* rfh) {
  DCHECK(rfh);
  PrefetchDocumentManager* prefetch_document_manager =
      PrefetchDocumentManager::GetForCurrentDocument(rfh);
  if (!prefetch_document_manager)
    return std::nullopt;

  return prefetch_document_manager->GetReferringPageMetrics();
}

// static
std::optional<PrefetchServingPageMetrics>
PrefetchServingPageMetrics::GetForNavigationHandle(
    NavigationHandle& navigation_handle) {
  PrefetchServingPageMetricsContainer* prefetch_serving_page_metrics_container =
      PrefetchServingPageMetricsContainer::GetForNavigationHandle(
          navigation_handle);
  if (!prefetch_serving_page_metrics_container)
    return std::nullopt;

  return prefetch_serving_page_metrics_container->GetServingPageMetrics();
}

}  // namespace content
