// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_NAVIGATION_URL_LOADER_IMPL_H_
#define CONTENT_BROWSER_LOADER_NAVIGATION_URL_LOADER_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "content/browser/loader/navigation_url_loader.h"
#include "content/browser/navigation_subresource_loader_params.h"
#include "content/common/content_export.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/weak_document_ptr.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/url_request/url_request.h"
#include "ppapi/buildflags/buildflags.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/cpp/record_ontransfersizeupdate_utils.h"
#include "services/network/public/cpp/single_request_url_loader_factory.h"
#include "services/network/public/mojom/accept_ch_frame_observer.mojom.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/navigation/navigation_policy.h"

namespace net {
struct RedirectInfo;
}

namespace content {

class BrowserContext;
class FrameTreeNode;
class NavigationEarlyHintsManager;
class NavigationLoaderInterceptor;
class PrefetchedSignedExchangeCache;
class SignedExchangeRequestHandler;
class StoragePartition;
class StoragePartitionImpl;
struct WebPluginInfo;

class CONTENT_EXPORT NavigationURLLoaderImpl
    : public NavigationURLLoader,
      public network::mojom::URLLoaderClient,
      public network::mojom::AcceptCHFrameObserver {
 public:
  // The caller is responsible for ensuring that `delegate` outlives the loader.
  NavigationURLLoaderImpl(
      BrowserContext* browser_context,
      StoragePartition* storage_partition,
      std::unique_ptr<NavigationRequestInfo> request_info,
      std::unique_ptr<NavigationUIData> navigation_ui_data,
      ServiceWorkerMainResourceHandle* service_worker_handle,
      scoped_refptr<PrefetchedSignedExchangeCache>
          prefetched_signed_exchange_cache,
      NavigationURLLoaderDelegate* delegate,
      mojo::PendingRemote<network::mojom::CookieAccessObserver> cookie_observer,
      mojo::PendingRemote<network::mojom::TrustTokenAccessObserver>
          trust_token_observer,
      mojo::PendingRemote<network::mojom::URLLoaderNetworkServiceObserver>
          url_loader_network_observer,
      mojo::PendingRemote<network::mojom::DevToolsObserver> devtools_observer,
      std::vector<std::unique_ptr<NavigationLoaderInterceptor>>
          initial_interceptors);

  NavigationURLLoaderImpl(const NavigationURLLoaderImpl&) = delete;
  NavigationURLLoaderImpl& operator=(const NavigationURLLoaderImpl&) = delete;

  ~NavigationURLLoaderImpl() override;

  // TODO(kinuko): Some method parameters can probably be just kept as
  // member variables rather than being passed around.

  // Intercepts loading of frame requests when network service is enabled and
  // either a network::mojom::TrustedURLLoaderHeaderClient is being used or
  // for schemes not handled by network service (e.g. files). This must be
  // called on the UI thread or before threads start.
  using URLLoaderFactoryInterceptor = base::RepeatingCallback<void(
      mojo::PendingReceiver<network::mojom::URLLoaderFactory>* receiver)>;
  static void SetURLLoaderFactoryInterceptorForTesting(
      const URLLoaderFactoryInterceptor& interceptor);

  // Creates a URLLoaderFactory for a navigation. The factory uses
  // `header_client`. This should have the same settings as the factory from
  // the URLLoaderFactoryGetter. Called on the UI thread.
  static void CreateURLLoaderFactoryWithHeaderClient(
      mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>
          header_client,
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> factory_receiver,
      StoragePartitionImpl* partition);

 private:
  FRIEND_TEST_ALL_PREFIXES(NavigationURLLoaderImplTest,
                           OnAcceptCHFrameReceivedUKM);

  // Creates a SharedURLLoaderFactory for network-service-bound requests.
  static scoped_refptr<network::SharedURLLoaderFactory>
  CreateNetworkLoaderFactory(BrowserContext* browser_context,
                             StoragePartitionImpl* storage_partition,
                             FrameTreeNode* frame_tree_node,
                             const ukm::SourceIdObj& ukm_id,
                             bool* bypass_redirect_checks);

  // Starts the loader by finalizing loader factories initialization and
  // calling Restart().
  // This is called only once (while Restart can be called multiple times).
  // Sets `started_` true.
  void StartImpl(
      scoped_refptr<PrefetchedSignedExchangeCache>
          prefetched_signed_exchange_cache,
      mojo::PendingRemote<network::mojom::URLLoaderFactory> factory_for_webui,
      std::string accept_langs);

  void BindNonNetworkURLLoaderFactoryReceiver(
      const GURL& url,
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> factory_receiver);
  void BindAndInterceptNonNetworkURLLoaderFactoryReceiver(
      const GURL& url,
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> factory_receiver);

  void CreateInterceptors(scoped_refptr<PrefetchedSignedExchangeCache>
                              prefetched_signed_exchange_cache,
                          const std::string& accept_langs);

  // This could be called multiple times to follow a chain of redirects.
  // This internally rather recreates another loader than actually following the
  // redirects.
  void Restart();

  // `interceptor` is non-null if this is called by one of the interceptors
  // (via a LoaderCallback).
  // `single_request_handler` is the RequestHandler given by the
  // `interceptor`, non-null if the interceptor wants to handle the request.
  void MaybeStartLoader(
      NavigationLoaderInterceptor* interceptor,
      scoped_refptr<network::SharedURLLoaderFactory> single_request_factory);

  // This is the `fallback_callback` passed to
  // NavigationLoaderInterceptor::MaybeCreateLoader. It allows an interceptor
  // to initially elect to handle a request, and later decide to fallback to
  // the default behavior. This is needed for service worker network fallback
  // and signed exchange (SXG) fallback redirect.
  void FallbackToNonInterceptedRequest(bool reset_subresource_loader_params);

  scoped_refptr<network::SharedURLLoaderFactory>
  PrepareForNonInterceptedRequest();

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
      const network::URLLoaderCompletionStatus& status,
      network::mojom::URLResponseHeadPtr* response);

