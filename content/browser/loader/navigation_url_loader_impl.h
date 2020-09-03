// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_NAVIGATION_URL_LOADER_IMPL_H_
#define CONTENT_BROWSER_LOADER_NAVIGATION_URL_LOADER_IMPL_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "content/browser/loader/navigation_url_loader.h"
#include "content/browser/loader/single_request_url_loader_factory.h"
#include "content/browser/navigation_subresource_loader_params.h"
#include "content/common/navigation_params.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/ssl_status.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/url_request/url_request.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/common/loader/previews_state.h"

namespace net {
struct RedirectInfo;
}

namespace content {

class BrowserContext;
class NavigationLoaderInterceptor;
class PrefetchedSignedExchangeCache;
class SignedExchangePrefetchMetricRecorder;
class SignedExchangeRequestHandler;
class StoragePartition;
class StoragePartitionImpl;
struct WebPluginInfo;

class CONTENT_EXPORT NavigationURLLoaderImpl
    : public NavigationURLLoader,
      public network::mojom::URLLoaderClient {
 public:
  // The caller is responsible for ensuring that |delegate| outlives the loader.
  // Note |initial_interceptors| is there for test purposes only.
  NavigationURLLoaderImpl(
      BrowserContext* browser_context,
      StoragePartition* storage_partition,
      std::unique_ptr<NavigationRequestInfo> request_info,
      std::unique_ptr<NavigationUIData> navigation_ui_data,
      ServiceWorkerMainResourceHandle* service_worker_handle,
      AppCacheNavigationHandle* appcache_handle,
      scoped_refptr<PrefetchedSignedExchangeCache>
          prefetched_signed_exchange_cache,
      NavigationURLLoaderDelegate* delegate,
      mojo::PendingRemote<network::mojom::CookieAccessObserver> cookie_observer,
      std::vector<std::unique_ptr<NavigationLoaderInterceptor>>
          initial_interceptors);
  ~NavigationURLLoaderImpl() override;

  // TODO(kinuko): Make most of these methods private.
  // TODO(kinuko): Some method parameters can probably be just kept as
  // member variables rather than being passed around.

  // Starts the loader by finalizing loader factories initialization and
  // calling Restart().
  // This is called only once (while Restart can be called multiple times).
  // Sets |started_| true.
  void Start(
      scoped_refptr<network::SharedURLLoaderFactory> network_loader_factory,
      AppCacheNavigationHandle* appcache_handle,
      scoped_refptr<PrefetchedSignedExchangeCache>
          prefetched_signed_exchange_cache,
      scoped_refptr<SignedExchangePrefetchMetricRecorder>
          signed_exchange_prefetch_metric_recorder,
      mojo::PendingRemote<network::mojom::URLLoaderFactory> factory_for_webui,
      std::string accept_langs,
      bool needs_loader_factory_interceptor);
  void CreateInterceptors(AppCacheNavigationHandle* appcache_handle,
                          scoped_refptr<PrefetchedSignedExchangeCache>
                              prefetched_signed_exchange_cache,
                          scoped_refptr<SignedExchangePrefetchMetricRecorder>
                              signed_exchange_prefetch_metric_recorder,
                          const std::string& accept_langs);

  // This could be called multiple times to follow a chain of redirects.
  void Restart();

  // |interceptor| is non-null if this is called by one of the interceptors
  // (via a LoaderCallback).
  // |single_request_handler| is the RequestHandler given by the
  // |interceptor|, non-null if the interceptor wants to handle the request.
  void MaybeStartLoader(
      NavigationLoaderInterceptor* interceptor,
      scoped_refptr<network::SharedURLLoaderFactory> single_request_factory);

  // This is the |fallback_callback| passed to
  // NavigationLoaderInterceptor::MaybeCreateLoader. It allows an interceptor
  // to initially elect to handle a request, and later decide to fallback to
  // the default behavior. This is needed for service worker network fallback
  // and signed exchange (SXG) fallback redirect.
  void FallbackToNonInterceptedRequest(bool reset_subresource_loader_params);

  scoped_refptr<network::SharedURLLoaderFactory>
  PrepareForNonInterceptedRequest(uint32_t* out_options);

  // TODO(kinuko): Merge this back to FollowRedirect().
  void FollowRedirectInternal(
      const std::vector<std::string>& removed_headers,
      const net::HttpRequestHeaders& modified_headers,
      const net::HttpRequestHeaders& modified_cors_exempt_headers,
      blink::PreviewsState new_previews_state,
      base::Time ui_post_time);

  // network::mojom::URLLoaderClient implementation:
  void OnReceiveResponse(network::mojom::URLResponseHeadPtr head) override;
  void OnStartLoadingResponseBody(
      mojo::ScopedDataPipeConsumerHandle response_body) override;
  void OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                         network::mojom::URLResponseHeadPtr head) override;
  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback callback) override;
  void OnReceiveCachedMetadata(mojo_base::BigBuffer data) override;
  void OnTransferSizeUpdated(int32_t transfer_size_diff) override {}
  void OnComplete(const network::URLLoaderCompletionStatus& status) override;

