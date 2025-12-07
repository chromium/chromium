// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_NAVIGATION_URL_LOADER_IMPL_H_
#define CONTENT_BROWSER_LOADER_NAVIGATION_URL_LOADER_IMPL_H_

#include <optional>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "content/browser/loader/navigation_url_loader.h"
#include "content/browser/loader/response_head_update_params.h"
#include "content/browser/navigation_subresource_loader_params.h"
#include "content/common/content_export.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/weak_document_ptr.h"
#include "content/public/common/buildflags.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/load_timing_info.h"
#include "net/cookies/cookie_setting_override.h"
#include "net/url_request/url_request.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/cpp/record_ontransfersizeupdate_utils.h"
#include "services/network/public/cpp/single_request_url_loader_factory.h"
#include "services/network/public/mojom/accept_ch_frame_observer.mojom.h"
#include "services/network/public/mojom/device_bound_sessions.mojom.h"
#include "services/network/public/mojom/network_context.mojom-forward.h"
#include "services/network/public/mojom/service_worker_router_info.mojom-forward.h"
#include "services/network/public/mojom/shared_dictionary_access_observer.mojom.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/common/navigation/navigation_policy.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/loader/referrer.mojom-forward.h"

namespace blink {
class URLLoaderThrottle;
}

namespace net {
struct RedirectInfo;
}

namespace network {
class URLLoaderFactoryBuilder;
}  // namespace network

