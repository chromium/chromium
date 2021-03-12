// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_APPCACHE_APPCACHE_REQUEST_HANDLER_H_
#define CONTENT_BROWSER_APPCACHE_APPCACHE_REQUEST_HANDLER_H_

#include <stdint.h>

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/appcache/appcache_entry.h"
#include "content/browser/appcache/appcache_host.h"
#include "content/browser/appcache/appcache_service_impl.h"
#include "content/browser/loader/navigation_loader_interceptor.h"
#include "content/browser/loader/single_request_url_loader_factory.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom-shared.h"

namespace net {
class NetworkDelegate;
class URLRequest;
}  // namespace net

namespace content {
class AppCacheURLLoader;
class AppCacheSubresourceURLFactory;
class AppCacheRequest;
class AppCacheRequestHandlerTest;
class AppCacheHost;

CONTENT_EXPORT extern const base::Feature kAppCacheAlwaysFallbackToNetwork;

// An instance is created for each net::URLRequest. The instance survives all
// http transactions involved in the processing of its net::URLRequest, and is
// given the opportunity to hijack the request along the way. Callers
// should use AppCacheHost::CreateRequestHandler to manufacture instances
// that can retrieve resources for a particular host.
class CONTENT_EXPORT AppCacheRequestHandler
    : public AppCacheHost::Observer,
      public AppCacheServiceImpl::Observer,
      public AppCacheStorage::Delegate,
      public NavigationLoaderInterceptor {
 public:
  using AppCacheLoaderCallback =
      base::OnceCallback<void(SingleRequestURLLoaderFactory::RequestHandler)>;

  ~AppCacheRequestHandler() override;

  // NetworkService loading

  // NavigationLoaderInterceptor overrides - main resource loading.
  // These methods are used by the NavigationURLLoaderImpl.
  // Internally they use MaybeLoadResource and MaybeLoadFallbackForResponse.
  // Eventually one of the Deliver*Response() methods is called and the
  // LoaderCallback is invoked.
  void MaybeCreateLoader(
      const network::ResourceRequest& tentative_resource_request,
      BrowserContext* browser_context,
      LoaderCallback callback,
      FallbackCallback fallback_callback) override;
  // MaybeCreateLoaderForResponse always returns synchronously.
  bool MaybeCreateLoaderForResponse(
      const network::ResourceRequest& request,
      network::mojom::URLResponseHeadPtr* response_head,
      mojo::ScopedDataPipeConsumerHandle* response_body,
      mojo::PendingRemote<network::mojom::URLLoader>* loader,
      mojo::PendingReceiver<network::mojom::URLLoaderClient>* client_receiver,
      blink::ThrottlingURLLoader* url_loader,
      bool* skip_other_interceptors,
      bool* will_return_unsafe_redirect) override;
  base::Optional<SubresourceLoaderParams> MaybeCreateSubresourceLoaderParams()
      override;

  // These methods are used for subresource loading by the
  // AppCacheSubresourceURLFactory::SubresourceLoader class.
  // Internally they use MaybeLoadResource, MaybeLoadFallbackForResponse, and
  // MaybeLoadFallbackForRedirect. Eventually one of the Deliver*Response()
  // methods is called and the LoaderCallback is invoked.
  void MaybeCreateSubresourceLoader(
      const network::ResourceRequest& resource_request,
      AppCacheLoaderCallback callback);
  void MaybeFallbackForSubresourceResponse(
      network::mojom::URLResponseHeadPtr response,
      AppCacheLoaderCallback callback);
  void MaybeFallbackForSubresourceRedirect(
      const net::RedirectInfo& redirect_info,
      AppCacheLoaderCallback callback);
  void MaybeFollowSubresourceRedirect(const net::RedirectInfo& redirect_info,
                                      AppCacheLoaderCallback callback);

  static std::unique_ptr<AppCacheRequestHandler>
  InitializeForMainResourceNetworkService(
      const network::ResourceRequest& request,
      base::WeakPtr<AppCacheHost> appcache_host,
      int frame_tree_node_id);

  static bool IsMainRequestDestination(
      network::mojom::RequestDestination destination);

  // Called by unittests to indicate that we are in test mode.
  static void SetRunningInTests(bool in_tests);
  static bool IsRunningInTests();

 private:
  friend class AppCacheHost;
  friend class AppCacheRequestHandlerTest;

  // Callers should use AppCacheHost::CreateRequestHandler.
  AppCacheRequestHandler(AppCacheHost* host,
                         network::mojom::RequestDestination request_destination,
                         bool should_reset_appcache,
                         std::unique_ptr<AppCacheRequest> request,
                         int frame_tree_node_id);

  void MaybeCreateLoaderInternal(
      const network::ResourceRequest& resource_request,
      AppCacheLoaderCallback callback);

  // AppCacheHost::Observer.
  void OnDestructionImminent(AppCacheHost* host) override;

  // AppCacheServiceImpl::Observer.
  void OnServiceDestructionImminent(AppCacheServiceImpl* service) override;

  // Helpers to instruct a waiting job with what response to
  // deliver for the request we're handling.
  void DeliverAppCachedResponse(const AppCacheEntry& entry,
                                int64_t cache_id,
                                const GURL& manifest_url,
                                bool is_fallback,
                                const GURL& namespace_entry_url);
  void DeliverNetworkResponse();
  void DeliverErrorResponse();

  // Helper method to create an AppCacheURLLoader and populate job_.
  // Caller takes ownership of returned value.
  std::unique_ptr<AppCacheURLLoader> CreateLoader(
      net::NetworkDelegate* network_delegate);

  // Helper to retrieve a pointer to the storage object.
  AppCacheStorage* storage() const;

  bool is_main_resource() const {
    return IsMainRequestDestination(request_destination_);
  }

  // These are called on each request intercept opportunity, by the various
  // MaybeCreateLoader methods in the public API.
  AppCacheURLLoader* MaybeLoadResource(net::NetworkDelegate* network_delegate);
  AppCacheURLLoader* MaybeLoadFallbackForRedirect(
      net::NetworkDelegate* network_delegate,
      const GURL& location);
  AppCacheURLLoader* MaybeLoadFallbackForResponse(
      net::NetworkDelegate* network_delegate);

  void GetExtraResponseInfo(int64_t* cache_id, GURL* manifest_url);

  // Main-resource loading -------------------------------------
  // Frame and SharedWorker main resources are handled here.

  std::unique_ptr<AppCacheURLLoader> MaybeLoadMainResource(
      net::NetworkDelegate* network_delegate);

  // AppCacheStorage::Delegate methods
  void OnMainResponseFound(const GURL& url,
                           const AppCacheEntry& entry,
                           const GURL& fallback_url,
                           const AppCacheEntry& fallback_entry,
                           int64_t cache_id,
                           int64_t group_id,
                           const GURL& mainfest_url) override;

  // NetworkService loading:
  // Called when a |callback| that is originally given to |MaybeCreateLoader()|
  // runs for the main resource. This flips |should_create_subresource_loader_|
  // if a non-null |handler| is given. Always invokes |callback| with |handler|.
  void RunLoaderCallbackForMainResource(
      BrowserContext* browser_context,
      LoaderCallback callback,
      SingleRequestURLLoaderFactory::RequestHandler handler);

  // Sub-resource loading -------------------------------------
  // Dedicated worker and all manner of sub-resources are handled here.

  std::unique_ptr<AppCacheURLLoader> MaybeLoadSubResource(
      net::NetworkDelegate* network_delegate);
  void ContinueMaybeLoadSubResource();

  // AppCacheHost::Observer override
  void OnCacheSelectionComplete(AppCacheHost* host) override;

  // Data members -----------------------------------------------

  // What host we're servicing a request for.
  AppCacheHost* host_;

  // Frame vs subresource vs sharedworker loads are somewhat different.
  network::mojom::RequestDestination request_destination_;

  // True if corresponding AppCache group should be resetted before load.
  bool should_reset_appcache_;

  // Subresource requests wait until after cache selection completes.
  bool is_waiting_for_cache_selection_;

  // Info about the type of response we found for delivery.
  // These are relevant for both main and subresource requests.
  int64_t found_group_id_;
  int64_t found_cache_id_;
  AppCacheEntry found_entry_;
  AppCacheEntry found_fallback_entry_;
  GURL found_namespace_entry_url_;
  GURL found_manifest_url_;
  bool found_network_namespace_;

  // True if a cache entry this handler attempted to return was
  // not found in the disk cache. Once set, the handler will take
  // no action on all subsequent intercept opportunities, so the
  // request and any redirects will be handled by the network library.
  bool cache_entry_not_found_;

  // True if the next time this request is started, the response should be
  // delivered from the network, bypassing the AppCache. Cleared after the next
  // intercept opportunity.
  bool is_delivering_network_response_;

  // True if this->MaybeLoadResource(...) has been called in the past.
  bool maybe_load_resource_executed_;

  // The job we use to deliver a response. Only NULL during the following times:
  // 1) Before request has started a job.
  // 2) Request is not being handled by appcache.
  // 3) Request has been cancelled, and the job killed.
  base::WeakPtr<AppCacheURLLoader> loader_;

  // Cached information about the response being currently served by the
  // AppCache, if there is one.
  int cache_id_;
  GURL manifest_url_;

  // Backptr to the central service object.
  AppCacheServiceImpl* service_;

  std::unique_ptr<AppCacheRequest> request_;

  // Network service related members.

  AppCacheLoaderCallback loader_callback_;

  // Flipped to true if AppCache wants to handle subresource requests
  // (i.e. when |loader_callback_| is fired with a non-null
  // RequestHandler for non-error cases.
  bool should_create_subresource_loader_ = false;

  int frame_tree_node_id_ = FrameTreeNode::kFrameTreeNodeInvalidId;

  // The AppCache host instance. We pass this to the
  // AppCacheSubresourceURLFactory instance on creation.
  base::WeakPtr<AppCacheHost> appcache_host_;

  base::WeakPtrFactory<AppCacheRequestHandler> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AppCacheRequestHandler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_APPCACHE_APPCACHE_REQUEST_HANDLER_H_
