// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_WEB_REQUEST_WEB_REQUEST_PROXYING_URL_LOADER_FACTORY_H_
#define EXTENSIONS_BROWSER_API_WEB_REQUEST_WEB_REQUEST_PROXYING_URL_LOADER_FACTORY_H_

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_frame_host.h"
#include "extensions/browser/api/web_request/extension_web_request_event_router.h"
#include "extensions/browser/api/web_request/web_request_api.h"
#include "extensions/browser/api/web_request/web_request_info.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/auth.h"
#include "net/base/completion_once_callback.h"
#include "net/base/request_priority.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace net {
class HttpRequestHeaders;
class HttpResponseHeaders;
class IPEndPoint;
struct RedirectInfo;
}  // namespace net

namespace network {
struct URLLoaderCompletionStatus;
}

namespace extensions {

class ExtensionNavigationUIData;

// Owns URLLoaderFactory bindings for WebRequest proxies with the Network
// Service enabled.
class WebRequestProxyingURLLoaderFactory
    : public WebRequestAPI::Proxy,
      public network::mojom::URLLoaderFactory,
      public network::mojom::TrustedURLLoaderHeaderClient {
 public:
  class InProgressRequest : public network::mojom::URLLoader,
                            public network::mojom::URLLoaderClient,
                            public network::mojom::TrustedHeaderClient {
   public:
    // For usual requests
    InProgressRequest(
        WebRequestProxyingURLLoaderFactory* factory,
        uint64_t request_id,
        int32_t network_service_request_id,
        int32_t view_routing_id,
        int32_t frame_routing_id,
        uint32_t options,
        ukm::SourceIdObj ukm_source_id,
        const network::ResourceRequest& request,
        const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
        mojo::PendingReceiver<network::mojom::URLLoader> loader_receiver,
        mojo::PendingRemote<network::mojom::URLLoaderClient> client,
        scoped_refptr<base::SequencedTaskRunner>
            navigation_response_task_runner);
    // For CORS preflights
    InProgressRequest(WebRequestProxyingURLLoaderFactory* factory,
                      uint64_t request_id,
                      int32_t frame_routing_id,
                      const network::ResourceRequest& request);

    InProgressRequest(const InProgressRequest&) = delete;
    InProgressRequest& operator=(const InProgressRequest&) = delete;

    ~InProgressRequest() override;

    void Restart();

    // network::mojom::URLLoader:
    void FollowRedirect(
        const std::vector<std::string>& removed_headers,
        const net::HttpRequestHeaders& modified_headers,
        const net::HttpRequestHeaders& modified_cors_exempt_headers,
        const std::optional<GURL>& new_url) override;
    void SetPriority(net::RequestPriority priority,
                     int32_t intra_priority_value) override;
    void PauseReadingBodyFromNet() override;
    void ResumeReadingBodyFromNet() override;

    // network::mojom::URLLoaderClient:
    void OnReceiveEarlyHints(
        network::mojom::EarlyHintsPtr early_hints) override;
    void OnReceiveResponse(
        network::mojom::URLResponseHeadPtr head,
        mojo::ScopedDataPipeConsumerHandle body,
        std::optional<mojo_base::BigBuffer> cached_metadata) override;
    void OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                           network::mojom::URLResponseHeadPtr head) override;
    void OnUploadProgress(int64_t current_position,
                          int64_t total_size,
                          OnUploadProgressCallback callback) override;
    void OnTransferSizeUpdated(int32_t transfer_size_diff) override;
    void OnComplete(const network::URLLoaderCompletionStatus& status) override;

    void HandleAuthRequest(
        const net::AuthChallengeInfo& auth_info,
        scoped_refptr<net::HttpResponseHeaders> response_headers,
        WebRequestAPI::AuthRequestCallback callback);

    void OnLoaderCreated(
        mojo::PendingReceiver<network::mojom::TrustedHeaderClient> receiver);

    // network::mojom::TrustedHeaderClient:
    void OnBeforeSendHeaders(const net::HttpRequestHeaders& headers,
                             OnBeforeSendHeadersCallback callback) override;
    void OnHeadersReceived(const std::string& headers,
                           const net::IPEndPoint& endpoint,
                           OnHeadersReceivedCallback callback) override;

    // Erases all DNR actions in `info_` that are associated with
    // `extension_id`.
    void EraseDNRActionsForExtension(const ExtensionId& extension_id);

   private:
    // The state of an InProgressRequest. This is reported via UMA and UKM
    // at the end of the request, so do not change enum values.
    enum State {
      kInProgress = 0,
      kInProgressWithFinalResponseReceived,
      kInvalid,  // This is an invalid state and must not be recorded.
      kRedirectFollowedByAnotherInProgressRequest,
      kRejectedByNetworkError,
      kRejectedByNetworkErrorAfterReceivingFinalResponse,
      kDetachedFromClient,
      kDetachedFromClientAfterReceivingResponse,
      kRejectedByOnBeforeRequest,
      kRejectedByOnBeforeSendHeaders,
      kRejectedByOnHeadersReceivedForFinalResponse,
      kRejectedByOnHeadersReceivedForRedirect,
      kRejectedByOnHeadersReceivedForAuth,
      kRejectedByOnAuthRequired,
      kCompleted,
      kMaxValue = kCompleted,
    };
    // These two methods combined form the implementation of Restart().
    void UpdateRequestInfo();
    void RestartInternal();

    void ContinueToBeforeSendHeaders(State state_on_error, int error_code);
    void ContinueToBeforeSendHeadersWithOk();
    void ContinueToSendHeaders(State state_on_error,
                               const std::set<std::string>& removed_headers,
                               const std::set<std::string>& set_headers,
                               int error_code);
    void ContinueToSendHeadersWithOk(
        const std::set<std::string>& removed_headers,
        const std::set<std::string>& set_headers);
    void ContinueToStartRequest(State state_on_error, int error_code);
    void ContinueToStartRequestWithOk();
    void ContinueToHandleOverrideHeaders(int error_code);
    void OverwriteHeadersAndContinueToResponseStarted(int error_code);
    void AssignParsedHeadersAndContinueToResponseStarted(
        network::mojom::ParsedHeadersPtr parsed_headers);
    void ContinueToResponseStarted();
    void ContinueAuthRequest(const net::AuthChallengeInfo& auth_info,
                             WebRequestAPI::AuthRequestCallback callback,
                             int error_code);
    void OnAuthRequestHandled(
        WebRequestAPI::AuthRequestCallback callback,
        WebRequestEventRouter::AuthRequiredResponse response);
    void ContinueToBeforeRedirect(const net::RedirectInfo& redirect_info,
                                  int error_code);
    void HandleResponseOrRedirectHeaders(
        net::CompletionOnceCallback continuation);
    void OnRequestError(const network::URLLoaderCompletionStatus& status,
                        State state);
    void OnNetworkError(const network::URLLoaderCompletionStatus& status);
    void OnClientDisconnected();
    void OnLoaderDisconnected(uint32_t custom_reason,
                              const std::string& description);
    bool IsRedirectSafe(const GURL& from_url,
                        const GURL& to_url,
                        bool is_navigation_request);
    void HandleBeforeRequestRedirect();

    network::URLLoaderCompletionStatus CreateURLLoaderCompletionStatus(
        int error_code,
        bool collapse_initiator = false);

    const raw_ptr<WebRequestProxyingURLLoaderFactory> factory_;
    network::ResourceRequest request_;
    const std::optional<url::Origin> original_initiator_;
    const uint64_t request_id_ = 0;
    const int32_t network_service_request_id_ = 0;
    const int32_t view_routing_id_ = MSG_ROUTING_NONE;
    const int32_t frame_routing_id_ = MSG_ROUTING_NONE;
    const uint32_t options_ = 0;
    const ukm::SourceIdObj ukm_source_id_;
    const net::MutableNetworkTrafficAnnotationTag traffic_annotation_;
    mojo::Receiver<network::mojom::URLLoader> proxied_loader_receiver_;
    mojo::Remote<network::mojom::URLLoaderClient> target_client_;

    std::optional<WebRequestInfo> info_;

    mojo::Receiver<network::mojom::URLLoaderClient> proxied_client_receiver_{
        this};
    mojo::Remote<network::mojom::URLLoader> target_loader_;

    // NOTE: This is state which ExtensionWebRequestEventRouter needs to have
    // persisted across some phases of this request -- namely between
    // |OnHeadersReceived()| and request completion or restart. Pointers to
    // these fields are stored in a |BlockedRequest| (created and owned by
    // ExtensionWebRequestEventRouter) through much of the request's lifetime.
    network::mojom::URLResponseHeadPtr current_response_;
    mojo::ScopedDataPipeConsumerHandle current_body_;
    std::optional<mojo_base::BigBuffer> current_cached_metadata_;
    scoped_refptr<net::HttpResponseHeaders> override_headers_;
    GURL redirect_url_;

    // Holds any provided auth credentials through the extent of the request's
    // lifetime.
    std::optional<net::AuthCredentials> auth_credentials_;

    int num_redirects_ = 0;

    const bool for_cors_preflight_ = false;

    // If |has_any_extra_headers_listeners_| is set to true, the request will be
    // sent with the network::mojom::kURLLoadOptionUseHeaderClient option, and
    // we expect events to come through the
    // network::mojom::TrustedURLLoaderHeaderClient binding on the factory. This
    // is only set to true if there is a listener that needs to view or modify
    // headers set in the network process.
    const bool has_any_extra_headers_listeners_ = false;
    bool current_request_uses_header_client_ = false;
    OnBeforeSendHeadersCallback on_before_send_headers_callback_;
    OnHeadersReceivedCallback on_headers_received_callback_;
    mojo::Receiver<network::mojom::TrustedHeaderClient> header_client_receiver_{
        this};
    bool is_header_client_receiver_paused_ = false;

    // If |has_any_extra_headers_listeners_| is set to false and a redirect is
    // in progress, this stores the parameters to FollowRedirect that came from
    // the client. That way we can combine it with any other changes that
    // extensions made to headers in their callbacks.
    struct FollowRedirectParams {
      FollowRedirectParams();
      FollowRedirectParams(const FollowRedirectParams&) = delete;
      FollowRedirectParams& operator=(const FollowRedirectParams&) = delete;
      ~FollowRedirectParams();
      std::vector<std::string> removed_headers;
      net::HttpRequestHeaders modified_headers;
      net::HttpRequestHeaders modified_cors_exempt_headers;
      std::optional<GURL> new_url;
    };
    std::unique_ptr<FollowRedirectParams> pending_follow_redirect_params_;
    State state_ = State::kInProgress;

    // A task runner that should be used for the request when non-null. Non-null
    // when this was created for a navigation request.
    scoped_refptr<base::SequencedTaskRunner> navigation_response_task_runner_;

    base::WeakPtrFactory<InProgressRequest> weak_factory_{this};
  };