namespace content {

class BrowserContext;
class FrameTreeNode;
class NavigationEarlyHintsManager;
class NavigationLoaderInterceptor;
class PrefetchedSignedExchangeCache;
class SignedExchangeRequestHandler;
class StoragePartition;
class StoragePartitionImpl;
class WebContents;

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
      mojo::PendingRemote<network::mojom::SharedDictionaryAccessObserver>
          shared_dictionary_observer,
      mojo::PendingRemote<network::mojom::URLLoaderNetworkServiceObserver>
          url_loader_network_observer,
      mojo::PendingRemote<network::mojom::DevToolsObserver> devtools_observer,
      mojo::PendingRemote<network::mojom::DeviceBoundSessionAccessObserver>
          device_bound_session_observer,
      std::vector<std::unique_ptr<NavigationLoaderInterceptor>>
          initial_interceptors);

  NavigationURLLoaderImpl(const NavigationURLLoaderImpl&) = delete;
  NavigationURLLoaderImpl& operator=(const NavigationURLLoaderImpl&) = delete;

  ~NavigationURLLoaderImpl() override;

  // Creates a URLLoaderFactory for a navigation. The factory uses
  // `header_client`. This should have the same settings as the factory from
  // the ReconnectableURLLoaderFactoryForIOThread. Called on the UI thread.
  static mojo::PendingRemote<network::mojom::URLLoaderFactory>
  CreateURLLoaderFactoryWithHeaderClient(
      mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>
          header_client,
      network::URLLoaderFactoryBuilder factory_builder,
      StoragePartitionImpl* partition,
      std::optional<net::CookieSettingOverrides> devtools_cookie_overrides,
      std::optional<net::CookieSettingOverrides> cookie_overrides);

  // NavigationURLLoader implementation:
  // Starts the loader by finalizing loader factories initialization and
  // calling Restart().
  // This is called only once (while Restart can be called multiple times).
  // Sets `started_` true.
  void Start() override;
  void FollowRedirect(
      std::vector<std::string> removed_headers,
      net::HttpRequestHeaders modified_headers,
      net::HttpRequestHeaders modified_cors_exempt_headers) override;
  bool SetNavigationTimeout(base::TimeDelta timeout) override;
  void CancelNavigationTimeout() override;

  void TriggerTimeoutForTesting();
  const network::ResourceRequest& GetResourceRequestForTesting() const;

 private:
  FRIEND_TEST_ALL_PREFIXES(NavigationURLLoaderImplTest,
                           OnAcceptCHFrameReceivedUKM);

  // Creates a terminal URLLoaderFactory only for a known non-network scheme.
  static mojo::PendingRemote<network::mojom::URLLoaderFactory>
  CreateTerminalNonNetworkLoaderFactory(BrowserContext* browser_context,
                                        StoragePartitionImpl* storage_partition,
                                        FrameTreeNode* frame_tree_node,
                                        const GURL& url);
  // Creates a complete URLLoaderFactory for non-network-service-bound requests.
  // Unlike `CreateTerminalNonNetworkLoaderFactory()`, this supports
  // ContentBrowserClient/DevTools interception, external protocols, and unknown
  // schemes.
  // `is_cacheable` indicates whether the returned factory can be cached.
  static std::pair</*is_cacheable=*/bool,
                   scoped_refptr<network::SharedURLLoaderFactory>>
  CreateNonNetworkLoaderFactory(
      BrowserContext* browser_context,
      StoragePartitionImpl* storage_partition,
      FrameTreeNode* frame_tree_node,
      const ukm::SourceIdObj& ukm_id,
      NavigationUIData* navigation_ui_data,
      const NavigationRequestInfo& request_info,
      base::RepeatingCallback<WebContents*()> web_contents_getter,
      const network::ResourceRequest& resource_request);
  // Like `CreateNonNetworkLoaderFactory()`, but caches the factory in
  // `non_network_url_loader_factories_` if `is_cacheable` is true, and reuses
  // it when the same scheme is used more than once in a navigational redirect
  // chain. This is rare because non-network schemes basically don't redirect,
  // but can actually happen e.g. in extension scheme's dynamic URLs (see
  // `DynamicOriginBrowserTest.DynamicUrl` unit test).
  // TODO(crbug.com/40251638): Consider removing the caching, as caches are
  // often source of bug. The caching mechanism is left here to keep the
  // existing behavior.
  scoped_refptr<network::SharedURLLoaderFactory>
  GetOrCreateNonNetworkLoaderFactory();

  // Creates a SharedURLLoaderFactory for network-service-bound requests.
  static scoped_refptr<network::SharedURLLoaderFactory>
  CreateNetworkLoaderFactory(BrowserContext* browser_context,
                             StoragePartitionImpl* storage_partition,
                             FrameTreeNode* frame_tree_node,
                             const ukm::SourceIdObj& ukm_id,
                             bool* bypass_redirect_checks);

  void BindAndInterceptNonNetworkURLLoaderFactoryReceiver(
      const GURL& url,
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> factory_receiver);

  void CreateInterceptors();

  // This could be called multiple times to follow a chain of redirects.
  // This internally rather recreates another loader than actually following the
  // redirects.
  void Restart();

  // `interceptor_result` is the result from the current interceptor (or nullopt
  // if not called via `LoaderCallback`).
  // `next_interceptor` indicates the index of the next interceptor to check.
  void MaybeStartLoader(
      size_t next_interceptor_index,
      std::optional<NavigationLoaderInterceptor::Result> interceptor_result);

  // Called from `MaybeStartLoader` when the request is elected to be
  // intercepted. Intercepts the request with `single_request_factory`.
  void StartInterceptedRequest(
      scoped_refptr<network::SharedURLLoaderFactory> single_request_factory);

  // Start a loader with the default behavior. This should be used when no
  // interceptors have elected to handle the request in the first place.
  void StartNonInterceptedRequest(ResponseHeadUpdateParams head_update_params);

  // This is the `fallback_callback_for_service_worker` passed to
  // NavigationLoaderInterceptor::MaybeCreateLoader. It allows an interceptor
  // to initially elect to handle a request, and later decide to fallback to
  // the default behavior. This is needed for service worker network fallback.
  //
  // This is a static method + WeakPtr `self` argument, to return `nullptr` when
  // `self` is destroyed (`base::BindOnce` + non-static member method +
  // `WeakPtr` doesn't work if the return type is not `void`).
  static network::mojom::URLLoaderFactory* FallbackToNonInterceptedRequest(
      base::WeakPtr<NavigationURLLoaderImpl> self,
      ResponseHeadUpdateParams head_update_params);

  void CreateThrottlingLoaderAndStart(
      scoped_refptr<network::SharedURLLoaderFactory> factory,
      std::vector<std::unique_ptr<blink::URLLoaderThrottle>>
          additional_throttles);

