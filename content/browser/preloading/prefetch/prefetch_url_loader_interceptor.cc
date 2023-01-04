// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_url_loader_interceptor.h"

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "content/browser/browser_context_impl.h"
#include "content/browser/loader/navigation_loader_interceptor.h"
#include "content/browser/preloading/prefetch/prefetch_container.h"
#include "content/browser/preloading/prefetch/prefetch_features.h"
#include "content/browser/preloading/prefetch/prefetch_from_string_url_loader.h"
#include "content/browser/preloading/prefetch/prefetch_origin_prober.h"
#include "content/browser/preloading/prefetch/prefetch_params.h"
#include "content/browser/preloading/prefetch/prefetch_probe_result.h"
#include "content/browser/preloading/prefetch/prefetch_service.h"
#include "content/browser/preloading/prefetch/prefetch_serving_page_metrics_container.h"
#include "content/browser/preloading/prefetch/prefetch_streaming_url_loader.h"
#include "content/browser/preloading/prefetch/prefetched_mainframe_response_container.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/public/browser/prefetch_metrics.h"
#include "content/public/browser/web_contents.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/single_request_url_loader_factory.h"
#include "services/network/public/cpp/wrapper_shared_url_loader_factory.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"

namespace content {
namespace {

BrowserContext* BrowserContextFromFrameTreeNodeId(int frame_tree_node_id) {
  WebContents* web_content =
      WebContents::FromFrameTreeNodeId(frame_tree_node_id);
  if (!web_content)
    return nullptr;
  return web_content->GetBrowserContext();
}

PrefetchService* PrefetchServiceFromFrameTreeNodeId(int frame_tree_node_id) {
  BrowserContext* browser_context =
      BrowserContextFromFrameTreeNodeId(frame_tree_node_id);
  if (!browser_context)
    return nullptr;
  return BrowserContextImpl::From(browser_context)->GetPrefetchService();
}

PrefetchServingPageMetricsContainer*
PrefetchServingPageMetricsContainerFromFrameTreeNodeId(int frame_tree_node_id) {
  FrameTreeNode* frame_tree_node =
      FrameTreeNode::GloballyFindByID(frame_tree_node_id);
  if (!frame_tree_node || !frame_tree_node->navigation_request())
    return nullptr;

  return PrefetchServingPageMetricsContainer::GetForNavigationHandle(
      *frame_tree_node->navigation_request());
}

void RecordCookieWaitTime(base::TimeDelta wait_time) {
  UMA_HISTOGRAM_CUSTOM_TIMES(
      "PrefetchProxy.AfterClick.Mainframe.CookieWaitTime", wait_time,
      base::TimeDelta(), base::Seconds(5), 50);
}

}  // namespace

// static
std::unique_ptr<PrefetchURLLoaderInterceptor>
PrefetchURLLoaderInterceptor::MaybeCreateInterceptor(int frame_tree_node_id) {
  if (!base::FeatureList::IsEnabled(features::kPrefetchUseContentRefactor))
    return nullptr;

  return std::make_unique<PrefetchURLLoaderInterceptor>(frame_tree_node_id);
}

PrefetchURLLoaderInterceptor::PrefetchURLLoaderInterceptor(
    int frame_tree_node_id)
    : frame_tree_node_id_(frame_tree_node_id) {}

PrefetchURLLoaderInterceptor::~PrefetchURLLoaderInterceptor() = default;

void PrefetchURLLoaderInterceptor::MaybeCreateLoader(
    const network::ResourceRequest& tenative_resource_request,
    BrowserContext* browser_context,
    NavigationLoaderInterceptor::LoaderCallback callback,
    NavigationLoaderInterceptor::FallbackCallback fallback_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK(!loader_callback_);
  loader_callback_ = std::move(callback);
  url_ = tenative_resource_request.url;
  GetPrefetch(url_, base::BindOnce(
                        &PrefetchURLLoaderInterceptor::OnGotPrefetchToServce,
                        weak_factory_.GetWeakPtr(), tenative_resource_request));
}

void PrefetchURLLoaderInterceptor::OnGotPrefetchToServce(
    const network::ResourceRequest& tenative_resource_request,
    base::WeakPtr<PrefetchContainer> prefetch_container) {
  // The |url_| might be different from |prefetch_container->GetURL()| because
  // of No-Vary-Search non-exact url match.
#if DCHECK_IS_ON()
  if (prefetch_container) {
    GURL::Replacements replacements;
    replacements.ClearRef();
    replacements.ClearQuery();
    DCHECK_EQ(url_.ReplaceComponents(replacements),
              prefetch_container->GetURL().ReplaceComponents(replacements));
  }
#endif

  if (!prefetch_container ||
      !prefetch_container->IsPrefetchServable(PrefetchCacheableDuration()) ||
      prefetch_container->HaveDefaultContextCookiesChanged()) {
    DoNotInterceptNavigation();
    return;
  }

  PrefetchOriginProber* origin_prober = GetPrefetchOriginProber();
  if (!origin_prober) {
    DoNotInterceptNavigation();
    return;
  }
  if (origin_prober->ShouldProbeOrigins()) {
    probe_start_time_ = base::TimeTicks::Now();
    base::OnceClosure on_success_callback =
        base::BindOnce(&PrefetchURLLoaderInterceptor::
                           EnsureCookiesCopiedAndInterceptPrefetchedNavigation,
                       weak_factory_.GetWeakPtr(), tenative_resource_request,
                       prefetch_container);

    origin_prober->Probe(
        url::SchemeHostPort(url_).GetURL(),
        base::BindOnce(&PrefetchURLLoaderInterceptor::OnProbeComplete,
                       weak_factory_.GetWeakPtr(), prefetch_container,
                       std::move(on_success_callback)));
    return;
  }

  prefetch_container->OnPrefetchProbeResult(PrefetchProbeResult::kNoProbing);
  PrefetchServingPageMetricsContainer* serving_page_metrics_container =
      PrefetchServingPageMetricsContainerFromFrameTreeNodeId(
          frame_tree_node_id_);
  if (serving_page_metrics_container)
    serving_page_metrics_container->SetPrefetchStatus(
        prefetch_container->GetPrefetchStatus());

  EnsureCookiesCopiedAndInterceptPrefetchedNavigation(tenative_resource_request,
                                                      prefetch_container);
}

void PrefetchURLLoaderInterceptor::GetPrefetch(
    const GURL& url,
    base::OnceCallback<void(base::WeakPtr<PrefetchContainer>)>
        get_prefetch_callback) const {
  PrefetchService* prefetch_service =
      PrefetchServiceFromFrameTreeNodeId(frame_tree_node_id_);
  if (!prefetch_service) {
    std::move(get_prefetch_callback).Run(nullptr);
    return;
  }

  prefetch_service->GetPrefetchToServe(url, std::move(get_prefetch_callback));
}

PrefetchOriginProber* PrefetchURLLoaderInterceptor::GetPrefetchOriginProber()
    const {
  PrefetchService* prefetch_service =
      PrefetchServiceFromFrameTreeNodeId(frame_tree_node_id_);
  if (!prefetch_service)
    return nullptr;

  return prefetch_service->GetPrefetchOriginProber();
}

void PrefetchURLLoaderInterceptor::OnProbeComplete(
    base::WeakPtr<PrefetchContainer> prefetch_container,
    base::OnceClosure on_success_callback,
    PrefetchProbeResult result) {
  DCHECK(probe_start_time_);

  PrefetchServingPageMetricsContainer* serving_page_metrics_container =
      PrefetchServingPageMetricsContainerFromFrameTreeNodeId(
          frame_tree_node_id_);
  if (serving_page_metrics_container)
    serving_page_metrics_container->SetProbeLatency(base::TimeTicks::Now() -
                                                    probe_start_time_.value());

  if (prefetch_container) {
    prefetch_container->OnPrefetchProbeResult(result);

    if (serving_page_metrics_container)
      serving_page_metrics_container->SetPrefetchStatus(
          prefetch_container->GetPrefetchStatus());
  }

  if (PrefetchProbeResultIsSuccess(result)) {
    std::move(on_success_callback).Run();
    return;
  }

  DoNotInterceptNavigation();
}

void PrefetchURLLoaderInterceptor::
    EnsureCookiesCopiedAndInterceptPrefetchedNavigation(
        const network::ResourceRequest& tenative_resource_request,
        base::WeakPtr<PrefetchContainer> prefetch_container) {
  if (prefetch_container) {
    prefetch_container->OnInterceptorCheckCookieCopy();
  }

  if (prefetch_container &&
      prefetch_container->IsIsolatedCookieCopyInProgress()) {
    cookie_copy_start_time_ = base::TimeTicks::Now();
    prefetch_container->SetOnCookieCopyCompleteCallback(base::BindOnce(
        &PrefetchURLLoaderInterceptor::InterceptPrefetchedNavigation,
        weak_factory_.GetWeakPtr(), tenative_resource_request,
        prefetch_container));
    return;
  }

  RecordCookieWaitTime(base::TimeDelta());

  InterceptPrefetchedNavigation(tenative_resource_request, prefetch_container);
}

void PrefetchURLLoaderInterceptor::InterceptPrefetchedNavigation(
    const network::ResourceRequest& tenative_resource_request,
    base::WeakPtr<PrefetchContainer> prefetch_container) {
  if (cookie_copy_start_time_) {
    base::TimeDelta wait_time =
        base::TimeTicks::Now() - cookie_copy_start_time_.value();
    DCHECK_GT(wait_time, base::TimeDelta());
    RecordCookieWaitTime(wait_time);
  }

  if (!prefetch_container) {
    DoNotInterceptNavigation();
    return;
  }

  // This can only happen when probing is required and probing is successful.
  // `PrefetchURLLoaderInterceptor::InterceptPrefetchedNavigation` is reached
  // from a different code path, see
  // `PrefetchURLLoaderInterceptor::MaybeCreateLoader`.
  if (const auto status = prefetch_container->GetPrefetchStatus();
      status != PrefetchStatus::kPrefetchResponseUsed) {
    prefetch_container->SetPrefetchStatus(
        PrefetchStatus::kPrefetchResponseUsed);
  }

  // Set up URL loader that will serve the prefetched data, and URL loader
  // factory that will "create" this loader.
  scoped_refptr<network::SingleRequestURLLoaderFactory>
      single_request_url_loader_factory;
  std::unique_ptr<PrefetchFromStringURLLoader> url_loader;
  if (prefetch_container->GetStreamingLoader()) {
    // The streaming URL loader manages its own lifetime after this point. It
    // will delete itself once the prefetch response is completed and the
    // prefetched response is served.
    std::unique_ptr<PrefetchStreamingURLLoader> prefetch_streaming_url_loader =
        prefetch_container->ReleaseStreamingLoader();
    auto* raw_prefetch_streaming_url_loader =
        prefetch_streaming_url_loader.get();

    single_request_url_loader_factory =
        base::MakeRefCounted<network::SingleRequestURLLoaderFactory>(
            raw_prefetch_streaming_url_loader->ServingResponseHandler(
                std::move(prefetch_streaming_url_loader)));
  } else {
    url_loader = std::make_unique<PrefetchFromStringURLLoader>(
        prefetch_container->ReleasePrefetchedResponse(),
        prefetch_container->GetPrefetchResponseSizes(),
        tenative_resource_request);
    single_request_url_loader_factory =
        base::MakeRefCounted<network::SingleRequestURLLoaderFactory>(
            url_loader->ServingResponseHandler());
  }

  // Create URL loader factory pipe that can be possibly proxied by Extensions.
  mojo::PendingReceiver<network::mojom::URLLoaderFactory> pending_receiver;
  mojo::PendingRemote<network::mojom::URLLoaderFactory> pending_remote =
      pending_receiver.InitWithNewPipeAndPassRemote();

  // Call WillCreateURLLoaderFactory so that Extensions (and other features) can
  // proxy the URLLoaderFactory pipe.
  FrameTreeNode* frame_tree_node =
      FrameTreeNode::GloballyFindByID(frame_tree_node_id_);
  RenderFrameHost* render_frame_host = frame_tree_node->current_frame_host();
  NavigationRequest* navigation_request = frame_tree_node->navigation_request();
  bool bypass_redirect_checks = false;

  // TODO (https://crbug.com/1369766): Investigate if header_client param should
  // be non-null, and then how to utilize it.
  GetContentClient()->browser()->WillCreateURLLoaderFactory(
      BrowserContextFromFrameTreeNodeId(frame_tree_node_id_), render_frame_host,
      render_frame_host->GetProcess()->GetID(),
      ContentBrowserClient::URLLoaderFactoryType::kNavigation, url::Origin(),
      navigation_request->GetNavigationId(),
      ukm::SourceIdObj::FromInt64(navigation_request->GetNextPageUkmSourceId()),
      &pending_receiver, /*header_client=*/nullptr, &bypass_redirect_checks,
      /*disable_secure_dns=*/nullptr, /*factory_override=*/nullptr);

  // Bind the (possibly proxied) mojo pipe to the URL loader factory that will
  // serve the prefetched data.
  single_request_url_loader_factory->Clone(std::move(pending_receiver));

  // Wrap the other end of the mojo pipe and use it to intercept the navigation.
  std::move(loader_callback_)
      .Run(network::SharedURLLoaderFactory::Create(
          std::make_unique<network::WrapperPendingSharedURLLoaderFactory>(
              std::move(pending_remote))));

  // url_loader manages its own lifetime once bound to the mojo pipes.
  if (url_loader) {
    url_loader.release();
  }
}

void PrefetchURLLoaderInterceptor::DoNotInterceptNavigation() {
  std::move(loader_callback_).Run({});
}

}  // namespace content