  WebRequestProxyingURLLoaderFactory(
      content::BrowserContext* browser_context,
      int render_process_id,
      int frame_routing_id,
      int view_routing_id,
      WebRequestAPI::RequestIDGenerator* request_id_generator,
      std::unique_ptr<ExtensionNavigationUIData> navigation_ui_data,
      std::optional<int64_t> navigation_id,
      ukm::SourceIdObj ukm_source_id,
      network::URLLoaderFactoryBuilder& factory_builder,
      mojo::PendingReceiver<network::mojom::TrustedURLLoaderHeaderClient>
          header_client_receiver,
      WebRequestAPI::ProxySet* proxies,
      content::ContentBrowserClient::URLLoaderFactoryType loader_factory_type,
      scoped_refptr<base::SequencedTaskRunner> navigation_response_task_runner);

  WebRequestProxyingURLLoaderFactory(
      const WebRequestProxyingURLLoaderFactory&) = delete;
  WebRequestProxyingURLLoaderFactory& operator=(
      const WebRequestProxyingURLLoaderFactory&) = delete;

  ~WebRequestProxyingURLLoaderFactory() override;

  static void StartProxying(
      content::BrowserContext* browser_context,
      int render_process_id,
      int frame_routing_id,
      int view_routing_id,
      WebRequestAPI::RequestIDGenerator* request_id_generator,
      std::unique_ptr<ExtensionNavigationUIData> navigation_ui_data,
      std::optional<int64_t> navigation_id,
      ukm::SourceIdObj ukm_source_id,
      network::URLLoaderFactoryBuilder& factory_builder,
      mojo::PendingReceiver<network::mojom::TrustedURLLoaderHeaderClient>
          header_client_receiver,
      WebRequestAPI::ProxySet* proxies,
      content::ContentBrowserClient::URLLoaderFactoryType loader_factory_type,
      scoped_refptr<base::SequencedTaskRunner> navigation_response_task_runner);

