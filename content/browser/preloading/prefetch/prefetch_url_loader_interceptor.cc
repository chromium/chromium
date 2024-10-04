// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_url_loader_interceptor.h"

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "content/browser/browser_context_impl.h"
#include "content/browser/loader/navigation_loader_interceptor.h"
#include "content/browser/loader/url_loader_factory_utils.h"
#include "content/browser/preloading/prefetch/prefetch_features.h"
#include "content/browser/preloading/prefetch/prefetch_match_resolver.h"
#include "content/browser/preloading/prefetch/prefetch_params.h"
#include "content/browser/preloading/prefetch/prefetch_service.h"
#include "content/browser/preloading/prefetch/prefetch_serving_page_metrics_container.h"
#include "content/browser/preloading/prefetch/prefetch_url_loader_helper.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_request.h"
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
    FrameTreeNodeId frame_tree_node_id,
    std::optional<blink::DocumentToken> initiator_document_token,
    base::WeakPtr<PrefetchServingPageMetricsContainer>
        serving_page_metrics_container)
    : frame_tree_node_id_(frame_tree_node_id),
      initiator_document_token_(std::move(initiator_document_token)),
      serving_page_metrics_container_(
          std::move(serving_page_metrics_container)) {}

PrefetchURLLoaderInterceptor::~PrefetchURLLoaderInterceptor() = default;

void PrefetchURLLoaderInterceptor::MaybeCreateLoader(
    const network::ResourceRequest& tentative_resource_request,
    BrowserContext* browser_context,
    NavigationLoaderInterceptor::LoaderCallback callback,
    NavigationLoaderInterceptor::FallbackCallback fallback_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CHECK(!loader_callback_);
  loader_callback_ = std::move(callback);

  if (redirect_reader_ && redirect_reader_.DoesCurrentURLToServeMatch(
                              tentative_resource_request.url)) {
    if (redirect_reader_.HaveDefaultContextCookiesChanged()) {
      // Cookies have changed for the next redirect hop's URL since the fetch,
      // so we cannot use this prefetch anymore.
      PrefetchContainer* prefetch_container =
          redirect_reader_.GetPrefetchContainer();
      CHECK(prefetch_container);
      if (UseNewWaitLoop()) {
        prefetch_container->OnDetectedCookiesChange2();
      } else {
        // Note: This method can only be called once per PrefetchContainer (we
        // have a CHECK in the method). This is guaranteed to be the first time
        // we call this method for |prefetch_container|, as the other callsite
        // (in PrefetchService::ReturnPrefetchToServe) would have prevented the
        // prefetch from being used to serve the navigation (making this
        // unreachable as |redirect_reader_| would never have been set to
        // |prefetch_container|). This will also never be called for
        // |prefetch_container| again as we don't use it to serve any subsequent
        // redirect hops for this navigation (we unset |redirect_reader_|
        // below), and
        // |PrefetchService::CollectMatchCandidates|
        // ignores any prefetches with the status kPrefetchNotUsedCookiesChanged
        // (which is set in |PrefetchContainer::OnDetectedCookiesChange|).
        prefetch_container->OnDetectedCookiesChange();
      }
    } else {
      OnGotPrefetchToServe(
          frame_tree_node_id_, tentative_resource_request,
          base::BindOnce(&PrefetchURLLoaderInterceptor::OnGetPrefetchComplete,
                         weak_factory_.GetWeakPtr()),
          std::move(redirect_reader_));
      return;
    }
  }

  if (redirect_reader_) {
    RecordWasFullRedirectChainServedHistogram(false);
    redirect_reader_ = PrefetchContainer::Reader();
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
    std::move(loader_callback_).Run(std::nullopt);
    return;
  }

  // During the lifetime of the PrefetchUrlLoaderInterceptor there is only one
  // cross-document navigation waiting for its final response.
  // We only need to worry about one active navigation while trying to match
  // prefetch_container in PrefetchService.
  // This navigation is represented by `prefetch_match_resolver`.
  // See documentation here as why this is true:
  // https://chromium.googlesource.com/chromium/src/+/main/docs/navigation_concepts.md#concurrent-navigations
  NavigationRequest* navigation_request = frame_tree_node->navigation_request();
  PrefetchMatchResolver::CreateForNavigationHandle(*navigation_request);
  PrefetchMatchResolver* prefetch_match_resolver =
      PrefetchMatchResolver::GetForNavigationHandle(*navigation_request);
  CHECK(prefetch_match_resolver);

  GetPrefetch(
      tentative_resource_request, *prefetch_match_resolver,
      base::BindOnce(&PrefetchURLLoaderInterceptor::OnGetPrefetchComplete,
                     weak_factory_.GetWeakPtr()));
}