#if BUILDFLAG(ENABLE_PLUGINS)
  void CheckPluginAndCallOnReceiveResponse(
      network::mojom::URLResponseHeadPtr head,
      network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints);
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
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  // When `clear_parsed_headers_for_testing` is true (which is only allowed in
  // tests), `head->parsed_headers` is cleared to enforce and test the async
  // `ParseHeaders()` path. https://crbug.com/434182226
  void ParseHeaders(
      const GURL& url,
      network::mojom::URLResponseHeadPtr head,
      base::OnceCallback<void(network::mojom::URLResponseHeadPtr)> continuation,
      bool clear_parsed_headers_for_testing);

  void NotifyResponseStarted(
      network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
      mojo::ScopedDataPipeConsumerHandle response_body,
      const GlobalRequestID& global_request_id,
      bool is_download,
      network::mojom::URLResponseHeadPtr response_head);

  void NotifyRequestRedirected(net::RedirectInfo redirect_info,
                               network::mojom::URLResponseHeadPtr response);

  void NotifyRequestFailed(const network::URLLoaderCompletionStatus& status);

  // network::mojom::URLLoaderClient implementation:
  void OnReceiveEarlyHints(network::mojom::EarlyHintsPtr early_hints) override;
  void OnReceiveResponse(
      network::mojom::URLResponseHeadPtr head,
      mojo::ScopedDataPipeConsumerHandle response_body,
      std::optional<mojo_base::BigBuffer> cached_metadata) override;
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

  // Records UKM for the navigation load.
  void RecordReceivedResponseUkmForOutermostMainFrame();

  // Record ServiceWorker and the static routing API evaluation related results.
  void MaybeRecordServiceWorkerMainResourceInfo(
      const network::mojom::URLResponseHeadPtr& head);

  raw_ptr<NavigationURLLoaderDelegate, DanglingUntriaged> delegate_;
  raw_ptr<BrowserContext> browser_context_;
  raw_ptr<StoragePartitionImpl> storage_partition_;
  raw_ptr<ServiceWorkerMainResourceHandle> service_worker_handle_;

  std::unique_ptr<network::ResourceRequest> resource_request_;
  std::unique_ptr<NavigationRequestInfo> request_info_;

  // Current URL that is being navigated, updated after redirection.
  GURL url_;

  const FrameTreeNodeId frame_tree_node_id_;
  const GlobalRequestID global_request_id_;
  net::RedirectInfo redirect_info_;
  int redirect_limit_ = net::URLRequest::kMaxRedirects;
  int accept_ch_restart_limit_ = net::URLRequest::kMaxRedirects;
  base::RepeatingCallback<WebContents*()> web_contents_getter_;
  std::unique_ptr<NavigationUIData> navigation_ui_data_;

  scoped_refptr<network::SharedURLLoaderFactory> network_loader_factory_;

  SubresourceLoaderParams subresource_loader_params_;

  std::vector<std::unique_ptr<NavigationLoaderInterceptor>> interceptors_;

  // Set to true if the default URLLoader (network service) was used for the
  // current navigation.
  bool default_loader_used_ = false;

  // Set to true if we receive a valid response from a URLLoader, i.e.
  // URLLoaderClient::OnReceiveResponse() is called.
  bool received_response_ = false;

  bool started_ = false;

  // The completion status if it has been received. This is needed to handle
  // the case that the response is intercepted by download, and OnComplete()
  // is already called while we are transferring the `url_loader_` and
  // response body to download code.
  std::optional<network::URLLoaderCompletionStatus> status_;

  // True when a proxy will handle the redirect checks, or when an interceptor
  // intentionally returned unsafe redirect response
  // (eg: NavigationLoaderInterceptor for loading a local Web Bundle file).
  bool bypass_redirect_checks_ = false;

  mojo::ScopedDataPipeConsumerHandle response_body_;

  // Lazily initialized and used in the case of non-network resource
  // navigations. Keyed by URL scheme.
  std::map<std::string, scoped_refptr<network::SharedURLLoaderFactory>>
      non_network_url_loader_factories_;

  // `NavigationURLLoaderImpl` performs the loading (receiving the
  // `URLLoaderClient` callbacks and related operations) in multiple ways (e.g.
  // through `url_loader_` or `response_loader_receiver_`). `LoaderHolder`
  // centralizes these, to ensure that:
  // - All related operations are cancelled when `Reset()` is called.
  // - The state transitions are consistent.
  //
  // Note: This isn't a complete encapsulation. Some states and operations
  // (especially related to https://crbug.com/434182226) are centralized and
  // checked in `LoaderHolder`, while other operations are still performed
  // directly via `url_loader()`.
  class LoaderHolder final {
   public:
    explicit LoaderHolder(network::mojom::URLLoaderClient* receiver);
    ~LoaderHolder();

    // Right now, `State` is used only for `DUMP_WILL_BE_CHECK()`ing and the
    // other underlying members (e.g. `url_loader_`) should be used to check
    // state-dependent conditions.
    // TODO(https://crbug.com/434182226): Once the `DUMP_WILL_BE_CHECK()`s stick
    // and are turned into `CHECK()`s, `State` must match with `url_loader_`
    // etc. and thus should be used also for non-CHECK purposes.
    enum class State {
      // Neither of the loader or receiver is active right now.
      // This can be a transient state when processing a redirect (after
      // resetting the previous redirect leg and just before starting the next
      // leg).
      kNone,

      // The loading is ongoing and the `NavigationURLLoaderImpl`'s
      // `URLLoaderClient` methods are called via `url_loader_` (the primary
      // cases).
      // The `URLLoaderClient` methods are called directly (synchronously) by
      // `blink::ThrottlingURLLoader`.
      kLoadingViaLoader,

      // The loading is ongoing and the `NavigationURLLoaderImpl`'s
      // `URLLoaderClient` methods are called via
      // `response_loader_receiver_` (for `MaybeCreateLoaderForResponse()`).
      // The `URLLoaderClient` methods are called via mojo.
      kLoadingViaReceiver,

      // All loading via `NavigationURLLoaderImpl` is done and further
      // operation on `LoaderHolder` or calls to `NavigationURLLoaderImpl`'s
      // `URLLoaderClient` methods shouldn't be made, except for some operations
      // directly through `url_loader()`.
      kUnbound,
    };

    State state() const { return state_; }

    blink::ThrottlingURLLoader* url_loader() const { return url_loader_.get(); }
    mojo::PendingRemote<network::mojom::URLLoader>* response_url_loader() {
      return &response_url_loader_;
    }

    // Cancel the current loading, if any.
    // Any associated pending operations should be cancelled.
    // Transitions to `State::kNone`.
    //
    // Note: The "exclusive tasks" (see `ExclusiveTaskState` below) can't be
    // gracefully cancelled here and thus `Reset()` should be called only when
    // there should always be no exclusive tasks. See also `ResetForFailure()`.
    void Reset();

    // When the caller wants to start a new loading operation while there can be
    // existing exclusive tasks, the caller should check `HasExclusiveTask()`,
    // and if there are exclusive tasks, call `ResetForFailure()` and fail the
    // entire loading instead.
    //
    // This is similar to `Reset()`, but also instructs exclusive tasks to be
    // cancelled.
    void ResetForFailure();

    // For starting loading via `url_loader` (transitioning from `kNone` to
    // `kLoadingViaLoader`). THe caller should actually start the loading by
    // calling `url_loader->Start()`.
    // Transitions to `State::kLoadingViaLoader`.
    void SetLoader(std::unique_ptr<blink::ThrottlingURLLoader> url_loader);

    // Switches to loading via `pending_receiver` (transitioning from
    // `kLoadingViaLoader` to `kLoadingViaReceiver`). The caller might already
    // call `url_loader()->Unbind()` etc.
    // Transitions to `State::kLoadingViaReceiver`.
    void BindReceiver(
        mojo::PendingReceiver<network::mojom::URLLoaderClient> pending_receiver,
        scoped_refptr<base::SequencedTaskRunner> task_runner);

    // Unbind the endpoints from ``NavigationURLLoaderImpl`` to
    // `URLLoaderClientEndpointsPtr` (transitioning to `kUnbound`).
    // Transitions to `State::kUnbound`.
    [[nodiscard]] network::mojom::URLLoaderClientEndpointsPtr Unbind();

    // Redirect handling: the expected sequence is:
    // 1. `NavigationURLLoaderImpl::OnReceiveRedirect()`
    // 2. `LoaderHolder::SetModifiedHeadersOnRedirect()`
    // 3. Either:
    //    - `LoaderHolder::FollowRedirect()`, when we want and can continue
    //      using the current `url_loader` for the next redirect leg, or
    //    - `LoaderHolder::ResetForFollowRedirect()` then
    //      `LoaderHolder::SetLoader()`, when we start the next redirect leg
    //      with a new loader.
    // Also `LoaderHolder::Reset()` can be called at any time during this to
    // cancel loading.
    // TODO(https://crbug.com/434182226): Add `CHECK()`s to confirm these
    // sequences.

    // Cache the modified request headers provided by clients during redirect.
    // They will be consumed by next `FollowRedirect()` or
    // `ResetForFollowRedirect()`.
    void SetModifiedHeadersOnRedirect(
        std::vector<std::string> removed_headers,
        net::HttpRequestHeaders modified_headers,
        net::HttpRequestHeaders modified_cors_exempt_headers);

    // Follows the redirect using the current `url_loader_`.
    void FollowRedirect();

    // Similar to `ResetLoader()`, but also calls
    // `URLLoader::ResetForFollowRedirect()` if needed.
    void ResetForFollowRedirect(network::ResourceRequest& resource_request);

    // See the `ExclusiveTaskState` comment below.
    // TODO(https://crbug.com/434182226): Add more exclusive tasks handing.
    enum ExclusiveTaskType {
      // From `OnReceiveRedirect()` until `FollowRedirect()`.
      // This contains two possible async tasks:
      // - Waiting for `network.mojom.NetworkService::ParseHeaders()` and
      // - Waiting for `NavigationURLLoaderDelegate`: from
      //   `OnRequestRedirected()` until
      //   `NavigationURLLoaderImpl::FollowRedirect()` is called.
      kRedirect,

      // Waiting for `NavigationLoaderInterceptor::MaybeCreateLoader()`.
      // From `NavigationURLLoaderImpl::Restart()`
      // Until `NavigationURLLoaderImpl::StartNonInterceptedRequest()` or
      // `NavigationURLLoaderImpl::StartInterceptedRequest()`.
      kInterceptor,
    };
    void OnExclusiveTaskStarted(ExclusiveTaskType exclusive_task_type);
    void OnExclusiveTaskCompleted(ExclusiveTaskType exclusive_task_type);
    bool HasExclusiveTask() const;
    // Should be called only during an exclusive task.
    bool ShouldCancelExclusiveTask(ExclusiveTaskType exclusive_task_type) const;

    bool receiver_is_bound_for_check() const;

   private:
    void ResetInternal();
    void CheckState() const;

    State state_ = State::kNone;

    // `NavigationURLLoaderImpl`'s `URLLoaderClient` methods are called either
    // via `url_loader_` or `response_loader_receiver_`.
    // See also the comment at `State` above for details.
    std::unique_ptr<blink::ThrottlingURLLoader> url_loader_;
    mojo::Receiver<network::mojom::URLLoaderClient> response_loader_receiver_;

    // URLLoader instance for response loaders, i.e loaders created for handling
    // responses received from the network URLLoader.
    //
    // NOTE: This looks like coupled with
    // `LoaderHolder::response_loader_receiver_` but actually isn't, because
    // `response_url_loader_` is never touched during
    // `MaybeCreateLoaderForResponse()` (at least within Chromium codesearch).
    // For now this is kept here as-is but probably can be removed.
    mojo::PendingRemote<network::mojom::URLLoader> response_url_loader_;

    struct ModifiedHeadersOnRedirect final {
      ModifiedHeadersOnRedirect(
          std::vector<std::string> removed_headers,
          net::HttpRequestHeaders modified_headers,
          net::HttpRequestHeaders modified_cors_exempt_headers);
      ModifiedHeadersOnRedirect(const ModifiedHeadersOnRedirect&) = delete;
      ModifiedHeadersOnRedirect(ModifiedHeadersOnRedirect&&) = delete;
      ~ModifiedHeadersOnRedirect();

      std::vector<std::string> removed_headers_;
      net::HttpRequestHeaders modified_headers_;
      net::HttpRequestHeaders modified_cors_exempt_headers_;
    };
    std::optional<ModifiedHeadersOnRedirect> modified_headers_on_redirect_;

    // `NavigationURLLoaderImpl` can be waiting for a certain (possibly async)
    // "exclusive task" and can't start a new request nor receive
    // URLLoaderClient method calls until the task completes. For example,
    // `NavigationURLLoaderImpl`, `NavigationLoaderInterceptor` and
    // `NavigationRequest` are going through checks before sending an initial or
    // redirected request and in the middle of updating the request and other
    // states.
    //
    // Not all arbitrary async operations are considered exclusive tasks here.
    // For example, waiting for URLLoaderClient calls from `url_loader_` or
    // `response_loader_receiver_` aren't considered exclusive tasks, because
    // e.g. we can cancel the `url_loader_`, issue a synthetic redirect response
    // as if it would be received from `url_loader_` and continue on the
    // synthetic redirect.
    //
    // Original design doc:
    // https://docs.google.com/document/d/1xCq9l9mYc3WE1adspyaX7FnPArxKDyz8BcRGTlpQIu8/edit?usp=sharing
    enum ExclusiveTaskState {
      kNoExclusiveTask,

      // There is an exclusive task, and therefore:
      // - No URLLoaderClient methods should be called (except for unexpected
      //   `OnComplete()` or error events like timeout).
      // - No actions that could initiate a new request/redirect etc. are
      //   allowed. When attempting such actions, `NavigationURLLoaderImpl`
      //   should call `ResetForFailure()` and make the navigation fail
      //   immediately.
      //
      // There should be at most one exclusive task at a time.
      kHasExclusiveTask,

      // After `ResetForFailure()` is called, navigation fails and existing
      // exclusive tasks should be cancelled.
      kCancelExclusiveTask,
    };

    ExclusiveTaskState exclusive_task_state_ =
        ExclusiveTaskState::kNoExclusiveTask;
    std::optional<ExclusiveTaskType> current_exclusive_task_type_;
  };

  LoaderHolder loader_holder_{this};

  std::unique_ptr<NavigationEarlyHintsManager> early_hints_manager_;

  // Cleared after `Start()`.
  scoped_refptr<PrefetchedSignedExchangeCache>
      prefetched_signed_exchange_cache_;

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

  // If this navigation was intercepted by a worker but the worker didn't handle
  // it, we still expose some parameters like the worker timing as part of the
  // response.
  ResponseHeadUpdateParams head_update_params_;

  base::WeakPtrFactory<NavigationURLLoaderImpl> weak_factory_{this};
};

// Creates a `ResourceRequest` and sets fields common to navigation and
// prefetch.
// This roughly corresponds to:
// - Step 3 of
// https://html.spec.whatwg.org/multipage/browsing-the-web.html#create-navigation-params-by-fetching
// - Step 2 of
// https://wicg.github.io/nav-speculation/prefetch.html#create-a-navigation-request
// and their surrounding steps.
//
// This helper method is used to create consistent navigational
// `ResourceRequest`s (exposed to the network service and ServiceWorker fetch
// handlers) and make them look similar, regardless of whether they are created
// for prefetches or non-prefetch navigations.
std::unique_ptr<network::ResourceRequest> CreateResourceRequestForNavigation(
    const std::string& method,
    const GURL& url,
    network::mojom::RequestDestination destination,
    const blink::mojom::Referrer& referrer,
    const net::IsolationInfo& isolation_info,
    mojo::PendingRemote<network::mojom::DevToolsObserver> devtools_observer,
    net::RequestPriority priority,
    bool is_main_frame);

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_NAVIGATION_URL_LOADER_IMPL_H_