  // network::mojom::URLLoaderFactory:
  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> loader_receiver,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override;
  void Clone(mojo::PendingReceiver<network::mojom::URLLoaderFactory>
                 loader_receiver) override;

  // network::mojom::TrustedURLLoaderHeaderClient:
  void OnLoaderCreated(
      int32_t request_id,
      mojo::PendingReceiver<network::mojom::TrustedHeaderClient> receiver)
      override;
  void OnLoaderForCorsPreflightCreated(
      const network::ResourceRequest& request,
      mojo::PendingReceiver<network::mojom::TrustedHeaderClient> receiver)
      override;

  // WebRequestAPI::Proxy:
  void HandleAuthRequest(
      const net::AuthChallengeInfo& auth_info,
      scoped_refptr<net::HttpResponseHeaders> response_headers,
      int32_t request_id,
      WebRequestAPI::AuthRequestCallback callback) override;
  void OnDNRExtensionUnloaded(const Extension* extension) override;

  content::ContentBrowserClient::URLLoaderFactoryType loader_factory_type()
      const {
    return loader_factory_type_;
  }

  bool IsForServiceWorkerScript() const;
  bool IsForDownload() const;

  static void EnsureAssociatedFactoryBuilt();

 private:
  void OnTargetFactoryError();
  void OnProxyBindingError();
  void RemoveRequest(int32_t network_service_request_id, uint64_t request_id);
  void MaybeRemoveProxy();