void PrefetchURLLoaderInterceptor::GetPrefetch(
    const network::ResourceRequest& tentative_resource_request,
    PrefetchMatchResolver& prefetch_match_resolver,
    base::OnceCallback<void(PrefetchContainer::Reader)> get_prefetch_callback)
    const {
  PrefetchService* prefetch_service =
      PrefetchService::GetFromFrameTreeNodeId(frame_tree_node_id_);
  if (!prefetch_service) {
    std::move(get_prefetch_callback).Run({});
    return;
  }

  if (!initiator_document_token_.has_value()) {
    if (!PrefetchBrowserInitiatedTriggersEnabled()) {
      std::move(get_prefetch_callback).Run({});
      return;
    }

    // TODO(crbug.com/40288091): Currently PrefetchServingPageMetricsContainer
    // is created only when the navigation is renderer-initiated and its
    // initiator document has PrefetchDocumentManager.
    CHECK(!serving_page_metrics_container_);
  }

  auto callback = base::BindOnce(&OnGotPrefetchToServe, frame_tree_node_id_,
                                 tentative_resource_request,
                                 std::move(get_prefetch_callback));
  auto key = PrefetchContainer::Key(initiator_document_token_,
                                    tentative_resource_request.url);
  if (UseNewWaitLoop()) {
    PrefetchMatchResolver2::FindPrefetch(std::move(key), *prefetch_service,
                                         serving_page_metrics_container_,
                                         std::move(callback));
  } else {
    prefetch_match_resolver.SetOnPrefetchToServeReadyCallback(
        std::move(callback));
    prefetch_service->GetPrefetchToServe(std::move(key),
                                         serving_page_metrics_container_,
                                         prefetch_match_resolver);
  }
}

void PrefetchURLLoaderInterceptor::OnGetPrefetchComplete(
    PrefetchContainer::Reader reader) {
  PrefetchRequestHandler request_handler;
  if (!reader || !(request_handler = reader.CreateRequestHandler())) {
    // Do not intercept the request.
    redirect_reader_ = PrefetchContainer::Reader();
    std::move(loader_callback_).Run(std::nullopt);
    if (GetPrefetchCompleteCallbackForTesting()) {
      GetPrefetchCompleteCallbackForTesting().Run(nullptr);  // IN-TEST
    }
    return;
  }

  scoped_refptr<network::SingleRequestURLLoaderFactory>
      single_request_url_loader_factory =
          base::MakeRefCounted<network::SingleRequestURLLoaderFactory>(
              std::move(request_handler));

  PrefetchContainer* prefetch_container = reader.GetPrefetchContainer();

  // If |prefetch_container| is done serving the prefetch, clear out
  // |redirect_reader_|, but otherwise cache it in |redirect_reader_|.
  if (reader.IsEnd()) {
    if (redirect_reader_) {
      RecordWasFullRedirectChainServedHistogram(true);
    }
    redirect_reader_ = PrefetchContainer::Reader();
  } else {
    redirect_reader_ = std::move(reader);
  }

  FrameTreeNode* frame_tree_node =
      FrameTreeNode::GloballyFindByID(frame_tree_node_id_);
  RenderFrameHost* render_frame_host = frame_tree_node->current_frame_host();
  NavigationRequest* navigation_request = frame_tree_node->navigation_request();
  bool bypass_redirect_checks = false;

  // TODO (https://crbug.com/1369766): Investigate if
  // `HeaderClientOption::kAllowed` should be used for `TerminalParams`, and
  // then how to utilize it.
  std::move(loader_callback_)
      .Run(NavigationLoaderInterceptor::Result(
          url_loader_factory::Create(
              ContentBrowserClient::URLLoaderFactoryType::kNavigation,
              url_loader_factory::TerminalParams::ForNonNetwork(
                  std::move(single_request_url_loader_factory),
                  network::mojom::kBrowserProcessId),
              url_loader_factory::ContentClientParams(
                  BrowserContextFromFrameTreeNodeId(frame_tree_node_id_),
                  render_frame_host, render_frame_host->GetProcess()->GetID(),
                  url::Origin(), net::IsolationInfo(),
                  ukm::SourceIdObj::FromInt64(
                      navigation_request->GetNextPageUkmSourceId()),
                  &bypass_redirect_checks,
                  navigation_request->GetNavigationId())),
          /*subresource_loader_params=*/{}));

  if (GetPrefetchCompleteCallbackForTesting()) {
    GetPrefetchCompleteCallbackForTesting().Run(prefetch_container);  // IN-TEST
  }
}

}  // namespace content
