// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_SUBRESOURCE_PROXYING_URL_LOADER_SERVICE_H_
#define CONTENT_BROWSER_LOADER_SUBRESOURCE_PROXYING_URL_LOADER_SERVICE_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "content/public/browser/weak_document_ptr.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "net/base/isolation_info.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "third_party/blink/public/common/loader/url_loader_factory_bundle.h"

namespace content {

class BrowserContext;
class PrefetchedSignedExchangeCache;
class PrefetchURLLoaderServiceContext;
class RenderFrameHostImpl;

// A URLLoaderFactory that can be passed to a renderer to intercept subresource
// requests.
//
// The renderer uses it for:
// - Prefetch requests including <link rel="prefetch">
// - Topics requests including fetch(<url>, {browsingTopics: true})
class CONTENT_EXPORT SubresourceProxyingURLLoaderService final
    : public network::mojom::URLLoaderFactory {
 public:
  struct CONTENT_EXPORT BindContext : public base::RefCounted<BindContext> {
    // `factory` is a clone of the default factory bundle for document
    // subresource requests.
    BindContext(FrameTreeNodeId frame_tree_node_id,
                scoped_refptr<network::SharedURLLoaderFactory> factory,
                base::WeakPtr<RenderFrameHostImpl> render_frame_host,
                scoped_refptr<PrefetchedSignedExchangeCache>
                    prefetched_signed_exchange_cache);

    // Set `document` to `committed_document`.
    void OnDidCommitNavigation(WeakDocumentPtr committed_document);

    const FrameTreeNodeId frame_tree_node_id;
    scoped_refptr<network::SharedURLLoaderFactory> factory;
    base::WeakPtr<RenderFrameHostImpl> render_frame_host;

    // This member is lazily initialized by EnsureCrossOriginFactory().
    scoped_refptr<network::SharedURLLoaderFactory> cross_origin_factory;

    scoped_refptr<PrefetchedSignedExchangeCache>
        prefetched_signed_exchange_cache;

    // This maps recursive prefetch tokens to IsolationInfos that they should be
    // fetched with.
    std::map<base::UnguessableToken, net::IsolationInfo>
        prefetch_isolation_infos;

    // Upon NavigationRequest::DidCommitNavigation(), `document` will be set to
    // the document that this `BindContext` is associated with. It will become
    // null whenever the document navigates away.
    WeakDocumentPtr document;

    // This must be the last member.
    base::WeakPtrFactory<SubresourceProxyingURLLoaderService::BindContext>
        weak_ptr_factory{this};

   private:
    ~BindContext();
    friend class base::RefCounted<BindContext>;
  };

  explicit SubresourceProxyingURLLoaderService(BrowserContext* browser_context);
  ~SubresourceProxyingURLLoaderService() override;

  SubresourceProxyingURLLoaderService(
      const SubresourceProxyingURLLoaderService&) = delete;
  SubresourceProxyingURLLoaderService& operator=(
      const SubresourceProxyingURLLoaderService&) = delete;

  base::WeakPtr<BindContext> GetFactory(
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver,
      FrameTreeNodeId frame_tree_node_id,
      scoped_refptr<network::SharedURLLoaderFactory>
          subresource_proxying_factory_bundle,
      base::WeakPtr<RenderFrameHostImpl> render_frame_host,
      scoped_refptr<PrefetchedSignedExchangeCache>
          prefetched_signed_exchange_cache);

  PrefetchURLLoaderServiceContext&
  prefetch_url_loader_service_context_for_testing() {
    return *prefetch_url_loader_service_context_;
  }

 private:
  // network::mojom::URLLoaderFactory:
  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> receiver,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& resource_request_in,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override;
  void Clone(mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver)
      override;

  void CreateSubresourceProxyingLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> receiver,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& resource_request_in,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation);

  mojo::ReceiverSet<network::mojom::URLLoaderFactory,
                    scoped_refptr<BindContext>>
      loader_factory_receivers_;

  mojo::ReceiverSet<network::mojom::URLLoader,
                    std::unique_ptr<network::mojom::URLLoader>>
      subresource_proxying_loader_receivers_;

  std::unique_ptr<PrefetchURLLoaderServiceContext>
      prefetch_url_loader_service_context_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_SUBRESOURCE_PROXYING_URL_LOADER_SERVICE_H_