#if BUILDFLAG(ENABLE_PLUGINS)
  void CheckPluginAndContinueOnReceiveResponse(
      network::mojom::URLResponseHeadPtr head,
      network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
      bool is_download_if_not_handled_by_plugin,
      const std::vector<WebPluginInfo>& plugins);
#endif

  void CallOnReceivedResponse(
      network::mojom::URLResponseHeadPtr head,
      network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
      bool is_download);
  bool MaybeCreateLoaderForResponse(
      network::mojom::URLResponseHeadPtr* response);
  std::vector<std::unique_ptr<blink::URLLoaderThrottle>>
  CreateURLLoaderThrottles();
  std::unique_ptr<SignedExchangeRequestHandler>
  CreateSignedExchangeRequestHandler(
      const NavigationRequestInfo& request_info,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      scoped_refptr<SignedExchangePrefetchMetricRecorder>
          signed_exchange_prefetch_metric_recorder,
      std::string accept_langs);
  void ParseHeaders(const GURL& url,
                    network::mojom::URLResponseHead* head,
                    base::OnceClosure continuation);

  // NavigationURLLoader implementation:
  void FollowRedirect(
      const std::vector<std::string>& removed_headers,
      const net::HttpRequestHeaders& modified_headers,
      const net::HttpRequestHeaders& modified_cors_exempt_headers,
      blink::PreviewsState new_previews_state) override;

  void NotifyRequestStarted(base::TimeTicks timestamp);
  void NotifyResponseStarted(
      network::mojom::URLResponseHeadPtr response_head,
      network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
      mojo::ScopedDataPipeConsumerHandle response_body,
      const GlobalRequestID& global_request_id,
      bool is_download);
  void NotifyRequestRedirected(net::RedirectInfo redirect_info,
                               network::mojom::URLResponseHeadPtr response);
  void NotifyRequestFailed(const network::URLLoaderCompletionStatus& status);

  // Intercepts loading of frame requests when network service is enabled and
  // either a network::mojom::TrustedURLLoaderHeaderClient is being used or
  // for schemes not handled by network service (e.g. files). This must be
  // called on the UI thread or before threads start.
  using URLLoaderFactoryInterceptor = base::RepeatingCallback<void(
      mojo::PendingReceiver<network::mojom::URLLoaderFactory>* receiver)>;
  static void SetURLLoaderFactoryInterceptorForTesting(
      const URLLoaderFactoryInterceptor& interceptor);

  // Creates a URLLoaderFactory for a navigation. The factory uses
  // |header_client|. This should have the same settings as the factory from
  // the URLLoaderFactoryGetter. Called on the UI thread.
  static void CreateURLLoaderFactoryWithHeaderClient(
      mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>
          header_client,
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> factory_receiver,
      StoragePartitionImpl* partition);

 private:
  // TODO(kinuko): This can be a file-local private anonymous function.
  static uint32_t GetURLLoaderOptions(bool is_main_frame);

  void BindNonNetworkURLLoaderFactoryReceiver(
      const GURL& url,
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> factory_receiver);
  void BindAndInterceptNonNetworkURLLoaderFactoryReceiver(
      const GURL& url,
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> factory_receiver);

  NavigationURLLoaderDelegate* delegate_;
  BrowserContext* browser_context_;
  StoragePartitionImpl* storage_partition_;
  ServiceWorkerMainResourceHandle* service_worker_handle_;

  std::unique_ptr<network::ResourceRequest> resource_request_;
  std::unique_ptr<NavigationRequestInfo> request_info_;

  // Current URL that is being navigated, updated after redirection.
  GURL url_;

  // Redirect URL chain.
  std::vector<GURL> url_chain_;

  const int frame_tree_node_id_;
  const GlobalRequestID global_request_id_;
  net::RedirectInfo redirect_info_;
  int redirect_limit_ = net::URLRequest::kMaxRedirects;
  base::RepeatingCallback<WebContents*()> web_contents_getter_;
  std::unique_ptr<NavigationUIData> navigation_ui_data_;

  scoped_refptr<network::SharedURLLoaderFactory> network_loader_factory_;
  std::unique_ptr<blink::ThrottlingURLLoader> url_loader_;

  // Caches the modified request headers provided by clients during redirect,
  // will be consumed by next |url_loader_->FollowRedirect()|.
  std::vector<std::string> url_loader_removed_headers_;
  net::HttpRequestHeaders url_loader_modified_headers_;
  net::HttpRequestHeaders url_loader_modified_cors_exempt_headers_;

  // Currently used by the AppCache loader to pass its factory to the
  // renderer which enables it to handle subresources.
  base::Optional<SubresourceLoaderParams> subresource_loader_params_;

  std::vector<std::unique_ptr<NavigationLoaderInterceptor>> interceptors_;
  size_t interceptor_index_ = 0;

  // Set to true if the default URLLoader (network service) was used for the
  // current navigation.
  bool default_loader_used_ = false;

  // URLLoaderClient receiver for loaders created for responses received from
  // the network loader.
  mojo::Receiver<network::mojom::URLLoaderClient> response_loader_receiver_{
      this};

  // URLLoader instance for response loaders, i.e loaders created for handling
  // responses received from the network URLLoader.
  mojo::PendingRemote<network::mojom::URLLoader> response_url_loader_;

  // Set to true if we receive a valid response from a URLLoader, i.e.
  // URLLoaderClient::OnStartLoadingResponseBody() is called.
  bool received_response_ = false;

  // When URLLoaderClient::OnReceiveResponse() is called. For UMA.
  base::TimeTicks on_receive_response_time_;

  bool started_ = false;

  // The completion status if it has been received. This is needed to handle
  // the case that the response is intercepted by download, and OnComplete()
  // is already called while we are transferring the |url_loader_| and
  // response body to download code.
  base::Optional<network::URLLoaderCompletionStatus> status_;

  // Before creating this URLLoaderRequestController on UI thread, the
  // embedder may have elected to proxy the URLLoaderFactory receiver, in
  // which case these fields will contain input (remote) and output (receiver)
  // endpoints for the proxy. If this controller is handling a receiver for
  // which proxying is supported, receivers will be plumbed through these
  // endpoints.
  //
  // Note that these are only used for receivers that go to the Network
  // Service.
  mojo::PendingReceiver<network::mojom::URLLoaderFactory>
      proxied_factory_receiver_;
  mojo::PendingRemote<network::mojom::URLLoaderFactory> proxied_factory_remote_;

  // The schemes that this loader can use. For anything else we'll try
  // external protocol handlers.
  std::set<std::string> known_schemes_;

  // True when a proxy will handle the redirect checks, or when an interceptor
  // intentionally returned unsafe redirect response
  // (eg: NavigationLoaderInterceptor for loading a local Web Bundle file).
  bool bypass_redirect_checks_ = false;

  network::mojom::URLResponseHeadPtr head_;
  mojo::ScopedDataPipeConsumerHandle response_body_;

  NavigationDownloadPolicy download_policy_;

  // Factories to handle navigation requests for non-network resources.
  ContentBrowserClient::NonNetworkURLLoaderFactoryMap
      non_network_url_loader_factories_;

  // Like |non_network_url_loader_factories_|, but with factories owned by
  // |this| NavigationURLLoaderImpl. (This ownership mode is deprecated - see
  // https://crbug.com/1106995)
  ContentBrowserClient::NonNetworkURLLoaderFactoryDeprecatedMap
      non_network_uniquely_owned_factories_;

  // Lazily initialized and used in the case of non-network resource
  // navigations. Keyed by URL scheme.
  // (These are cloned by entries populated in
  // non_network_url_loader_factories_ and are ready to use, i.e. preparation
  // calls like WillCreateURLLoaderFactory are already called)
  std::map<std::string, mojo::Remote<network::mojom::URLLoaderFactory>>
      non_network_url_loader_factory_remotes_;

  // Counts the time overhead of all the hops from the IO to the UI threads.
  base::TimeDelta io_to_ui_time_;

  base::WeakPtrFactory<NavigationURLLoaderImpl> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(NavigationURLLoaderImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_NAVIGATION_URL_LOADER_IMPL_H_
