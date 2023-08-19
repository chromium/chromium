// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_url_loader_helper.h"

#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "content/browser/preloading/prefetch/prefetch_origin_prober.h"
#include "content/browser/preloading/prefetch/prefetch_params.h"
#include "content/browser/preloading/prefetch/prefetch_probe_result.h"
#include "content/browser/preloading/prefetch/prefetch_service.h"
#include "content/browser/preloading/prefetch/prefetch_serving_page_metrics_container.h"
#include "content/browser/preloading/prefetch/prefetch_status.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/public/browser/prefetch_metrics.h"
#include "content/public/browser/web_contents.h"
#include "services/network/public/cpp/resource_request.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"

namespace content {
namespace {

using GetPrefetchCallback = base::OnceCallback<void(PrefetchContainer::Reader)>;

PrefetchServingPageMetricsContainer*
PrefetchServingPageMetricsContainerFromFrameTreeNodeId(int frame_tree_node_id) {
  FrameTreeNode* frame_tree_node =
      FrameTreeNode::GloballyFindByID(frame_tree_node_id);
  if (!frame_tree_node || !frame_tree_node->navigation_request()) {
    return nullptr;
  }

  return PrefetchServingPageMetricsContainer::GetForNavigationHandle(
      *frame_tree_node->navigation_request());
}

void RecordCookieWaitTime(base::TimeDelta wait_time) {
  UMA_HISTOGRAM_CUSTOM_TIMES(
      "PrefetchProxy.AfterClick.Mainframe.CookieWaitTime", wait_time,
      base::TimeDelta(), base::Seconds(5), 50);
}

// Gets the relevant |GetPrefetchOriginProber| from |PrefetchService|.
PrefetchOriginProber* GetPrefetchOriginProber(int frame_tree_node_id) {
  PrefetchService* prefetch_service =
      PrefetchService::GetFromFrameTreeNodeId(frame_tree_node_id);
  if (!prefetch_service) {
    return nullptr;
  }

  return prefetch_service->GetPrefetchOriginProber();
}

// Called when all checks below are complete.
void OnComplete(int frame_tree_node_id,
                GetPrefetchCallback get_prefetch_callback,
                PrefetchContainer::Reader reader,
                PrefetchProbeResult probe_result) {
  if (!reader || !reader.IsPrefetchServable(PrefetchCacheableDuration())) {
    std::move(get_prefetch_callback).Run({});
    return;
  }

  // Delay updating the prefetch with the probe result in case it becomes not
  // servable.
  if (reader) {
    reader.OnPrefetchProbeResult(probe_result);

    PrefetchServingPageMetricsContainer* serving_page_metrics_container =
        PrefetchServingPageMetricsContainerFromFrameTreeNodeId(
            frame_tree_node_id);
    if (serving_page_metrics_container) {
      serving_page_metrics_container->SetPrefetchStatus(
          reader.GetPrefetchStatus());
    }
  }

  std::move(get_prefetch_callback).Run(std::move(reader));
}

// Called when cookie copy is completed (if asynchronously waited).
// `cookie_copy_start_time` is the time when we started waiting for cookies to
// be copied, delaying the navigation. Used to calculate total cookie wait
// time.
void OnCookieCopyComplete(int frame_tree_node_id,
                          GetPrefetchCallback get_prefetch_callback,
                          PrefetchContainer::Reader reader,
                          PrefetchProbeResult probe_result,
                          base::TimeTicks cookie_copy_start_time) {
  base::TimeDelta wait_time = base::TimeTicks::Now() - cookie_copy_start_time;
  DCHECK_GT(wait_time, base::TimeDelta());
  RecordCookieWaitTime(wait_time);
  OnComplete(frame_tree_node_id, std::move(get_prefetch_callback),
             std::move(reader), probe_result);
}

// Starts the cookie copy for next redirect hop of |reader|.
void StartCookieCopy(int frame_tree_node_id,
                     const PrefetchContainer::Reader& reader) {
  PrefetchService* prefetch_service =
      PrefetchService::GetFromFrameTreeNodeId(frame_tree_node_id);
  if (!prefetch_service) {
    return;
  }

  prefetch_service->CopyIsolatedCookies(reader);
}

// Ensures that the cookies for prefetch are copied from its isolated
// network context to the default network context.
void EnsureCookiesCopiedAndInterceptPrefetchedNavigation(
    int frame_tree_node_id,
    const network::ResourceRequest& tentative_resource_request,
    GetPrefetchCallback get_prefetch_callback,
    PrefetchContainer::Reader reader,
    PrefetchProbeResult probe_result) {
  if (reader && !reader.HasIsolatedCookieCopyStarted()) {
    StartCookieCopy(frame_tree_node_id, reader);
  }

  if (reader) {
    reader.OnInterceptorCheckCookieCopy();
  }

  if (reader && reader.IsIsolatedCookieCopyInProgress()) {
    auto temp_reader = reader.Clone();
    temp_reader.SetOnCookieCopyCompleteCallback(base::BindOnce(
        &OnCookieCopyComplete, frame_tree_node_id,
        std::move(get_prefetch_callback), std::move(reader), probe_result,
        /* cookie_copy_start_time */ base::TimeTicks::Now()));
    return;
  }

  RecordCookieWaitTime(base::TimeDelta());

  OnComplete(frame_tree_node_id, std::move(get_prefetch_callback),
             std::move(reader), probe_result);
}

// Called when the `PrefetchOriginProber` check is done (if performed).
// `probe_start_time` is used to calculate probe latency which is
// reported to the tab helper.
void OnProbeComplete(int frame_tree_node_id,
                     const network::ResourceRequest& tentative_resource_request,
                     GetPrefetchCallback get_prefetch_callback,
                     PrefetchContainer::Reader reader,
                     base::TimeTicks probe_start_time,
                     PrefetchProbeResult probe_result) {
  PrefetchServingPageMetricsContainer* serving_page_metrics_container =
      PrefetchServingPageMetricsContainerFromFrameTreeNodeId(
          frame_tree_node_id);
  if (serving_page_metrics_container) {
    serving_page_metrics_container->SetProbeLatency(base::TimeTicks::Now() -
                                                    probe_start_time);
  }

  if (PrefetchProbeResultIsSuccess(probe_result)) {
    EnsureCookiesCopiedAndInterceptPrefetchedNavigation(
        frame_tree_node_id, tentative_resource_request,
        std::move(get_prefetch_callback), std::move(reader), probe_result);
    return;
  }

  if (reader) {
    reader.OnPrefetchProbeResult(probe_result);

    if (serving_page_metrics_container) {
      serving_page_metrics_container->SetPrefetchStatus(
          reader.GetPrefetchStatus());
    }
  }

  std::move(get_prefetch_callback).Run({});
}

}  // namespace

void OnGotPrefetchToServe(
    int frame_tree_node_id,
    const network::ResourceRequest& tentative_resource_request,
    GetPrefetchCallback get_prefetch_callback,
    PrefetchContainer::Reader reader) {
  // The |tentative_resource_request.url| might be different from
  // |GetCurrentURLToServe()| because of No-Vary-Search non-exact url
  // match.
#if DCHECK_IS_ON()
  if (reader) {
    GURL::Replacements replacements;
    replacements.ClearRef();
    replacements.ClearQuery();
    DCHECK_EQ(tentative_resource_request.url.ReplaceComponents(replacements),
              reader.GetCurrentURLToServe().ReplaceComponents(replacements));
  }
#endif

  if (!reader || !reader.IsPrefetchServable(PrefetchCacheableDuration())) {
    std::move(get_prefetch_callback).Run({});
    return;
  }

  if (reader.HaveDefaultContextCookiesChanged()) {
    reader.GetPrefetchContainer()->SetPrefetchStatus(
        PrefetchStatus::kPrefetchNotUsedCookiesChanged);
    reader.GetPrefetchContainer()->UpdateServingPageMetrics();
    reader.GetPrefetchContainer()->ResetAllStreamingURLLoaders();

    std::move(get_prefetch_callback).Run({});
    return;
  }

  PrefetchOriginProber* origin_prober =
      GetPrefetchOriginProber(frame_tree_node_id);
  if (!origin_prober) {
    std::move(get_prefetch_callback).Run({});
    return;
  }
  if (reader.IsIsolatedNetworkContextRequiredToServe() &&
      origin_prober->ShouldProbeOrigins()) {
    origin_prober->Probe(
        url::SchemeHostPort(tentative_resource_request.url).GetURL(),
        base::BindOnce(&OnProbeComplete, frame_tree_node_id,
                       tentative_resource_request,
                       std::move(get_prefetch_callback), std::move(reader),
                       /* probe_start_time */ base::TimeTicks::Now()));
    return;
  }

  EnsureCookiesCopiedAndInterceptPrefetchedNavigation(
      frame_tree_node_id, tentative_resource_request,
      std::move(get_prefetch_callback), std::move(reader),
      PrefetchProbeResult::kNoProbing);
}

}  // namespace content
