// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_url_loader_interceptor.h"

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "content/browser/browser_context_impl.h"
#include "content/browser/loader/navigation_loader_interceptor.h"
#include "content/browser/preloading/prefetch/prefetch_features.h"
#include "content/browser/preloading/prefetch/prefetch_match_resolver.h"
#include "content/browser/preloading/prefetch/prefetch_service.h"
#include "content/browser/preloading/prefetch/prefetch_url_loader_helper.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/public/browser/web_contents.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/single_request_url_loader_factory.h"
#include "services/network/public/cpp/wrapper_shared_url_loader_factory.h"

namespace content {
namespace {

BrowserContext* BrowserContextFromFrameTreeNodeId(int frame_tree_node_id) {
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

}  // namespace

// static
std::unique_ptr<PrefetchURLLoaderInterceptor>
PrefetchURLLoaderInterceptor::MaybeCreateInterceptor(
    int frame_tree_node_id,
    const GlobalRenderFrameHostId& referring_render_frame_host_id) {
  if (!referring_render_frame_host_id) {
    // This is expected to occur only in unit tests.
    return nullptr;
  }

  return std::make_unique<PrefetchURLLoaderInterceptor>(
      frame_tree_node_id, referring_render_frame_host_id);
}

PrefetchURLLoaderInterceptor::PrefetchURLLoaderInterceptor(
    int frame_tree_node_id,
    const GlobalRenderFrameHostId& referring_render_frame_host_id)
    : frame_tree_node_id_(frame_tree_node_id),
      referring_render_frame_host_id_(referring_render_frame_host_id) {
  CHECK(referring_render_frame_host_id_);
}

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
    OnGotPrefetchToServe(
        frame_tree_node_id_, tentative_resource_request,
        base::BindOnce(&PrefetchURLLoaderInterceptor::OnGetPrefetchComplete,
                       weak_factory_.GetWeakPtr()),
        std::move(redirect_reader_));
    return;
  }

  if (redirect_reader_) {
    RecordWasFullRedirectChainServedHistogram(false);
    redirect_reader_ = PrefetchContainer::Reader();
  }

  FrameTreeNode* frame_tree_node =
      FrameTreeNode::GloballyFindByID(frame_tree_node_id_);
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

  prefetch_match_resolver.SetOnPrefetchToServeReadyCallback(base::BindOnce(
      &OnGotPrefetchToServe, frame_tree_node_id_, tentative_resource_request,
      std::move(get_prefetch_callback)));

  prefetch_service->GetPrefetchToServe(
      PrefetchContainer::Key(referring_render_frame_host_id_,
                             tentative_resource_request.url),
      prefetch_match_resolver);
}

void PrefetchURLLoaderInterceptor::OnGetPrefetchComplete(
    PrefetchContainer::Reader reader) {
  if (!reader) {
    // Do not intercept the request.
    redirect_reader_ = PrefetchContainer::Reader();
    std::move(loader_callback_).Run({});
    return;
  }

  auto request_handler = reader.CreateRequestHandler();
  if (!request_handler) {
    redirect_reader_ = PrefetchContainer::Reader();
    std::move(loader_callback_).Run({});
    return;
  }

  scoped_refptr<network::SingleRequestURLLoaderFactory>
      single_request_url_loader_factory =
          base::MakeRefCounted<network::SingleRequestURLLoaderFactory>(
              std::move(request_handler));

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
      /*disable_secure_dns=*/nullptr, /*factory_override=*/nullptr,
      /*navigation_response_task_runner=*/nullptr);

  // Bind the (possibly proxied) mojo pipe to the URL loader factory that will
  // serve the prefetched data.
  single_request_url_loader_factory->Clone(std::move(pending_receiver));

  // Wrap the other end of the mojo pipe and use it to intercept the navigation.
  std::move(loader_callback_)
      .Run(network::SharedURLLoaderFactory::Create(
          std::make_unique<network::WrapperPendingSharedURLLoaderFactory>(
              std::move(pending_remote))));
}

}  // namespace content
