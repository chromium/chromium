// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_url_loader_interceptor.h"

#include "base/debug/dump_without_crashing.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "content/browser/loader/navigation_loader_interceptor.h"
#include "content/browser/loader/url_loader_factory_utils.h"
#include "content/browser/preloading/prefetch/prefetch_features.h"
#include "content/browser/preloading/prefetch/prefetch_match_resolver.h"
#include "content/browser/preloading/prefetch/prefetch_params.h"
#include "content/browser/preloading/prefetch/prefetch_service.h"
#include "content/browser/preloading/prefetch/prefetch_url_loader_helper.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/service_worker/service_worker_main_resource_handle.h"
#include "content/public/browser/web_contents.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/single_request_url_loader_factory.h"

namespace content {
namespace {

BrowserContext* BrowserContextFromFrameTreeNodeId(
    FrameTreeNodeId frame_tree_node_id) {
  WebContents* web_content =
      WebContents::FromFrameTreeNodeId(frame_tree_node_id);
  if (!web_content)
    return nullptr;
  return web_content->GetBrowserContext();
}

void RecordWasFullRedirectChainServedHistogram(
    bool was_full_redirect_chain_served) {
  UMA_HISTOGRAM_BOOLEAN("PrefetchProxy.AfterClick.WasFullRedirectChainServed",
                        was_full_redirect_chain_served);
}

PrefetchCompleteCallbackForTesting& GetPrefetchCompleteCallbackForTesting() {
  static base::NoDestructor<PrefetchCompleteCallbackForTesting>
      get_prefetch_complete_callback_for_testing;
  return *get_prefetch_complete_callback_for_testing;
}

}  // namespace

// static
void PrefetchURLLoaderInterceptor::SetPrefetchCompleteCallbackForTesting(
    PrefetchCompleteCallbackForTesting callback) {
  GetPrefetchCompleteCallbackForTesting() = std::move(callback);  // IN-TEST
}

PrefetchURLLoaderInterceptor::PrefetchURLLoaderInterceptor(
    PrefetchServiceWorkerState expected_service_worker_state,
    base::WeakPtr<ServiceWorkerMainResourceHandle>
        service_worker_handle_for_navigation,
    FrameTreeNodeId frame_tree_node_id,
    std::optional<blink::DocumentToken> initiator_document_token,
    base::WeakPtr<PrefetchServingPageMetricsContainer>
        serving_page_metrics_container)
    : expected_service_worker_state_(expected_service_worker_state),
      service_worker_handle_for_navigation_(
          std::move(service_worker_handle_for_navigation)),
      frame_tree_node_id_(frame_tree_node_id),
      initiator_document_token_(std::move(initiator_document_token)),
      serving_page_metrics_container_(
          std::move(serving_page_metrics_container)) {
  if (!features::IsPrefetchServiceWorkerEnabled(
          BrowserContextFromFrameTreeNodeId(frame_tree_node_id_))) {
    CHECK_EQ(expected_service_worker_state_,
             PrefetchServiceWorkerState::kDisallowed);
  }
}

PrefetchURLLoaderInterceptor::~PrefetchURLLoaderInterceptor() = default;

void PrefetchURLLoaderInterceptor::MaybeCreateLoader(
    const network::ResourceRequest& tentative_resource_request,
    BrowserContext* browser_context,
    NavigationLoaderInterceptor::LoaderCallback callback,
    NavigationLoaderInterceptor::FallbackCallback fallback_callback) {
  TRACE_EVENT_BEGIN("loading",
                    "PrefetchURLLoaderInterceptor::MaybeCreateLoader",
                    perfetto::Flow::FromPointer(this));
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CHECK(!loader_callback_);
  loader_callback_ = std::move(callback);

  // Prefetches are only ever used to fulfill `GET` requests. spec: if
  // documentResource is null,
  // https://wicg.github.io/nav-speculation/prefetch.html#create-navigation-params-from-a-prefetch-record
  // etc. are not called.
  if (tentative_resource_request.method !=
      net::HttpRequestHeaders::kGetMethod) {
    redirect_serving_handle_ = PrefetchServingHandle();
    TRACE_EVENT_END("loading");
    std::move(loader_callback_).Run(std::nullopt);
    return;
  }

  // SW-controlled prefetches shouldn't serve navigation with
  // `skip_service_worker` == `true`.
  // TODO(https://crbug.com/438478667): The current serving-time
  // `skip_service_worker` check here assumes prefetching-time
  // `skip_service_worker` is always false (see the
  // `CHECK(!skip_service_worker)` in
  // `PrefetchContainer::MakeResourceRequest()`). We should revisit the check
  // when we support prefetch-time `skip_service_worker`. Probably a prefetch
  // whose request's `skip_service_worker` == `true` shouldn't serve navigation
  // whose request's `skip_service_worker` == `false`.
  if (tentative_resource_request.skip_service_worker &&
      expected_service_worker_state_ ==
          PrefetchServiceWorkerState::kControlled) {
    redirect_serving_handle_ = PrefetchServingHandle();
    TRACE_EVENT_END("loading");
    std::move(loader_callback_).Run(std::nullopt);
    return;
  }

  if (redirect_serving_handle_ &&
      redirect_serving_handle_.DoesCurrentURLToServeMatch(
          tentative_resource_request.url)) {
    if (redirect_serving_handle_.HaveDefaultContextCookiesChanged()) {
      // Cookies have changed for the next redirect hop's URL since the fetch,
      // so we cannot use this prefetch anymore.
      PrefetchContainer* prefetch_container =
          redirect_serving_handle_.GetPrefetchContainer();
      CHECK(prefetch_container);
      // Use `std::nullopt` as we need to record the crash key to identify
      // which case in `PrefetchMatchResolver` is the cause.
      prefetch_container->OnDetectedCookiesChange(
          /*is_unblock_for_cookies_changed_triggered_by_this_prefetch_container*/
          std::nullopt);
    } else {
      TRACE_EVENT_END("loading");
      OnGotPrefetchToServe(
          frame_tree_node_id_, tentative_resource_request.url,
          base::BindOnce(&PrefetchURLLoaderInterceptor::OnGetPrefetchComplete,
                         weak_factory_.GetWeakPtr(),

                         tentative_resource_request.url,
                         ServiceWorkerMainResourceHandle::
                             TopFrameOriginForInitializeForRequest(
                                 tentative_resource_request)),
          std::move(redirect_serving_handle_));
      return;
    }
  }

  if (redirect_serving_handle_) {
    RecordWasFullRedirectChainServedHistogram(false);
    redirect_serving_handle_ = PrefetchServingHandle();
  }

  FrameTreeNode* frame_tree_node =
      FrameTreeNode::GloballyFindByID(frame_tree_node_id_);
  if (!frame_tree_node->IsOutermostMainFrame()) {
    // The prefetch code does not currently deal with prefetching within a frame
    // (i.e., where the partition which should be assigned to the request is not
    // the same as the partition belonging to its site at the top level).
    //
    // This could be made smarter in the future (to do those prefetches within
    // the right partition, or at minimum to use it from that partition if they
    // happen to be the same, i.e., the URL remains within the same site as the
    // top-level document).
    TRACE_EVENT_END("loading");
    std::move(loader_callback_).Run(std::nullopt);
    return;
  }

  TRACE_EVENT_END("loading");
  GetPrefetch(
      tentative_resource_request.url,
      base::BindOnce(&PrefetchURLLoaderInterceptor::OnGetPrefetchComplete,
                     weak_factory_.GetWeakPtr(), tentative_resource_request.url,

                     ServiceWorkerMainResourceHandle::
                         TopFrameOriginForInitializeForRequest(
                             tentative_resource_request)));
}

void PrefetchURLLoaderInterceptor::GetPrefetch(
    const GURL& url,
    base::OnceCallback<void(PrefetchServingHandle)> get_prefetch_callback)
    const {
  TRACE_EVENT_BEGIN("loading", "PrefetchURLLoaderInterceptor::GetPrefetch",
                    perfetto::Flow::FromPointer(
                        const_cast<PrefetchURLLoaderInterceptor*>(this)));

  PrefetchService* prefetch_service =
      PrefetchService::GetFromFrameTreeNodeId(frame_tree_node_id_);
  if (!prefetch_service) {
    TRACE_EVENT_END("loading");
    std::move(get_prefetch_callback).Run({});
    return;
  }

  if (!initiator_document_token_.has_value()) {
    // TODO(crbug.com/40288091): Currently PrefetchServingPageMetricsContainer
    // is created only when the navigation is renderer-initiated and its
    // initiator document has PrefetchDocumentManager.
    CHECK(!serving_page_metrics_container_);
  }

  auto callback = base::BindOnce(&OnGotPrefetchToServe, frame_tree_node_id_,
                                 url, std::move(get_prefetch_callback));
  auto key = PrefetchKey(initiator_document_token_, url);
  TRACE_EVENT_END("loading");
  PrefetchMatchResolver::FindPrefetch(
      frame_tree_node_id_, *prefetch_service, std::move(key),
      expected_service_worker_state_, serving_page_metrics_container_,
      std::move(callback),
      perfetto::Flow::FromPointer(
          const_cast<PrefetchURLLoaderInterceptor*>(this)));
}

void PrefetchURLLoaderInterceptor::OnGetPrefetchComplete(
    const GURL& url,
    const std::optional<url::Origin>& top_frame_origin,
    PrefetchServingHandle serving_handle) {
  TRACE_EVENT_BEGIN("loading",
                    "PrefetchURLLoaderInterceptor::OnGetPrefetchComplete",
                    perfetto::Flow::FromPointer(this));

  PrefetchRequestHandler request_handler;
  base::WeakPtr<ServiceWorkerClient> client_for_prefetch;
  if (serving_handle) {
    std::tie(request_handler, client_for_prefetch) =
        serving_handle.CreateRequestHandler();
  }

  if (expected_service_worker_state_ ==
          PrefetchServiceWorkerState::kControlled &&
      request_handler) {
    // ServiceWorker-controlled prefetch should be always non-redirecting.
    CHECK(serving_handle.IsEnd());

    if (!service_worker_handle_for_navigation_ || !client_for_prefetch) {
      // Do not intercept the request.
      request_handler = PrefetchRequestHandler();
    } else if (!service_worker_handle_for_navigation_->InitializeForRequest(
                   url, top_frame_origin, client_for_prefetch.get())) {
      // Make tests fail and report in production builds when
      // `InitializeForRequest()` should fail, i.e. when top frame origin or
      // storage key used for `client_for_prefetch` is wrong/mismatching.
      // TODO(https://crbug.com/413207408): Monitor the reports and fix
      // `ServiceWorkerClient::CalculateStorageKeyForUpdateUrls()` if there
      // are actual mismatches.
      DCHECK(false);
      base::debug::DumpWithoutCrashing();

      // We anyway gracefully fallback to non-prefetch path.
      // Do not intercept the request.
      request_handler = PrefetchRequestHandler();
    }
  }

  if (!request_handler) {
    // Do not intercept the request.
    redirect_serving_handle_ = PrefetchServingHandle();
    if (GetPrefetchCompleteCallbackForTesting()) {
      GetPrefetchCompleteCallbackForTesting().Run(nullptr);  // IN-TEST
    }
    TRACE_EVENT_END("loading");
    std::move(loader_callback_).Run(std::nullopt);
    return;
  }

  scoped_refptr<network::SingleRequestURLLoaderFactory>
      single_request_url_loader_factory =
          base::MakeRefCounted<network::SingleRequestURLLoaderFactory>(
              std::move(request_handler));

  PrefetchContainer* prefetch_container = serving_handle.GetPrefetchContainer();

  // If |prefetch_container| is done serving the prefetch, clear out
  // |redirect_serving_handle_|, but otherwise cache it in
  // |redirect_serving_handle_|.
  if (serving_handle.IsEnd()) {
    if (redirect_serving_handle_) {
      RecordWasFullRedirectChainServedHistogram(true);
    }
    redirect_serving_handle_ = PrefetchServingHandle();
  } else {
    CHECK_EQ(expected_service_worker_state_,
             PrefetchServiceWorkerState::kDisallowed);
    redirect_serving_handle_ = std::move(serving_handle);
  }

  FrameTreeNode* frame_tree_node =
      FrameTreeNode::GloballyFindByID(frame_tree_node_id_);
  RenderFrameHost* render_frame_host = frame_tree_node->current_frame_host();
  NavigationRequest* navigation_request = frame_tree_node->navigation_request();
  bool bypass_redirect_checks = false;

  if (GetPrefetchCompleteCallbackForTesting()) {
    GetPrefetchCompleteCallbackForTesting().Run(prefetch_container);  // IN-TEST
  }

  // TODO (https://crbug.com/1369766): Investigate if
  // `HeaderClientOption::kAllowed` should be used for `TerminalParams`, and
  // then how to utilize it.
  TRACE_EVENT_END("loading");
  std::move(loader_callback_)
      .Run(NavigationLoaderInterceptor::Result(
          url_loader_factory::Create(
              ContentBrowserClient::URLLoaderFactoryType::kNavigation,
              url_loader_factory::TerminalParams::ForNonNetwork(
                  std::move(single_request_url_loader_factory),
                  network::mojom::kBrowserProcessId),
              url_loader_factory::ContentClientParams(
                  BrowserContextFromFrameTreeNodeId(frame_tree_node_id_),
                  render_frame_host,
                  render_frame_host->GetProcess()->GetDeprecatedID(),
                  url::Origin(), net::IsolationInfo(),
                  ukm::SourceIdObj::FromInt64(
                      navigation_request->GetNextPageUkmSourceId()),
                  &bypass_redirect_checks,
                  navigation_request->GetNavigationId())),
          /*subresource_loader_params=*/{}));
}

}  // namespace content
