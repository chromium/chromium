// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/subresource_proxying_url_loader_service.h"

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "content/browser/loader/prefetch_url_loader_service_context.h"
#include "content/browser/loader/subresource_proxying_url_loader.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/web_package/prefetched_signed_exchange_cache.h"
#include "content/public/common/content_client.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/base/load_flags.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom-shared.h"

namespace content {

SubresourceProxyingURLLoaderService::BindContext::BindContext(
    FrameTreeNodeId frame_tree_node_id,
    scoped_refptr<network::SharedURLLoaderFactory> factory,
    base::WeakPtr<RenderFrameHostImpl> render_frame_host,
    scoped_refptr<PrefetchedSignedExchangeCache>
        prefetched_signed_exchange_cache)
    : frame_tree_node_id(frame_tree_node_id),
      factory(factory),
      render_frame_host(std::move(render_frame_host)),
      cross_origin_factory(nullptr),
      prefetched_signed_exchange_cache(
          std::move(prefetched_signed_exchange_cache)) {}

void SubresourceProxyingURLLoaderService::BindContext::OnDidCommitNavigation(
    WeakDocumentPtr committed_document) {
  document = committed_document;
}

SubresourceProxyingURLLoaderService::BindContext::~BindContext() = default;

SubresourceProxyingURLLoaderService::SubresourceProxyingURLLoaderService(
    BrowserContext* browser_context)
    : prefetch_url_loader_service_context_(
          std::make_unique<PrefetchURLLoaderServiceContext>(
              browser_context,
              loader_factory_receivers_)) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

base::WeakPtr<SubresourceProxyingURLLoaderService::BindContext>
SubresourceProxyingURLLoaderService::GetFactory(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver,
    FrameTreeNodeId frame_tree_node_id,
    scoped_refptr<network::SharedURLLoaderFactory>
        subresource_proxying_factory_bundle,
    base::WeakPtr<RenderFrameHostImpl> render_frame_host,
    scoped_refptr<PrefetchedSignedExchangeCache>
        prefetched_signed_exchange_cache) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto bind_context = base::MakeRefCounted<BindContext>(
      frame_tree_node_id, subresource_proxying_factory_bundle,
      std::move(render_frame_host),
      std::move(prefetched_signed_exchange_cache));

  base::WeakPtr<BindContext> weak_bind_context =
      bind_context->weak_ptr_factory.GetWeakPtr();

  loader_factory_receivers_.Add(this, std::move(receiver),
                                std::move(bind_context));

  return weak_bind_context;
}

void SubresourceProxyingURLLoaderService::CreateLoaderAndStart(
    mojo::PendingReceiver<network::mojom::URLLoader> receiver,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& resource_request_in,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!PrefetchURLLoaderServiceContext::IsPrefetchRequest(
          resource_request_in) &&
      !resource_request_in.browsing_topics &&
      !resource_request_in.ad_auction_headers) {
    loader_factory_receivers_.ReportBadMessage(
        "Unexpected `resource_request_in` in "
        "SubresourceProxyingURLLoaderService::CreateLoaderAndStart(): it's not "
        "a prefetch or browsing_topics or ad_auction_headers request.");
    return;
  }

  if (PrefetchURLLoaderServiceContext::IsPrefetchRequest(resource_request_in) &&
      (resource_request_in.browsing_topics ||
       resource_request_in.ad_auction_headers)) {
    loader_factory_receivers_.ReportBadMessage(
        "Unexpected `resource_request_in` in "
        "SubresourceProxyingURLLoaderService::CreateLoaderAndStart(): prefetch "
        "cannot be set at the same time with browsing_topics or "
        "ad_auction_headers.");
    return;
  }

  if (resource_request_in.browsing_topics &&
      !base::FeatureList::IsEnabled(blink::features::kBrowsingTopics)) {
    loader_factory_receivers_.ReportBadMessage(
        "Unexpected `resource_request_in` in "
        "SubresourceProxyingURLLoaderService::CreateLoaderAndStart(): "
        "browsing_topics is set when Topics API is disabled.");
    return;
  }

  if (resource_request_in.ad_auction_headers &&
      !base::FeatureList::IsEnabled(blink::features::kInterestGroupStorage)) {
    loader_factory_receivers_.ReportBadMessage(
        "Unexpected `resource_request_in` in "
        "SubresourceProxyingURLLoaderService::CreateLoaderAndStart(): "
        "ad_auction_headers is set when InterestGroupStorage is disabled.");
    return;
  }

  if (PrefetchURLLoaderServiceContext::IsPrefetchRequest(resource_request_in)) {
    prefetch_url_loader_service_context_->CreatePrefetchLoaderAndStart(
        std::move(receiver), request_id, options, resource_request_in,
        std::move(client), traffic_annotation);
    return;
  }

  // For non-prefetch requests, fall back to `SubresourceProxyingURLLoader`.
  CreateSubresourceProxyingLoaderAndStart(
      std::move(receiver), request_id, options, resource_request_in,
      std::move(client), traffic_annotation);
}

void SubresourceProxyingURLLoaderService::
    CreateSubresourceProxyingLoaderAndStart(
        mojo::PendingReceiver<network::mojom::URLLoader> receiver,
        int32_t request_id,
        uint32_t options,
        const network::ResourceRequest& resource_request_in,
        mojo::PendingRemote<network::mojom::URLLoaderClient> client,
        const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  BindContext* current_context =
      loader_factory_receivers_.current_context().get();

  auto loader = std::make_unique<SubresourceProxyingURLLoader>(
      current_context->document, request_id, options, resource_request_in,
      std::move(client), traffic_annotation, current_context->factory);

  auto* raw_loader = loader.get();

  subresource_proxying_loader_receivers_.Add(raw_loader, std::move(receiver),
                                             std::move(loader));
}

SubresourceProxyingURLLoaderService::~SubresourceProxyingURLLoaderService() =
    default;

void SubresourceProxyingURLLoaderService::Clone(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  loader_factory_receivers_.Add(this, std::move(receiver),
                                loader_factory_receivers_.current_context());
}

}  // namespace content