  const raw_ptr<content::BrowserContext> browser_context_;
  const int render_process_id_;
  const int frame_routing_id_;
  const int view_routing_id_;
  const raw_ptr<WebRequestAPI::RequestIDGenerator> request_id_generator_;
  std::unique_ptr<ExtensionNavigationUIData> navigation_ui_data_;
  std::optional<int64_t> navigation_id_;
  mojo::ReceiverSet<network::mojom::URLLoaderFactory> proxy_receivers_;
  mojo::Remote<network::mojom::URLLoaderFactory> target_factory_;
  mojo::Receiver<network::mojom::TrustedURLLoaderHeaderClient>
      url_loader_header_client_receiver_{this};
  // Owns |this|.
  const raw_ptr<WebRequestAPI::ProxySet> proxies_;

  const content::ContentBrowserClient::URLLoaderFactoryType
      loader_factory_type_;
  // A UKM source ID to attribute activity to.
  ukm::SourceIdObj ukm_source_id_;

  // Mapping from our own internally generated request ID to an
  // InProgressRequest instance.
  std::map<uint64_t, std::unique_ptr<InProgressRequest>> requests_;

  // A mapping from the network stack's notion of request ID to our own
  // internally generated request ID for the same request.
  std::map<int32_t, uint64_t> network_request_id_to_web_request_id_;

  // Notifies the proxy that the browser context has been shutdown.
  base::CallbackListSubscription shutdown_notifier_subscription_;

  // A task runner that should be used for requests when non-null. Non-null when
  // this was created for a navigation request.
  scoped_refptr<base::SequencedTaskRunner> navigation_response_task_runner_;

  base::WeakPtrFactory<WebRequestProxyingURLLoaderFactory> weak_factory_{this};
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_WEB_REQUEST_WEB_REQUEST_PROXYING_URL_LOADER_FACTORY_H_