  std::vector<std::unique_ptr<blink::URLLoaderThrottle>>
  CreateURLLoaderThrottles();

  std::unique_ptr<SignedExchangeRequestHandler>
  CreateSignedExchangeRequestHandler(
      const NavigationRequestInfo& request_info,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      std::string accept_langs);

  void ParseHeaders(const GURL& url,
                    network::mojom::URLResponseHead* head,
                    base::OnceClosure continuation);

  void NotifyResponseStarted(
      network::mojom::URLResponseHeadPtr response_head,
      network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
      mojo::ScopedDataPipeConsumerHandle response_body,
      const GlobalRequestID& global_request_id,
      bool is_download);

  void NotifyRequestRedirected(net::RedirectInfo redirect_info,
                               network::mojom::URLResponseHeadPtr response);

  void NotifyRequestFailed(const network::URLLoaderCompletionStatus& status);

  // network::mojom::URLLoaderClient implementation:
  void OnReceiveEarlyHints(network::mojom::EarlyHintsPtr early_hints) override;
  void OnReceiveResponse(
      network::mojom::URLResponseHeadPtr head,
      mojo::ScopedDataPipeConsumerHandle response_body,
      absl::optional<mojo_base::BigBuffer> cached_metadata) override;
  void OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                         network::mojom::URLResponseHeadPtr head) override;
  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback callback) override;
  void OnTransferSizeUpdated(int32_t transfer_size_diff) override;
  void OnComplete(const network::URLLoaderCompletionStatus& status) override;

  // network::mojom::AcceptCHFrameObserver implementation
  void OnAcceptCHFrameReceived(
      const url::Origin& origin,
      const std::vector<network::mojom::WebClientHintsType>& accept_ch_frame,
      OnAcceptCHFrameReceivedCallback callback) override;
  void Clone(mojo::PendingReceiver<network::mojom::AcceptCHFrameObserver>
                 listener) override;

  // NavigationURLLoader implementation:
  void Start() override;
  void FollowRedirect(
      const std::vector<std::string>& removed_headers,
      const net::HttpRequestHeaders& modified_headers,
      const net::HttpRequestHeaders& modified_cors_exempt_headers) override;
  bool SetNavigationTimeout(base::TimeDelta timeout) override;

  // Records UKM for the navigation load.
  void RecordReceivedResponseUkmForOutermostMainFrame();

  raw_ptr<NavigationURLLoaderDelegate, DanglingUntriaged> delegate_;
  raw_ptr<BrowserContext> browser_context_;
  raw_ptr<StoragePartitionImpl> storage_partition_;
  raw_ptr<ServiceWorkerMainResourceHandle, DanglingUntriaged>
      service_worker_handle_;

  std::unique_ptr<network::ResourceRequest> resource_request_;
  std::unique_ptr<NavigationRequestInfo> request_info_;

  // Current URL that is being navigated, updated after redirection.
  GURL url_;

  // Redirect URL chain.
  std::vector<GURL> url_chain_;

  const int frame_tree_node_id_;
  const GlobalRequestID global_request_id_;
  const WeakDocumentPtr initiator_document_;
  net::RedirectInfo redirect_info_;
  int redirect_limit_ = net::URLRequest::kMaxRedirects;
  int accept_ch_restart_limit_ = net::URLRequest::kMaxRedirects;
  base::RepeatingCallback<WebContents*()> web_contents_getter_;
  std::unique_ptr<NavigationUIData> navigation_ui_data_;

  scoped_refptr<network::SharedURLLoaderFactory> network_loader_factory_;

  // Caches the modified request headers provided by clients during redirect,
  // will be consumed by next `url_loader_->FollowRedirect()`.
  std::vector<std::string> url_loader_removed_headers_;
  net::HttpRequestHeaders url_loader_modified_headers_;
  net::HttpRequestHeaders url_loader_modified_cors_exempt_headers_;

  absl::optional<SubresourceLoaderParams> subresource_loader_params_;

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
  // URLLoaderClient::OnReceiveResponse() is called.
  bool received_response_ = false;

  bool started_ = false;

  // The completion status if it has been received. This is needed to handle
  // the case that the response is intercepted by download, and OnComplete()
  // is already called while we are transferring the `url_loader_` and
  // response body to download code.
  absl::optional<network::URLLoaderCompletionStatus> status_;

  // The schemes that this loader can use. For anything else we'll try
  // external protocol handlers.
  std::set<std::string> known_schemes_;

  // True when a proxy will handle the redirect checks, or when an interceptor
  // intentionally returned unsafe redirect response
  // (eg: NavigationLoaderInterceptor for loading a local Web Bundle file).
  bool bypass_redirect_checks_ = false;

  network::mojom::URLResponseHeadPtr head_;
  mojo::ScopedDataPipeConsumerHandle response_body_;

  // Factories to handle navigation requests for non-network resources.
  ContentBrowserClient::NonNetworkURLLoaderFactoryMap
      non_network_url_loader_factories_;

  // Lazily initialized and used in the case of non-network resource
  // navigations. Keyed by URL scheme.
  // (These are cloned by entries populated in
  // non_network_url_loader_factories_ and are ready to use, i.e. preparation
  // calls like WillCreateURLLoaderFactory are already called)
  std::map<std::string, mojo::Remote<network::mojom::URLLoaderFactory>>
      non_network_url_loader_factory_remotes_;

  // This needs to be declared here because the underlying object might take a
  // reference on a URLLoaderFactory stored in
  // `non_network_url_loader_factory_remotes_`.
  std::unique_ptr<blink::ThrottlingURLLoader> url_loader_;

  std::unique_ptr<NavigationEarlyHintsManager> early_hints_manager_;

  // Set on the constructor and runs in Start(). This is used for transferring
  // parameters prepared in the constructor to Start().
  base::OnceClosure start_closure_;

  // While it's not expected to have two active Remote ends for the same
  // NavigationURLLoaderImpl, when a TrustedParam is copied all of the pipes are
  // cloned instead of being destroyed.
  mojo::ReceiverSet<network::mojom::AcceptCHFrameObserver>
      accept_ch_frame_observers_;

  // Timer used for triggering (optional) early timeout on the navigation.
  base::OneShotTimer timeout_timer_;

  // The time this loader was created.
  base::TimeTicks loader_creation_time_;

  // Whether the navigation processed an ACCEPT_CH frame in the TLS handshake.
  bool received_accept_ch_frame_ = false;

  // UKM source id used for recording events associated with navigation loading.
  const ukm::SourceId ukm_source_id_;

  base::WeakPtrFactory<NavigationURLLoaderImpl> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_NAVIGATION_URL_LOADER_IMPL_H_
