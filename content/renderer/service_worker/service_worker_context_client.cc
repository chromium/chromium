// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/service_worker/service_worker_context_client.h"

#include <map>
#include <memory>
#include <utility>

#include "base/debug/alias.h"
#include "base/feature_list.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_local.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "content/common/service_worker/service_worker.mojom.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/common/content_features.h"
#include "content/public/common/network_service_util.h"
#include "content/public/common/referrer.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/public/renderer/document_state.h"
#include "content/public/renderer/worker_thread.h"
#include "content/renderer/loader/child_url_loader_factory_bundle.h"
#include "content/renderer/loader/request_extra_data.h"
#include "content/renderer/loader/tracked_child_url_loader_factory_bundle.h"
#include "content/renderer/loader/web_data_consumer_handle_impl.h"
#include "content/renderer/loader/web_url_loader_impl.h"
#include "content/renderer/loader/web_url_request_util.h"
#include "content/renderer/notifications/notification_data_conversions.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/renderer_blink_platform_impl.h"
#include "content/renderer/service_worker/controller_service_worker_impl.h"
#include "content/renderer/service_worker/embedded_worker_instance_client_impl.h"
#include "content/renderer/service_worker/service_worker_fetch_context_impl.h"
#include "content/renderer/service_worker/service_worker_network_provider.h"
#include "content/renderer/service_worker/service_worker_provider_context.h"
#include "content/renderer/service_worker/service_worker_timeout_timer.h"
#include "content/renderer/service_worker/service_worker_type_converters.h"
#include "content/renderer/service_worker/service_worker_type_util.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/request_context_frame_type.mojom.h"
#include "storage/common/blob_storage/blob_handle.h"
#include "third_party/blink/public/common/messaging/message_port_channel.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"
#include "third_party/blink/public/common/service_worker/service_worker_utils.h"
#include "third_party/blink/public/mojom/blob/blob.mojom.h"
#include "third_party/blink/public/mojom/blob/blob_registry.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_client.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_error_type.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_event_status.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_object.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"
#include "third_party/blink/public/platform/interface_provider.h"
#include "third_party/blink/public/platform/modules/background_fetch/background_fetch.mojom.h"
#include "third_party/blink/public/platform/modules/background_fetch/web_background_fetch_registration.h"
#include "third_party/blink/public/platform/modules/notifications/web_notification_data.h"
#include "third_party/blink/public/platform/modules/payments/web_payment_handler_response.h"
#include "third_party/blink/public/platform/modules/payments/web_payment_request_event_data.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_clients_info.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_error.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_network_provider.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_request.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_response.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/url_conversion.h"
#include "third_party/blink/public/platform/web_blob_registry.h"
#include "third_party/blink/public/platform/web_http_body.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/public/web/modules/service_worker/web_service_worker_context_client.h"
#include "third_party/blink/public/web/modules/service_worker/web_service_worker_context_proxy.h"

using blink::WebURLRequest;
using blink::MessagePortChannel;

namespace content {

namespace {

constexpr char kServiceWorkerContextClientScope[] =
    "ServiceWorkerContextClient";

// For now client must be a per-thread instance.
base::LazyInstance<base::ThreadLocalPointer<ServiceWorkerContextClient>>::
    Leaky g_worker_client_tls = LAZY_INSTANCE_INITIALIZER;

// Called on the main thread only and blink owns it.
class WebServiceWorkerNetworkProviderImpl
    : public blink::WebServiceWorkerNetworkProvider {
 public:
  explicit WebServiceWorkerNetworkProviderImpl(
      std::unique_ptr<ServiceWorkerNetworkProvider> provider)
      : provider_(std::move(provider)) {}

  // Blink calls this method for each request starting with the main script,
  // we tag them with the provider id.
  void WillSendRequest(WebURLRequest& request) override {
    auto extra_data = std::make_unique<RequestExtraData>();
    extra_data->set_service_worker_provider_id(provider_->provider_id());
    extra_data->set_originated_from_service_worker(true);
    // Service workers are only available in secure contexts, so all requests
    // are initiated in a secure context.
    extra_data->set_initiated_in_secure_context(true);
    request.SetExtraData(std::move(extra_data));
  }

  std::unique_ptr<blink::WebURLLoader> CreateURLLoader(
      const WebURLRequest& request,
      std::unique_ptr<blink::scheduler::WebResourceLoadingTaskRunnerHandle>
          task_runner_handle) override {
    RenderThreadImpl* render_thread = RenderThreadImpl::current();
    if (render_thread && provider_->script_loader_factory() &&
        blink::ServiceWorkerUtils::IsServicificationEnabled() &&
        IsScriptRequest(request)) {
      // TODO(crbug.com/796425): Temporarily wrap the raw
      // mojom::URLLoaderFactory pointer into SharedURLLoaderFactory.
      return std::make_unique<WebURLLoaderImpl>(
          render_thread->resource_dispatcher(), std::move(task_runner_handle),
          base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
              provider_->script_loader_factory()));
    }
    return nullptr;
  }

  network::mojom::URLLoaderFactory* script_loader_factory() {
    return provider_->script_loader_factory();
  }

  int ProviderID() const override { return provider_->provider_id(); }

 private:
  static bool IsScriptRequest(const WebURLRequest& request) {
    auto request_context = request.GetRequestContext();
    return request_context ==
               blink::mojom::RequestContextType::SERVICE_WORKER ||
           request_context == blink::mojom::RequestContextType::SCRIPT ||
           request_context == blink::mojom::RequestContextType::IMPORT;
  }

  std::unique_ptr<ServiceWorkerNetworkProvider> provider_;
};

class StreamHandleListener
    : public blink::WebServiceWorkerStreamHandle::Listener {
 public:
  StreamHandleListener(
      blink::mojom::ServiceWorkerStreamCallbackPtr callback_ptr,
      std::unique_ptr<ServiceWorkerTimeoutTimer::StayAwakeToken> token)
      : callback_ptr_(std::move(callback_ptr)), token_(std::move(token)) {}

  ~StreamHandleListener() override {}

  void OnAborted() override {
    callback_ptr_->OnAborted();
    token_.reset();
  }

  void OnCompleted() override {
    callback_ptr_->OnCompleted();
    token_.reset();
  }

 private:
  blink::mojom::ServiceWorkerStreamCallbackPtr callback_ptr_;
  std::unique_ptr<ServiceWorkerTimeoutTimer::StayAwakeToken> token_;
};

blink::mojom::RequestContextType GetBlinkRequestContext(
    blink::mojom::RequestContextType request_context_type) {
  return static_cast<blink::mojom::RequestContextType>(request_context_type);
}

blink::WebServiceWorkerClientInfo ToWebServiceWorkerClientInfo(
    blink::mojom::ServiceWorkerClientInfoPtr client_info) {
  DCHECK(!client_info->client_uuid.empty());

  blink::WebServiceWorkerClientInfo web_client_info;

  web_client_info.uuid = blink::WebString::FromASCII(client_info->client_uuid);
  web_client_info.page_visibility_state = client_info->page_visibility_state;
  web_client_info.is_focused = client_info->is_focused;
  web_client_info.url = client_info->url;
  web_client_info.frame_type = client_info->frame_type;
  web_client_info.client_type = client_info->client_type;

  return web_client_info;
}

// Converts a content::BackgroundFetchRegistration object to
// a blink::WebBackgroundFetchRegistration object.
blink::WebBackgroundFetchRegistration ToWebBackgroundFetchRegistration(
    const BackgroundFetchRegistration& registration) {
  return blink::WebBackgroundFetchRegistration(
      blink::WebString::FromUTF8(registration.developer_id),
      blink::WebString::FromUTF8(registration.unique_id),
      registration.upload_total, registration.uploaded,
      registration.download_total, registration.downloaded, registration.result,
      registration.failure_reason);
}

// This is complementary to ConvertWebKitPriorityToNetPriority, defined in
// web_url_loader_impl.cc.
WebURLRequest::Priority ConvertNetPriorityToWebKitPriority(
    const net::RequestPriority priority) {
  switch (priority) {
    case net::HIGHEST:
      return WebURLRequest::Priority::kVeryHigh;
    case net::MEDIUM:
      return WebURLRequest::Priority::kHigh;
    case net::LOW:
      return WebURLRequest::Priority::kMedium;
    case net::LOWEST:
      return WebURLRequest::Priority::kLow;
    case net::IDLE:
      return WebURLRequest::Priority::kVeryLow;
    case net::THROTTLED:
      NOTREACHED();
      return WebURLRequest::Priority::kVeryLow;
  }

  NOTREACHED();
  return WebURLRequest::Priority::kVeryLow;
}

// If |is_for_fetch_event| is true, some headers may be omitted according
// to the embedder. It's always true now, but it might change with
// more Onion Souping, and future callsites like Background Fetch probably don't
// want to do the header filtering, since it's important for them to have the
// exact request as sent to network.
void ToWebServiceWorkerRequest(const network::ResourceRequest& request,
                               const std::string request_body_blob_uuid,
                               uint64_t request_body_blob_size,
                               blink::mojom::BlobPtrInfo request_body_blob,
                               const std::string& client_id,
                               std::vector<blink::mojom::BlobPtrInfo> blob_ptrs,
                               blink::WebServiceWorkerRequest* web_request,
                               bool is_for_fetch_event) {
  DCHECK(web_request);
  web_request->SetURL(blink::WebURL(request.url));
  web_request->SetMethod(blink::WebString::FromUTF8(request.method));
  if (!request.headers.IsEmpty()) {
    for (net::HttpRequestHeaders::Iterator it(request.headers); it.GetNext();) {
      if (!is_for_fetch_event ||
          !GetContentClient()
               ->renderer()
               ->IsExcludedHeaderForServiceWorkerFetchEvent(it.name())) {
        web_request->SetHeader(blink::WebString::FromUTF8(it.name()),
                               blink::WebString::FromUTF8(it.value()));
      }
    }
  }

  // Non-S13nServiceWorker: The body is provided as a blob.
  if (request_body_blob) {
    DCHECK(!base::FeatureList::IsEnabled(network::features::kNetworkService));
    mojo::ScopedMessagePipeHandle blob_pipe = request_body_blob.PassHandle();
    web_request->SetBlob(blink::WebString::FromASCII(request_body_blob_uuid),
                         request_body_blob_size, std::move(blob_pipe));
  }
  // S13nServiceWorker: The body is provided in |request|.
  else if (request.request_body) {
    DCHECK(blink::ServiceWorkerUtils::IsServicificationEnabled());
    // |blob_ptrs| should be empty when Network Service is enabled.
    DCHECK(!base::FeatureList::IsEnabled(network::features::kNetworkService) ||
           blob_ptrs.empty());
    blink::WebHTTPBody body = GetWebHTTPBodyForRequestBodyWithBlobPtrs(
        *request.request_body, std::move(blob_ptrs));
    body.SetUniqueBoundary();
    web_request->SetBody(body);
  }

  web_request->SetReferrer(blink::WebString::FromUTF8(request.referrer.spec()),
                           Referrer::NetReferrerPolicyToBlinkReferrerPolicy(
                               request.referrer_policy));
  web_request->SetMode(request.fetch_request_mode);
  web_request->SetIsMainResourceLoad(ServiceWorkerUtils::IsMainResourceType(
      static_cast<ResourceType>(request.resource_type)));
  web_request->SetCredentialsMode(request.fetch_credentials_mode);
  web_request->SetCacheMode(
      ServiceWorkerUtils::GetCacheModeFromLoadFlags(request.load_flags));
  web_request->SetRedirectMode(request.fetch_redirect_mode);
  web_request->SetRequestContext(
      GetBlinkRequestContext(static_cast<blink::mojom::RequestContextType>(
          request.fetch_request_context_type)));
  web_request->SetFrameType(request.fetch_frame_type);
  web_request->SetClientId(blink::WebString::FromUTF8(client_id));
  web_request->SetIsReload(ui::PageTransitionCoreTypeIs(
      static_cast<ui::PageTransition>(request.transition_type),
      ui::PAGE_TRANSITION_RELOAD));
  web_request->SetIntegrity(
      blink::WebString::FromUTF8(request.fetch_integrity));
  web_request->SetPriority(
      ConvertNetPriorityToWebKitPriority(request.priority));
  web_request->SetKeepalive(request.keepalive);
  web_request->SetIsHistoryNavigation(request.transition_type &
                                      ui::PAGE_TRANSITION_FORWARD_BACK);
}

// Finds an event callback keyed by |event_id| from |map|, and runs the callback
// with |args|. Returns true if the callback was found and called, otherwise
// returns false.
template <typename MapType, class... Args>
bool RunEventCallback(MapType* map,
                      ServiceWorkerTimeoutTimer* timer,
                      int event_id,
                      Args... args) {
  auto iter = map->find(event_id);
  // The event may have been aborted.
  if (iter == map->end())
    return false;
  std::move(iter->second).Run(args...);
  map->erase(iter);
  timer->EndEvent(event_id);
  return true;
}

// Creates a callback which takes an |event_id|, which calls the given event's
// callback with ABORTED status and removes it from |map|.
template <typename MapType, typename... Args>
base::OnceCallback<void(int /* event_id */)> CreateAbortCallback(MapType* map,
                                                                 Args... args) {
  return base::BindOnce(
      [](MapType* map, Args... args, base::TimeTicks dispatched_time,
         int event_id) {
        auto iter = map->find(event_id);
        DCHECK(iter != map->end());
        std::move(iter->second)
            .Run(blink::mojom::ServiceWorkerEventStatus::ABORTED,
                 std::forward<Args>(args)..., dispatched_time);
        map->erase(iter);
      },
      map, std::forward<Args>(args)..., base::TimeTicks::Now());
}

}  // namespace

// Holding data that needs to be bound to the worker context on the
// worker thread.
struct ServiceWorkerContextClient::WorkerContextData {
  explicit WorkerContextData(ServiceWorkerContextClient* owner)
      : service_worker_binding(owner),
        weak_factory(owner),
        proxy_weak_factory(owner->proxy_) {}

  ~WorkerContextData() {
    DCHECK(thread_checker.CalledOnValidThread());
  }

  mojo::Binding<mojom::ServiceWorker> service_worker_binding;

  // Maps for inflight event callbacks.
  // These are mapped from an event id issued from ServiceWorkerTimeoutTimer to
  // the Mojo callback to notify the end of the event.
  std::map<int, DispatchInstallEventCallback> install_event_callbacks;
  std::map<int, DispatchActivateEventCallback> activate_event_callbacks;
  std::map<int, DispatchBackgroundFetchAbortEventCallback>
      background_fetch_abort_event_callbacks;
  std::map<int, DispatchBackgroundFetchClickEventCallback>
      background_fetch_click_event_callbacks;
  std::map<int, DispatchBackgroundFetchFailEventCallback>
      background_fetch_fail_event_callbacks;
  std::map<int, DispatchBackgroundFetchSuccessEventCallback>
      background_fetched_event_callbacks;
  std::map<int, DispatchSyncEventCallback> sync_event_callbacks;
  std::map<int, payments::mojom::PaymentHandlerResponseCallbackPtr>
      abort_payment_result_callbacks;
  std::map<int, DispatchCanMakePaymentEventCallback>
      abort_payment_event_callbacks;
  std::map<int, DispatchCanMakePaymentEventCallback>
      can_make_payment_event_callbacks;
  std::map<int, DispatchPaymentRequestEventCallback>
      payment_request_event_callbacks;
  std::map<int, DispatchNotificationClickEventCallback>
      notification_click_event_callbacks;
  std::map<int, DispatchNotificationCloseEventCallback>
      notification_close_event_callbacks;
  std::map<int, DispatchPushEventCallback> push_event_callbacks;
  std::map<int, DispatchFetchEventCallback> fetch_event_callbacks;
  std::map<int, DispatchCookieChangeEventCallback>
      cookie_change_event_callbacks;
  std::map<int, DispatchExtendableMessageEventCallback> message_event_callbacks;

  // Maps for response callbacks.
  // These are mapped from an event id to the Mojo interface pointer which is
  // passed from the relevant DispatchSomeEvent() method.
  std::map<int, payments::mojom::PaymentHandlerResponseCallbackPtr>
      can_make_payment_result_callbacks;
  std::map<int, payments::mojom::PaymentHandlerResponseCallbackPtr>
      payment_response_callbacks;
  std::map<int, blink::mojom::ServiceWorkerFetchResponseCallbackPtr>
      fetch_response_callbacks;

  // Inflight navigation preload requests.
  base::IDMap<std::unique_ptr<NavigationPreloadRequest>> preload_requests;

  // S13nServiceWorker
  // Timer triggered when the service worker considers it should be stopped or
  // an event should be aborted.
  std::unique_ptr<ServiceWorkerTimeoutTimer> timeout_timer;

  // S13nServiceWorker
  // |controller_impl| should be destroyed before |timeout_timer| since the
  // pipe needs to be disconnected before callbacks passed by
  // DispatchSomeEvent() get destructed, which may be stored in |timeout_timer|
  // as parameters of pending tasks added after a termination request.
  std::unique_ptr<ControllerServiceWorkerImpl> controller_impl;

  base::ThreadChecker thread_checker;
  base::WeakPtrFactory<ServiceWorkerContextClient> weak_factory;
  base::WeakPtrFactory<blink::WebServiceWorkerContextProxy> proxy_weak_factory;
};

class ServiceWorkerContextClient::NavigationPreloadRequest final
    : public network::mojom::URLLoaderClient {
 public:
  NavigationPreloadRequest(
      int fetch_event_id,
      const GURL& url,
      blink::mojom::FetchEventPreloadHandlePtr preload_handle)
      : fetch_event_id_(fetch_event_id),
        url_(url),
        url_loader_(std::move(preload_handle->url_loader)),
        binding_(this, std::move(preload_handle->url_loader_client_request)) {}

  ~NavigationPreloadRequest() override {}

  void OnReceiveResponse(
      const network::ResourceResponseHead& response_head) override {
    DCHECK(!response_);
    response_ = std::make_unique<blink::WebURLResponse>();
    // TODO(horo): Set report_security_info to true when DevTools is attached.
    const bool report_security_info = false;
    WebURLLoaderImpl::PopulateURLResponse(url_, response_head, response_.get(),
                                          report_security_info,
                                          -1 /* request_id */);
    MaybeReportResponseToClient();
  }

  void OnReceiveRedirect(
      const net::RedirectInfo& redirect_info,
      const network::ResourceResponseHead& response_head) override {
    DCHECK(!response_);
    DCHECK(net::HttpResponseHeaders::IsRedirectResponseCode(
        response_head.headers->response_code()));

    ServiceWorkerContextClient* client =
        ServiceWorkerContextClient::ThreadSpecificInstance();
    if (!client)
      return;
    response_ = std::make_unique<blink::WebURLResponse>();
    WebURLLoaderImpl::PopulateURLResponse(url_, response_head, response_.get(),
                                          false /* report_security_info */,
                                          -1 /* request_id */);
    client->OnNavigationPreloadResponse(fetch_event_id_, std::move(response_),
                                        mojo::ScopedDataPipeConsumerHandle());
    // This will delete |this|.
    client->OnNavigationPreloadComplete(
        fetch_event_id_, response_head.response_start,
        response_head.encoded_data_length, 0 /* encoded_body_length */,
        0 /* decoded_body_length */);
  }

  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback ack_callback) override {
    NOTREACHED();
  }

  void OnReceiveCachedMetadata(const std::vector<uint8_t>& data) override {}

  void OnTransferSizeUpdated(int32_t transfer_size_diff) override {
  }

  void OnStartLoadingResponseBody(
      mojo::ScopedDataPipeConsumerHandle body) override {
    DCHECK(!body_.is_valid());
    body_ = std::move(body);
    MaybeReportResponseToClient();
  }

  void OnComplete(const network::URLLoaderCompletionStatus& status) override {
    if (status.error_code != net::OK) {
      std::string message;
      std::string unsanitized_message;
      if (status.error_code == net::ERR_ABORTED) {
        message =
            "The service worker navigation preload request was cancelled "
            "before 'preloadResponse' settled. If you intend to use "
            "'preloadResponse', use waitUntil() or respondWith() to wait for "
            "the promise to settle.";
      } else {
        message =
            "The service worker navigation preload request failed with a "
            "network error.";
        unsanitized_message =
            "The service worker navigation preload request failed with network "
            "error: " +
            net::ErrorToString(status.error_code) + ".";
      }

      // This will delete |this|.
      ReportErrorToClient(message, unsanitized_message);
      return;
    }

    ServiceWorkerContextClient* client =
        ServiceWorkerContextClient::ThreadSpecificInstance();
    if (!client)
      return;
    if (response_) {
      // When the response body from the server is empty, OnComplete() is called
      // without OnStartLoadingResponseBody().
      DCHECK(!body_.is_valid());
      client->OnNavigationPreloadResponse(fetch_event_id_, std::move(response_),
                                          mojo::ScopedDataPipeConsumerHandle());
    }
    // This will delete |this|.
    client->OnNavigationPreloadComplete(
        fetch_event_id_, status.completion_time, status.encoded_data_length,
        status.encoded_body_length, status.decoded_body_length);
  }

 private:
  void MaybeReportResponseToClient() {
    if (!response_ || !body_.is_valid())
      return;
    ServiceWorkerContextClient* client =
        ServiceWorkerContextClient::ThreadSpecificInstance();
    if (!client)
      return;

    client->OnNavigationPreloadResponse(fetch_event_id_, std::move(response_),
                                        std::move(body_));
  }

  void ReportErrorToClient(const std::string& message,
                           const std::string& unsanitized_message) {
    ServiceWorkerContextClient* client =
        ServiceWorkerContextClient::ThreadSpecificInstance();
    if (!client)
      return;
    // This will delete |this|.
    client->OnNavigationPreloadError(
        fetch_event_id_, std::make_unique<blink::WebServiceWorkerError>(
                             blink::mojom::ServiceWorkerErrorType::kNetwork,
                             blink::WebString::FromUTF8(message),
                             blink::WebString::FromUTF8(unsanitized_message)));
  }

  const int fetch_event_id_;
  const GURL url_;
  network::mojom::URLLoaderPtr url_loader_;
  mojo::Binding<network::mojom::URLLoaderClient> binding_;

  std::unique_ptr<blink::WebURLResponse> response_;
  mojo::ScopedDataPipeConsumerHandle body_;
};

ServiceWorkerContextClient*
ServiceWorkerContextClient::ThreadSpecificInstance() {
  return g_worker_client_tls.Pointer()->Get();
}

ServiceWorkerContextClient::ServiceWorkerContextClient(
    int embedded_worker_id,
    int64_t service_worker_version_id,
    const GURL& service_worker_scope,
    const GURL& script_url,
    bool is_starting_installed_worker,
    RendererPreferences renderer_preferences,
    mojom::ServiceWorkerRequest service_worker_request,
    mojom::ControllerServiceWorkerRequest controller_request,
    mojom::EmbeddedWorkerInstanceHostAssociatedPtrInfo instance_host,
    mojom::ServiceWorkerProviderInfoForStartWorkerPtr provider_info,
    std::unique_ptr<EmbeddedWorkerInstanceClientImpl> embedded_worker_client,
    mojom::EmbeddedWorkerStartTimingPtr start_timing,
    mojom::RendererPreferenceWatcherRequest preference_watcher_request,
    std::unique_ptr<URLLoaderFactoryBundleInfo> subresource_loaders,
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner)
    : embedded_worker_id_(embedded_worker_id),
      service_worker_version_id_(service_worker_version_id),
      service_worker_scope_(service_worker_scope),
      script_url_(script_url),
      is_starting_installed_worker_(is_starting_installed_worker),
      renderer_preferences_(std::move(renderer_preferences)),
      preference_watcher_request_(std::move(preference_watcher_request)),
      main_thread_task_runner_(std::move(main_thread_task_runner)),
      proxy_(nullptr),
      pending_service_worker_request_(std::move(service_worker_request)),
      pending_controller_request_(std::move(controller_request)),
      embedded_worker_client_(std::move(embedded_worker_client)),
      start_timing_(std::move(start_timing)) {
  instance_host_ =
      mojom::ThreadSafeEmbeddedWorkerInstanceHostAssociatedPtr::Create(
          std::move(instance_host), main_thread_task_runner_);

  if (subresource_loaders) {
    if (IsOutOfProcessNetworkService()) {
      // If the network service crashes, this worker self-terminates, so it can
      // be restarted later with a connection to the restarted network
      // service.
      // Note that the default factory is the network service factory. It's set
      // on the start worker sequence.
      network_service_connection_error_handler_holder_.Bind(
          std::move(subresource_loaders->default_factory_info()));
      network_service_connection_error_handler_holder_->Clone(
          mojo::MakeRequest(&subresource_loaders->default_factory_info()));
      network_service_connection_error_handler_holder_
          .set_connection_error_handler(base::BindOnce(
              &ServiceWorkerContextClient::StopWorker, base::Unretained(this)));
    }

    loader_factories_ = base::MakeRefCounted<HostChildURLLoaderFactoryBundle>(
        main_thread_task_runner_);
    loader_factories_->Update(std::make_unique<ChildURLLoaderFactoryBundleInfo>(
                                  std::move(subresource_loaders)),
                              base::nullopt /* subresource_overrides */);
  }

  // Create a content::ServiceWorkerNetworkProvider for this data source so
  // we can observe its requests.
  pending_network_provider_ = ServiceWorkerNetworkProvider::CreateForController(
      std::move(provider_info));
  provider_context_ = pending_network_provider_->context();

  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1("ServiceWorker",
                                    "ServiceWorkerContextClient", this,
                                    "script_url", script_url_.spec());
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1(
      "ServiceWorker", "LOAD_SCRIPT", this, "Source",
      (is_starting_installed_worker_ ? "InstalledScriptsManager"
                                     : "ResourceLoader"));
}

ServiceWorkerContextClient::~ServiceWorkerContextClient() {}

void ServiceWorkerContextClient::WorkerReadyForInspection() {
  DCHECK(main_thread_task_runner_->RunsTasksInCurrentSequence());
  (*instance_host_)->OnReadyForInspection();
}

void ServiceWorkerContextClient::WorkerContextFailedToStart() {
  DCHECK(main_thread_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(!proxy_);

  (*instance_host_)->OnStopped();

  TRACE_EVENT_NESTABLE_ASYNC_END1("ServiceWorker", "ServiceWorkerContextClient",
                                  this, "Status", "WorkerContextFailedToStart");

  DCHECK(embedded_worker_client_);
  embedded_worker_client_->WorkerContextDestroyed();
}

void ServiceWorkerContextClient::FailedToLoadInstalledScript() {
  DCHECK(worker_task_runner_->RunsTasksInCurrentSequence());
  TRACE_EVENT_NESTABLE_ASYNC_END1("ServiceWorker", "LOAD_SCRIPT", this,
                                  "Status", "FailedToLoadInstalledScript");
  // Cleanly send an OnStopped() message instead of just breaking the
  // Mojo connection on termination, for consistency with the other
  // startup failure paths.
  (*instance_host_)->OnStopped();

  // The caller is responsible for terminating the thread which
  // eventually destroys |this|.
}

void ServiceWorkerContextClient::WorkerScriptLoaded() {
  if (!is_starting_installed_worker_)
    (*instance_host_)->OnScriptLoaded();
  TRACE_EVENT_NESTABLE_ASYNC_END0("ServiceWorker", "LOAD_SCRIPT", this);
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("ServiceWorker", "START_WORKER_CONTEXT",
                                    this);
}

void ServiceWorkerContextClient::WorkerContextStarted(
    blink::WebServiceWorkerContextProxy* proxy) {
  DCHECK(!worker_task_runner_.get());
  DCHECK_NE(0, WorkerThread::GetCurrentId());
  worker_task_runner_ = base::ThreadTaskRunnerHandle::Get();
  // g_worker_client_tls.Pointer()->Get() could return nullptr if this context
  // gets deleted before workerContextStarted() is called.
  DCHECK(g_worker_client_tls.Pointer()->Get() == nullptr);
  DCHECK(!proxy_);
  g_worker_client_tls.Pointer()->Set(this);
  proxy_ = proxy;

  // Initialize pending callback maps. This needs to be freed on the
  // same thread before the worker context goes away in
  // willDestroyWorkerContext.
  context_.reset(new WorkerContextData(this));

  DCHECK(pending_service_worker_request_.is_pending());
  DCHECK(pending_controller_request_.is_pending());
  DCHECK(!context_->service_worker_binding.is_bound());
  DCHECK(!context_->controller_impl);
  context_->service_worker_binding.Bind(
      std::move(pending_service_worker_request_));

  if (blink::ServiceWorkerUtils::IsServicificationEnabled()) {
    context_->controller_impl = std::make_unique<ControllerServiceWorkerImpl>(
        std::move(pending_controller_request_), GetWeakPtr());
  }

  TRACE_EVENT_NESTABLE_ASYNC_END0("ServiceWorker", "START_WORKER_CONTEXT",
                                  this);
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("ServiceWorker", "EVALUATE_SCRIPT", this);
}

void ServiceWorkerContextClient::WillEvaluateScript() {
  DCHECK(worker_task_runner_->RunsTasksInCurrentSequence());
  start_timing_->script_evaluation_start_time = base::TimeTicks::Now();
  (*instance_host_)->OnScriptEvaluationStart();
}

void ServiceWorkerContextClient::DidEvaluateScript(bool success) {
  DCHECK(worker_task_runner_->RunsTasksInCurrentSequence());
  start_timing_->script_evaluation_end_time = base::TimeTicks::Now();

  blink::mojom::ServiceWorkerStartStatus status =
      success ? blink::mojom::ServiceWorkerStartStatus::kNormalCompletion
              : blink::mojom::ServiceWorkerStartStatus::kAbruptCompletion;

  // Schedule a task to send back WorkerStarted asynchronously, so we can be
  // sure that the worker is really started.
  // TODO(falken): Is this really needed? Probably if kNormalCompletion, the
  // worker is definitely running so we can SendStartWorker immediately.
  worker_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&ServiceWorkerContextClient::SendWorkerStarted,
                                GetWeakPtr(), status));

  TRACE_EVENT_NESTABLE_ASYNC_END1("ServiceWorker", "EVALUATE_SCRIPT", this,
                                  "Status",
                                  ServiceWorkerUtils::MojoEnumToString(status));
}

void ServiceWorkerContextClient::DidInitializeWorkerContext(
    v8::Local<v8::Context> context) {
  GetContentClient()
      ->renderer()
      ->DidInitializeServiceWorkerContextOnWorkerThread(
          context, service_worker_version_id_, service_worker_scope_,
          script_url_);
}

void ServiceWorkerContextClient::WillDestroyWorkerContext(
    v8::Local<v8::Context> context) {
  // At this point WillStopCurrentWorkerThread is already called, so
  // worker_task_runner_->RunsTasksInCurrentSequence() returns false
  // (while we're still on the worker thread).
  proxy_ = nullptr;

  blob_registry_.reset();

  // We have to clear callbacks now, as they need to be freed on the
  // same thread.
  context_.reset();

  // This also lets the message filter stop dispatching messages to
  // this client.
  g_worker_client_tls.Pointer()->Set(nullptr);

  GetContentClient()->renderer()->WillDestroyServiceWorkerContextOnWorkerThread(
      context, service_worker_version_id_, service_worker_scope_, script_url_);
}

void ServiceWorkerContextClient::WorkerContextDestroyed() {
  DCHECK(g_worker_client_tls.Pointer()->Get() == nullptr);

  // TODO(shimazu): The signals to the browser should be in the order:
  // (1) WorkerStopped (via mojo call EmbeddedWorkerInstanceHost.OnStopped())
  // (2) ProviderDestroyed (via mojo call
  // ServiceWorkerDispatcherHost.OnProviderDestroyed()), this is triggered by
  // the following EmbeddedWorkerInstanceClientImpl::WorkerContextDestroyed(),
  // which will eventually lead to destruction of the service worker provider.
  // But currently EmbeddedWorkerInstanceHost interface is associated with
  // EmbeddedWorkerInstanceClient interface, and ServiceWorkerDispatcherHost
  // interface is associated with the IPC channel, since they are using
  // different mojo message pipes, the FIFO ordering can not be guaranteed now.
  // This will be solved once ServiceWorkerProvider{Host,Client} are mojoified
  // and they are also associated with EmbeddedWorkerInstanceClient in other CLs
  // (https://crrev.com/2653493009 and https://crrev.com/2779763004).
  (*instance_host_)->OnStopped();

  DCHECK(embedded_worker_client_);
  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&EmbeddedWorkerInstanceClientImpl::WorkerContextDestroyed,
                     std::move(embedded_worker_client_)));
}

void ServiceWorkerContextClient::CountFeature(
    blink::mojom::WebFeature feature) {
  (*instance_host_)->CountFeature(feature);
}

void ServiceWorkerContextClient::ReportException(
    const blink::WebString& error_message,
    int line_number,
    int column_number,
    const blink::WebString& source_url) {
  (*instance_host_)
      ->OnReportException(error_message.Utf16(), line_number, column_number,
                          blink::WebStringToGURL(source_url));
}

void ServiceWorkerContextClient::ReportConsoleMessage(
    int source,
    int level,
    const blink::WebString& message,
    int line_number,
    const blink::WebString& source_url) {
  (*instance_host_)
      ->OnReportConsoleMessage(source, level, message.Utf16(), line_number,
                               blink::WebStringToGURL(source_url));
}

void ServiceWorkerContextClient::DidHandleActivateEvent(
    int request_id,
    blink::mojom::ServiceWorkerEventStatus status,
    base::TimeTicks event_dispatch_time) {
  TRACE_EVENT_WITH_FLOW1("ServiceWorker",
                         "ServiceWorkerContextClient::DidHandleActivateEvent",
                         TRACE_ID_WITH_SCOPE(kServiceWorkerContextClientScope,
                                             TRACE_ID_LOCAL(request_id)),
                         TRACE_EVENT_FLAG_FLOW_IN, "status",
                         ServiceWorkerUtils::MojoEnumToString(status));
  RunEventCallback(&context_->activate_event_callbacks,
                   context_->timeout_timer.get(), request_id, status,
                   event_dispatch_time);
}

void ServiceWorkerContextClient::DidHandleBackgroundFetchAbortEvent(
    int request_id,
    blink::mojom::ServiceWorkerEventStatus status,
    base::TimeTicks event_dispatch_time) {
  TRACE_EVENT_WITH_FLOW1(
      "ServiceWorker",
      "ServiceWorkerContextClient::DidHandleBackgroundFetchAbortEvent",
      TRACE_ID_WITH_SCOPE(kServiceWorkerContextClientScope,
                          TRACE_ID_LOCAL(request_id)),
      TRACE_EVENT_FLAG_FLOW_IN, "status",
      ServiceWorkerUtils::MojoEnumToString(status));
  RunEventCallback(&context_->background_fetch_abort_event_callbacks,
                   context_->timeout_timer.get(), request_id, status,
                   event_dispatch_time);
}

void ServiceWorkerContextClient::DidHandleBackgroundFetchClickEvent(
    int request_id,
    blink::mojom::ServiceWorkerEventStatus status,
    base::TimeTicks event_dispatch_time) {
  TRACE_EVENT_WITH_FLOW1(
      "ServiceWorker",
      "ServiceWorkerContextClient::DidHandleBackgroundFetchClickEvent",
      TRACE_ID_WITH_SCOPE(kServiceWorkerContextClientScope,
                          TRACE_ID_LOCAL(request_id)),
      TRACE_EVENT_FLAG_FLOW_IN, "status",
      ServiceWorkerUtils::MojoEnumToString(status));
  RunEventCallback(&context_->background_fetch_click_event_callbacks,
                   context_->timeout_timer.get(), request_id, status,
                   event_dispatch_time);
}

void ServiceWorkerContextClient::DidHandleBackgroundFetchFailEvent(
    int request_id,
    blink::mojom::ServiceWorkerEventStatus status,
    base::TimeTicks event_dispatch_time) {
  TRACE_EVENT_WITH_FLOW1(
      "ServiceWorker",
      "ServiceWorkerContextClient::DidHandleBackgroundFetchFailEvent",
      TRACE_ID_WITH_SCOPE(kServiceWorkerContextClientScope,
                          TRACE_ID_LOCAL(request_id)),
      TRACE_EVENT_FLAG_FLOW_IN, "status",
      ServiceWorkerUtils::MojoEnumToString(status));
  RunEventCallback(&context_->background_fetch_fail_event_callbacks,
                   context_->timeout_timer.get(), request_id, status,
                   event_dispatch_time);
}

void ServiceWorkerContextClient::DidHandleBackgroundFetchSuccessEvent(
    int request_id,
    blink::mojom::ServiceWorkerEventStatus status,
    base::TimeTicks event_dispatch_time) {
  TRACE_EVENT_WITH_FLOW1(
      "ServiceWorker",
      "ServiceWorkerContextClient::DidHandleBackgroundFetchSuccessEvent",
      TRACE_ID_WITH_SCOPE(kServiceWorkerContextClientScope,
                          TRACE_ID_LOCAL(request_id)),
      TRACE_EVENT_FLAG_FLOW_IN, "status",
      ServiceWorkerUtils::MojoEnumToString(status));
  RunEventCallback(&context_->background_fetched_event_callbacks,
                   context_->timeout_timer.get(), request_id, status,
                   event_dispatch_time);
}

void ServiceWorkerContextClient::DidHandleCookieChangeEvent(
    int request_id,
    blink::mojom::ServiceWorkerEventStatus status,
    base::TimeTicks event_dispatch_time) {
  TRACE_EVENT_WITH_FLOW1(
      "ServiceWorker", "ServiceWorkerContextClient::DidHandleCookieChangeEvent",
      TRACE_ID_WITH_SCOPE(kServiceWorkerContextClientScope,
                          TRACE_ID_LOCAL(request_id)),
      TRACE_EVENT_FLAG_FLOW_IN, "status",
      ServiceWorkerUtils::MojoEnumToString(status));
  RunEventCallback(&context_->cookie_change_event_callbacks,
                   context_->timeout_timer.get(), request_id, status,
                   event_dispatch_time);
}

void ServiceWorkerContextClient::DidHandleExtendableMessageEvent(
    int request_id,
    blink::mojom::ServiceWorkerEventStatus status,
    base::TimeTicks event_dispatch_time) {
  TRACE_EVENT_WITH_FLOW1(
      "ServiceWorker",
      "ServiceWorkerContextClient::DidHandleExtendableMessageEvent",
      TRACE_ID_WITH_SCOPE(kServiceWorkerContextClientScope,
                          TRACE_ID_LOCAL(request_id)),
      TRACE_EVENT_FLAG_FLOW_IN, "status",
      ServiceWorkerUtils::MojoEnumToString(status));
  RunEventCallback(&context_->message_event_callbacks,
                   context_->timeout_timer.get(), request_id, status,
                   event_dispatch_time);
}

void ServiceWorkerContextClient::DidHandleInstallEvent(
    int event_id,
    blink::mojom::ServiceWorkerEventStatus status,
    base::TimeTicks event_dispatch_time) {
  TRACE_EVENT_WITH_FLOW1("ServiceWorker",
                         "ServiceWorkerContextClient::DidHandleInstallEvent",
                         TRACE_ID_WITH_SCOPE(kServiceWorkerContextClientScope,
                                             TRACE_ID_LOCAL(event_id)),
                         TRACE_EVENT_FLAG_FLOW_IN, "status",
                         ServiceWorkerUtils::MojoEnumToString(status));
  RunEventCallback(&context_->install_event_callbacks,
                   context_->timeout_timer.get(), event_id, status,
                   proxy_->HasFetchEventHandler(), event_dispatch_time);
}

void ServiceWorkerContextClient::RespondToFetchEventWithNoResponse(
    int fetch_event_id,
    base::TimeTicks event_dispatch_time,
    base::TimeTicks respond_with_settled_time) {
  TRACE_EVENT_WITH_FLOW0(
      "ServiceWorker",
      "ServiceWorkerContextClient::RespondToFetchEventWithNoResponse",
      TRACE_ID_WITH_SCOPE(kServiceWorkerContextClientScope,
                          TRACE_ID_LOCAL(fetch_event_id)),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  // Changed DCHECK to CHECK temporary: https://crbug.com/889567.
  CHECK(base::ContainsKey(context_->fetch_response_callbacks, fetch_event_id));
  const blink::mojom::ServiceWorkerFetchResponseCallbackPtr& response_callback =
      context_->fetch_response_callbacks[fetch_event_id];
  CHECK(response_callback.is_bound());

  auto timing = blink::mojom::ServiceWorkerFetchEventTiming::New();
  timing->dispatch_event_time = event_dispatch_time;
  timing->respond_with_settled_time = respond_with_settled_time;

  response_callback->OnFallback(std::move(timing));
  context_->fetch_response_callbacks.erase(fetch_event_id);
}

void ServiceWorkerContextClient::RespondToFetchEvent(
    int fetch_event_id,
    const blink::WebServiceWorkerResponse& web_response,
    base::TimeTicks event_dispatch_time,
    base::TimeTicks respond_with_settled_time) {
  TRACE_EVENT_WITH_FLOW0(
      "ServiceWorker", "ServiceWorkerContextClient::RespondToFetchEvent",
      TRACE_ID_WITH_SCOPE(kServiceWorkerContextClientScope,
                          TRACE_ID_LOCAL(fetch_event_id)),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);

  // These CHECKs are for debugging: https://crbug.com/889567.
  CHECK(context_);
  if (!base::ContainsKey(context_->fetch_response_callbacks, fetch_event_id)) {
    bool does_exist_fetch_event_callback =
        base::ContainsKey(context_->fetch_event_callbacks, fetch_event_id);
    base::debug::Alias(&does_exist_fetch_event_callback);
    CHECK(false);
    return;
  }

  blink::mojom::FetchAPIResponsePtr response(
      GetFetchAPIResponseFromWebResponse(web_response));
  const blink::mojom::ServiceWorkerFetchResponseCallbackPtr& response_callback =
      context_->fetch_response_callbacks[fetch_event_id];

  auto timing = blink::mojom::ServiceWorkerFetchEventTiming::New();
  timing->dispatch_event_time = event_dispatch_time;
  timing->respond_with_settled_time = respond_with_settled_time;

  response_callback->OnResponse(std::move(response), std::move(timing));
  context_->fetch_response_callbacks.erase(fetch_event_id);
}

void ServiceWorkerContextClient::RespondToFetchEventWithResponseStream(
    int fetch_event_id,
    const blink::WebServiceWorkerResponse& web_response,
    blink::WebServiceWorkerStreamHandle* web_body_as_stream,
    base::TimeTicks event_dispatch_time,
    base::TimeTicks respond_with_settled_time) {
  TRACE_EVENT_WITH_FLOW0(
      "ServiceWorker",
      "ServiceWorkerContextClient::RespondToFetchEventWithResponseStream",
      TRACE_ID_WITH_SCOPE(kServiceWorkerContextClientScope,
                          TRACE_ID_LOCAL(fetch_event_id)),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  // Changed DCHECK to CHECK temporary: https://crbug.com/889567.
  CHECK(base::ContainsKey(context_->fetch_response_callbacks, fetch_event_id));
  blink::mojom::FetchAPIResponsePtr response(
      GetFetchAPIResponseFromWebResponse(web_response));
  const blink::mojom::ServiceWorkerFetchResponseCallbackPtr& response_callback =
      context_->fetch_response_callbacks[fetch_event_id];
  blink::mojom::ServiceWorkerStreamHandlePtr body_as_stream =
      blink::mojom::ServiceWorkerStreamHandle::New();
  blink::mojom::ServiceWorkerStreamCallbackPtr callback_ptr;
  body_as_stream->callback_request = mojo::MakeRequest(&callback_ptr);
  body_as_stream->stream = web_body_as_stream->DrainStreamDataPipe();
  DCHECK(body_as_stream->stream.is_valid());

  web_body_as_stream->SetListener(std::make_unique<StreamHandleListener>(
      std::move(callback_ptr),
      context_->timeout_timer->CreateStayAwakeToken()));

  auto timing = blink::mojom::ServiceWorkerFetchEventTiming::New();
  timing->dispatch_event_time = event_dispatch_time;
  timing->respond_with_settled_time = respond_with_settled_time;

  response_callback->OnResponseStream(
      std::move(response), std::move(body_as_stream), std::move(timing));
  context_->fetch_response_callbacks.erase(fetch_event_id);
}

void ServiceWorkerContextClient::DidHandleFetchEvent(
    int event_id,
    blink::mojom::ServiceWorkerEventStatus status,
    base::TimeTicks event_dispatch_time) {
  // This TRACE_EVENT is used for perf benchmark to confirm if all of fetch
  // events have completed. (crbug.com/736697)
  TRACE_EVENT_WITH_FLOW1("ServiceWorker",
                         "ServiceWorkerContextClient::DidHandleFetchEvent",
                         TRACE_ID_WITH_SCOPE(kServiceWorkerContextClientScope,
                                             TRACE_ID_LOCAL(event_id)),
                         TRACE_EVENT_FLAG_FLOW_IN, "status",
                         ServiceWorkerUtils::MojoEnumToString(status));
  if (!RunEventCallback(&context_->fetch_event_callbacks,
                        context_->timeout_timer.get(), event_id, status,
                        event_dispatch_time)) {
    // The event may have been aborted. Its response callback also needs to be
    // deleted.
    context_->fetch_response_callbacks.erase(event_id);
  } else {
    // |fetch_response_callback| should be used before settling a promise for
    // waitUntil().
    // Changed DCHECK to CHECK temporary: https://crbug.com/889567.
    CHECK(!base::ContainsKey(context_->fetch_response_callbacks, event_id));
  }
}

void ServiceWorkerContextClient::DidHandleNotificationClickEvent(
    int request_id,
    blink::mojom::ServiceWorkerEventStatus status,
    base::TimeTicks event_dispatch_time) {
  TRACE_EVENT_WITH_FLOW1(
      "ServiceWorker",
      "ServiceWorkerContextClient::DidHandleNotificationClickEvent",
      TRACE_ID_WITH_SCOPE(kServiceWorkerContextClientScope,
                          TRACE_ID_LOCAL(request_id)),
      TRACE_EVENT_FLAG_FLOW_IN, "status",
      ServiceWorkerUtils::MojoEnumToString(status));
  RunEventCallback(&context_->notification_click_event_callbacks,
                   context_->timeout_timer.get(), request_id, status,
                   event_dispatch_time);
}

void ServiceWorkerContextClient::DidHandleNotificationCloseEvent(
    int request_id,
    blink::mojom::ServiceWorkerEventStatus status,
    base::TimeTicks event_dispatch_time) {
  TRACE_EVENT_WITH_FLOW1(
      "ServiceWorker",
      "ServiceWorkerContextClient::DidHandleNotificationCloseEvent",
      TRACE_ID_WITH_SCOPE(kServiceWorkerContextClientScope,
                          TRACE_ID_LOCAL(request_id)),
      TRACE_EVENT_FLAG_FLOW_IN, "status",
      ServiceWorkerUtils::MojoEnumToString(status));
  RunEventCallback(&context_->notification_close_event_callbacks,
                   context_->timeout_timer.get(), request_id, status,
                   event_dispatch_time);
}

void ServiceWorkerContextClient::DidHandlePushEvent(
    int request_id,
    blink::mojom::ServiceWorkerEventStatus status,
    base::TimeTicks event_dispatch_time) {
  TRACE_EVENT_WITH_FLOW1("ServiceWorker",
                         "ServiceWorkerContextClient::DidHandlePushEvent",
                         TRACE_ID_WITH_SCOPE(kServiceWorkerContextClientScope,
                                             TRACE_ID_LOCAL(request_id)),
                         TRACE_EVENT_FLAG_FLOW_IN, "status",
                         ServiceWorkerUtils::MojoEnumToString(status));
  RunEventCallback(&context_->push_event_callbacks,
                   context_->timeout_timer.get(), request_id, status,
                   event_dispatch_time);
}

void ServiceWorkerContextClient::DidHandleSyncEvent(
    int request_id,
    blink::mojom::ServiceWorkerEventStatus status,
    base::TimeTicks event_dispatch_time) {
  TRACE_EVENT_WITH_FLOW1("ServiceWorker",
                         "ServiceWorkerContextClient::DidHandleSyncEvent",
                         TRACE_ID_WITH_SCOPE(kServiceWorkerContextClientScope,
                                             TRACE_ID_LOCAL(request_id)),
                         TRACE_EVENT_FLAG_FLOW_IN, "status",
                         ServiceWorkerUtils::MojoEnumToString(status));
  RunEventCallback(&context_->sync_event_callbacks,
                   context_->timeout_timer.get(), request_id, status,
                   event_dispatch_time);
}

void ServiceWorkerContextClient::RespondToAbortPaymentEvent(
    int event_id,
    bool payment_aborted,
    base::TimeTicks event_dispatch_time) {
  TRACE_EVENT_WITH_FLOW0(
      "ServiceWorker", "ServiceWorkerContextClient::RespondToAbortPaymentEvent",
      TRACE_ID_WITH_SCOPE(kServiceWorkerContextClientScope,
                          TRACE_ID_LOCAL(event_id)),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  DCHECK(base::ContainsKey(context_->abort_payment_result_callbacks, event_id));
  const payments::mojom::PaymentHandlerResponseCallbackPtr& result_callback =
      context_->abort_payment_result_callbacks[event_id];
  result_callback->OnResponseForAbortPayment(payment_aborted,
                                             event_dispatch_time);
  context_->abort_payment_result_callbacks.erase(event_id);
}

void ServiceWorkerContextClient::DidHandleAbortPaymentEvent(
    int event_id,
    blink::mojom::ServiceWorkerEventStatus status,
    base::TimeTicks event_dispatch_time) {
  TRACE_EVENT_WITH_FLOW1(
      "ServiceWorker", "ServiceWorkerContextClient::DidHandleAbortPaymentEvent",
      TRACE_ID_WITH_SCOPE(kServiceWorkerContextClientScope,
                          TRACE_ID_LOCAL(event_id)),
      TRACE_EVENT_FLAG_FLOW_IN, "status",
      ServiceWorkerUtils::MojoEnumToString(status));
  if (RunEventCallback(&context_->abort_payment_event_callbacks,
                       context_->timeout_timer.get(), event_id, status,
                       event_dispatch_time)) {
    context_->abort_payment_result_callbacks.erase(event_id);
  }
}

void ServiceWorkerContextClient::RespondToCanMakePaymentEvent(
    int event_id,
    bool can_make_payment,
    base::TimeTicks event_dispatch_time) {
  TRACE_EVENT_WITH_FLOW0(
      "ServiceWorker",
      "ServiceWorkerContextClient::RespondToCanMakePaymentEvent",
      TRACE_ID_WITH_SCOPE(kServiceWorkerContextClientScope,
                          TRACE_ID_LOCAL(event_id)),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  DCHECK(
      base::ContainsKey(context_->can_make_payment_result_callbacks, event_id));
  const payments::mojom::PaymentHandlerResponseCallbackPtr& result_callback =
      context_->can_make_payment_result_callbacks[event_id];
  result_callback->OnResponseForCanMakePayment(can_make_payment,
                                               event_dispatch_time);
  context_->can_make_payment_result_callbacks.erase(event_id);
}

void ServiceWorkerContextClient::DidHandleCanMakePaymentEvent(
    int event_id,
    blink::mojom::ServiceWorkerEventStatus status,
    base::TimeTicks event_dispatch_time) {
  TRACE_EVENT_WITH_FLOW1(
      "ServiceWorker",
      "ServiceWorkerContextClient::DidHandleCanMakePaymentEvent",
      TRACE_ID_WITH_SCOPE(kServiceWorkerContextClientScope,
                          TRACE_ID_LOCAL(event_id)),
      TRACE_EVENT_FLAG_FLOW_IN, "status",
      ServiceWorkerUtils::MojoEnumToString(status));
  if (RunEventCallback(&context_->can_make_payment_event_callbacks,
                       context_->timeout_timer.get(), event_id, status,
                       event_dispatch_time)) {
    context_->can_make_payment_result_callbacks.erase(event_id);
  }
}

void ServiceWorkerContextClient::RespondToPaymentRequestEvent(
    int payment_request_id,
    const blink::WebPaymentHandlerResponse& web_response,
    base::TimeTicks event_dispatch_time) {
  TRACE_EVENT_WITH_FLOW0(
      "ServiceWorker",
      "ServiceWorkerContextClient::RespondToPaymentRequestEvent",
      TRACE_ID_WITH_SCOPE(kServiceWorkerContextClientScope,
                          TRACE_ID_LOCAL(payment_request_id)),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  DCHECK(base::ContainsKey(context_->payment_response_callbacks,
                           payment_request_id));
  const payments::mojom::PaymentHandlerResponseCallbackPtr& response_callback =
      context_->payment_response_callbacks[payment_request_id];
  payments::mojom::PaymentHandlerResponsePtr response =
      payments::mojom::PaymentHandlerResponse::New();
  response->method_name = web_response.method_name.Utf8();
  response->stringified_details = web_response.stringified_details.Utf8();
  response_callback->OnResponseForPaymentRequest(std::move(response),
                                                 event_dispatch_time);
  context_->payment_response_callbacks.erase(payment_request_id);
}

void ServiceWorkerContextClient::DidHandlePaymentRequestEvent(
    int payment_request_id,
    blink::mojom::ServiceWorkerEventStatus status,
    base::TimeTicks event_dispatch_time) {
  TRACE_EVENT_WITH_FLOW1(
      "ServiceWorker",
      "ServiceWorkerContextClient::DidHandlePaymentRequestEvent",
      TRACE_ID_WITH_SCOPE(kServiceWorkerContextClientScope,
                          TRACE_ID_LOCAL(payment_request_id)),
      TRACE_EVENT_FLAG_FLOW_IN, "status",
      ServiceWorkerUtils::MojoEnumToString(status));
  if (RunEventCallback(&context_->payment_request_event_callbacks,
                       context_->timeout_timer.get(), payment_request_id,
                       status, event_dispatch_time)) {
    context_->payment_response_callbacks.erase(payment_request_id);
  }
}

std::unique_ptr<blink::WebServiceWorkerNetworkProvider>
ServiceWorkerContextClient::CreateServiceWorkerNetworkProvider() {
  DCHECK(main_thread_task_runner_->RunsTasksInCurrentSequence());
  return std::make_unique<WebServiceWorkerNetworkProviderImpl>(
      std::move(pending_network_provider_));
}

std::unique_ptr<blink::WebWorkerFetchContext>
ServiceWorkerContextClient::CreateServiceWorkerFetchContext(
    blink::WebServiceWorkerNetworkProvider* provider) {
  DCHECK(main_thread_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(preference_watcher_request_.is_pending());

  scoped_refptr<ChildURLLoaderFactoryBundle> url_loader_factory_bundle;
  if (loader_factories_) {
    url_loader_factory_bundle = loader_factories_;
  } else {
    url_loader_factory_bundle = RenderThreadImpl::current()
                                    ->blink_platform_impl()
                                    ->CreateDefaultURLLoaderFactoryBundle();
  }
  DCHECK(url_loader_factory_bundle);

  std::unique_ptr<network::SharedURLLoaderFactoryInfo>
      script_loader_factory_info;
  if (blink::ServiceWorkerUtils::IsServicificationEnabled()) {
    // TODO(crbug.com/796425): Temporarily wrap the raw
    // mojom::URLLoaderFactory pointer into SharedURLLoaderFactory.
    script_loader_factory_info =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            static_cast<WebServiceWorkerNetworkProviderImpl*>(provider)
                ->script_loader_factory())
            ->Clone();
  }

  return std::make_unique<ServiceWorkerFetchContextImpl>(
      renderer_preferences_, script_url_, url_loader_factory_bundle->Clone(),
      std::move(script_loader_factory_info), provider_context_->provider_id(),
      GetContentClient()->renderer()->CreateURLLoaderThrottleProvider(
          URLLoaderThrottleProviderType::kWorker),
      GetContentClient()
          ->renderer()
          ->CreateWebSocketHandshakeThrottleProvider(),
      std::move(preference_watcher_request_));
}

void ServiceWorkerContextClient::DispatchOrQueueFetchEvent(
    blink::mojom::DispatchFetchEventParamsPtr params,
    blink::mojom::ServiceWorkerFetchResponseCallbackPtr response_callback,
    DispatchFetchEventCallback callback) {
  TRACE_EVENT2("ServiceWorker",
               "ServiceWorkerContextClient::DispatchOrQueueFetchEvent", "url",
               params->request.url.spec(), "queued",
               RequestedTermination() ? "true" : "false");
  if (RequestedTermination()) {
    context_->timeout_timer->PushPendingTask(base::BindOnce(
        &ServiceWorkerContextClient::DispatchFetchEvent, GetWeakPtr(),
        std::move(params), std::move(response_callback), std::move(callback)));
    return;
  }
  DispatchFetchEvent(std::move(params), std::move(response_callback),
                     std::move(callback));
}

void ServiceWorkerContextClient::DispatchSyncEvent(
    const std::string& tag,
    bool last_chance,
    base::TimeDelta timeout,
    DispatchSyncEventCallback callback) {
  int request_id = context_->timeout_timer->StartEventWithCustomTimeout(
      CreateAbortCallback(&context_->sync_event_callbacks), timeout);
  context_->sync_event_callbacks.emplace(request_id, std::move(callback));
  TRACE_EVENT_WITH_FLOW0("ServiceWorker",
                         "ServiceWorkerContextClient::DispatchSyncEvent",
                         TRACE_ID_WITH_SCOPE(kServiceWorkerContextClientScope,
                                             TRACE_ID_LOCAL(request_id)),
                         TRACE_EVENT_FLAG_FLOW_OUT);

  // TODO(jkarlin): Make this blink::WebString::FromUTF8Lenient once
  // https://crrev.com/1768063002/ lands.
  proxy_->DispatchSyncEvent(request_id, blink::WebString::FromUTF8(tag),
                            last_chance);
}

void ServiceWorkerContextClient::DispatchAbortPaymentEvent(
    payments::mojom::PaymentHandlerResponseCallbackPtr response_callback,
    DispatchAbortPaymentEventCallback callback) {
  int event_id = context_->timeout_timer->StartEvent(
      CreateAbortCallback(&context_->abort_payment_event_callbacks));
  context_->abort_payment_event_callbacks.emplace(event_id,
                                                  std::move(callback));
  context_->abort_payment_result_callbacks.emplace(
      event_id, std::move(response_callback));
  TRACE_EVENT_WITH_FLOW0(
      "ServiceWorker", "ServiceWorkerContextClient::DispatchAbortPaymentEvent",
      TRACE_ID_WITH_SCOPE(kServiceWorkerContextClientScope,
                          TRACE_ID_LOCAL(event_id)),
      TRACE_EVENT_FLAG_FLOW_OUT);
  proxy_->DispatchAbortPaymentEvent(event_id);
}

void ServiceWorkerContextClient::DispatchCanMakePaymentEvent(
    payments::mojom::CanMakePaymentEventDataPtr eventData,
    payments::mojom::PaymentHandlerResponseCallbackPtr response_callback,
    DispatchCanMakePaymentEventCallback callback) {
  int event_id = context_->timeout_timer->StartEvent(
      CreateAbortCallback(&context_->can_make_payment_event_callbacks));
  context_->can_make_payment_event_callbacks.emplace(event_id,
                                                     std::move(callback));
  context_->can_make_payment_result_callbacks.emplace(
      event_id, std::move(response_callback));
  TRACE_EVENT_WITH_FLOW0(
      "ServiceWorker",
      "ServiceWorkerContextClient::DispatchCanMakePaymentEvent",
      TRACE_ID_WITH_SCOPE(kServiceWorkerContextClientScope,
                          TRACE_ID_LOCAL(event_id)),
      TRACE_EVENT_FLAG_FLOW_OUT);

  blink::WebCanMakePaymentEventData webEventData =
      mojo::ConvertTo<blink::WebCanMakePaymentEventData>(std::move(eventData));
  proxy_->DispatchCanMakePaymentEvent(event_id, webEventData);
}

void ServiceWorkerContextClient::DispatchPaymentRequestEvent(
    payments::mojom::PaymentRequestEventDataPtr eventData,
    payments::mojom::PaymentHandlerResponseCallbackPtr response_callback,
    DispatchPaymentRequestEventCallback callback) {
  int event_id = context_->timeout_timer->StartEvent(
      CreateAbortCallback(&context_->payment_request_event_callbacks));
  context_->payment_request_event_callbacks.emplace(event_id,
                                                    std::move(callback));
  context_->payment_response_callbacks.emplace(event_id,
                                               std::move(response_callback));
  TRACE_EVENT_WITH_FLOW0(
      "ServiceWorker",
      "ServiceWorkerContextClient::DispatchPaymentRequestEvent",
      TRACE_ID_WITH_SCOPE(kServiceWorkerContextClientScope,
                          TRACE_ID_LOCAL(event_id)),
      TRACE_EVENT_FLAG_FLOW_OUT);

  blink::WebPaymentRequestEventData webEventData =
      mojo::ConvertTo<blink::WebPaymentRequestEventData>(std::move(eventData));
  proxy_->DispatchPaymentRequestEvent(event_id, webEventData);
}

void ServiceWorkerContextClient::SendWorkerStarted(
    blink::mojom::ServiceWorkerStartStatus status) {
  DCHECK(worker_task_runner_->RunsTasksInCurrentSequence());

  if (GetContentClient()->renderer()) {  // nullptr in unit_tests.
    GetContentClient()->renderer()->DidStartServiceWorkerContextOnWorkerThread(
        service_worker_version_id_, service_worker_scope_, script_url_);
  }

  (*instance_host_)
      ->OnStarted(status, WorkerThread::GetCurrentId(),
                  std::move(start_timing_));
  TRACE_EVENT_NESTABLE_ASYNC_END0("ServiceWorker", "ServiceWorkerContextClient",
                                  this);

  // Start the idle timer.
  context_->timeout_timer =
      std::make_unique<ServiceWorkerTimeoutTimer>(base::BindRepeating(
          &ServiceWorkerContextClient::OnIdleTimeout, base::Unretained(this)));
}

void ServiceWorkerContextClient::DispatchActivateEvent(
    DispatchActivateEventCallback callback) {
  int request_id = context_->timeout_timer->StartEvent(
      CreateAbortCallback(&context_->activate_event_callbacks));
  context_->activate_event_callbacks.emplace(request_id, std::move(callback));
  TRACE_EVENT_WITH_FLOW0("ServiceWorker",
                         "ServiceWorkerContextClient::DispatchActivateEvent",
                         TRACE_ID_WITH_SCOPE(kServiceWorkerContextClientScope,
                                             TRACE_ID_LOCAL(request_id)),
                         TRACE_EVENT_FLAG_FLOW_OUT);
  proxy_->DispatchActivateEvent(request_id);
}

void ServiceWorkerContextClient::DispatchBackgroundFetchAbortEvent(
    const BackgroundFetchRegistration& registration,
    DispatchBackgroundFetchAbortEventCallback callback) {
  int request_id = context_->timeout_timer->StartEvent(
      CreateAbortCallback(&context_->background_fetch_abort_event_callbacks));
  context_->background_fetch_abort_event_callbacks.emplace(request_id,
                                                           std::move(callback));
  TRACE_EVENT_WITH_FLOW0(
      "ServiceWorker",
      "ServiceWorkerContextClient::DispatchBackgroundFetchAbortEvent",
      TRACE_ID_WITH_SCOPE(kServiceWorkerContextClientScope,
                          TRACE_ID_LOCAL(request_id)),
      TRACE_EVENT_FLAG_FLOW_OUT);

  proxy_->DispatchBackgroundFetchAbortEvent(
      request_id, ToWebBackgroundFetchRegistration(registration));
}

void ServiceWorkerContextClient::DispatchBackgroundFetchClickEvent(
    const BackgroundFetchRegistration& registration,
    DispatchBackgroundFetchClickEventCallback callback) {
  int request_id = context_->timeout_timer->StartEvent(
      CreateAbortCallback(&context_->background_fetch_click_event_callbacks));
  context_->background_fetch_click_event_callbacks.emplace(request_id,
                                                           std::move(callback));
  TRACE_EVENT_WITH_FLOW0(
      "ServiceWorker",
      "ServiceWorkerContextClient::DispatchBackgroundFetchClickEvent",
      TRACE_ID_WITH_SCOPE(kServiceWorkerContextClientScope,
                          TRACE_ID_LOCAL(request_id)),
      TRACE_EVENT_FLAG_FLOW_OUT);

  proxy_->DispatchBackgroundFetchClickEvent(
      request_id, ToWebBackgroundFetchRegistration(registration));
}

void ServiceWorkerContextClient::DispatchBackgroundFetchFailEvent(
    const BackgroundFetchRegistration& registration,
    DispatchBackgroundFetchFailEventCallback callback) {
  int request_id = context_->timeout_timer->StartEvent(
      CreateAbortCallback(&context_->background_fetch_fail_event_callbacks));
  context_->background_fetch_fail_event_callbacks.emplace(request_id,
                                                          std::move(callback));
  TRACE_EVENT_WITH_FLOW0(
      "ServiceWorker",
      "ServiceWorkerContextClient::DispatchBackgroundFetchFailEvent",
      TRACE_ID_WITH_SCOPE(kServiceWorkerContextClientScope,
                          TRACE_ID_LOCAL(request_id)),
      TRACE_EVENT_FLAG_FLOW_OUT);

  proxy_->DispatchBackgroundFetchFailEvent(
      request_id, ToWebBackgroundFetchRegistration(registration));
}

void ServiceWorkerContextClient::DispatchBackgroundFetchSuccessEvent(
    const BackgroundFetchRegistration& registration,
    DispatchBackgroundFetchSuccessEventCallback callback) {
  int request_id = context_->timeout_timer->StartEvent(
      CreateAbortCallback(&context_->background_fetched_event_callbacks));
  context_->background_fetched_event_callbacks.emplace(request_id,
                                                       std::move(callback));
  TRACE_EVENT_WITH_FLOW0(
      "ServiceWorker",
      "ServiceWorkerContextClient::DispatchBackgroundFetchSuccessEvent",
      TRACE_ID_WITH_SCOPE(kServiceWorkerContextClientScope,
                          TRACE_ID_LOCAL(request_id)),
      TRACE_EVENT_FLAG_FLOW_OUT);

  proxy_->DispatchBackgroundFetchSuccessEvent(
      request_id, ToWebBackgroundFetchRegistration(registration));
}

void ServiceWorkerContextClient::InitializeGlobalScope(
    blink::mojom::ServiceWorkerHostAssociatedPtrInfo service_worker_host,
    blink::mojom::ServiceWorkerRegistrationObjectInfoPtr registration_info) {
  DCHECK(worker_task_runner_->RunsTasksInCurrentSequence());
  // Connect to the blink::mojom::ServiceWorkerHost.
  proxy_->BindServiceWorkerHost(service_worker_host.PassHandle());
  // Set ServiceWorkerGlobalScope#registration.
  DCHECK_NE(registration_info->registration_id,
            blink::mojom::kInvalidServiceWorkerRegistrationId);
  DCHECK(registration_info->host_ptr_info.is_valid());
  DCHECK(registration_info->request.is_pending());
  proxy_->SetRegistration(
      registration_info.To<blink::WebServiceWorkerRegistrationObjectInfo>());

  proxy_->ReadyToEvaluateScript();
}

void ServiceWorkerContextClient::DispatchInstallEvent(
    DispatchInstallEventCallback callback) {
  int event_id = context_->timeout_timer->StartEvent(CreateAbortCallback(
      &context_->install_event_callbacks, false /* has_fetch_handler */));
  context_->install_event_callbacks.emplace(event_id, std::move(callback));
  TRACE_EVENT_WITH_FLOW0("ServiceWorker",
                         "ServiceWorkerContextClient::DispatchInstallEvent",
                         TRACE_ID_WITH_SCOPE(kServiceWorkerContextClientScope,
                                             TRACE_ID_LOCAL(event_id)),
                         TRACE_EVENT_FLAG_FLOW_OUT);

  proxy_->DispatchInstallEvent(event_id);
}

void ServiceWorkerContextClient::DispatchExtendableMessageEvent(
    mojom::ExtendableMessageEventPtr event,
    DispatchExtendableMessageEventCallback callback) {
  int request_id = context_->timeout_timer->StartEvent(
      CreateAbortCallback(&context_->message_event_callbacks));
  context_->message_event_callbacks.emplace(request_id, std::move(callback));
  TRACE_EVENT_WITH_FLOW0(
      "ServiceWorker",
      "ServiceWorkerContextClient::DispatchExtendableMessageEvent",
      TRACE_ID_WITH_SCOPE(kServiceWorkerContextClientScope,
                          TRACE_ID_LOCAL(request_id)),
      TRACE_EVENT_FLAG_FLOW_OUT);

  if (event->source_info_for_client) {
    blink::WebServiceWorkerClientInfo web_client =
        ToWebServiceWorkerClientInfo(std::move(event->source_info_for_client));
    proxy_->DispatchExtendableMessageEvent(request_id,
                                           std::move(event->message),
                                           event->source_origin, web_client);
    return;
  }

  DCHECK_NE(event->source_info_for_service_worker->version_id,
            blink::mojom::kInvalidServiceWorkerVersionId);
  proxy_->DispatchExtendableMessageEvent(
      request_id, std::move(event->message), event->source_origin,
      event->source_info_for_service_worker
          .To<blink::WebServiceWorkerObjectInfo>());
}

void ServiceWorkerContextClient::
    DispatchExtendableMessageEventWithCustomTimeout(
        mojom::ExtendableMessageEventPtr event,
        base::TimeDelta timeout,
        DispatchExtendableMessageEventCallback callback) {
  int request_id = context_->timeout_timer->StartEventWithCustomTimeout(
      CreateAbortCallback(&context_->message_event_callbacks), timeout);

  context_->message_event_callbacks.emplace(request_id, std::move(callback));
  TRACE_EVENT1("ServiceWorker",
               "ServiceWorkerContextClient::"
               "DispatchExtendableMessageEventWithCustomTimeout",
               "request_id", request_id);

  if (event->source_info_for_client) {
    blink::WebServiceWorkerClientInfo web_client =
        ToWebServiceWorkerClientInfo(std::move(event->source_info_for_client));
    proxy_->DispatchExtendableMessageEvent(request_id,
                                           std::move(event->message),
                                           event->source_origin, web_client);
    return;
  }

  DCHECK_NE(event->source_info_for_service_worker->version_id,
            blink::mojom::kInvalidServiceWorkerVersionId);
  proxy_->DispatchExtendableMessageEvent(
      request_id, std::move(event->message), event->source_origin,
      event->source_info_for_service_worker
          .To<blink::WebServiceWorkerObjectInfo>());
}

// S13nServiceWorker
void ServiceWorkerContextClient::DispatchFetchEvent(
    blink::mojom::DispatchFetchEventParamsPtr params,
    blink::mojom::ServiceWorkerFetchResponseCallbackPtr response_callback,
    DispatchFetchEventCallback callback) {
  int event_id = context_->timeout_timer->StartEvent(
      CreateAbortCallback(&context_->fetch_event_callbacks));
  context_->fetch_event_callbacks.emplace(event_id, std::move(callback));
  context_->fetch_response_callbacks.emplace(event_id,
                                             std::move(response_callback));

  // This TRACE_EVENT is used for perf benchmark to confirm if all of fetch
  // events have completed. (crbug.com/736697)
  TRACE_EVENT_WITH_FLOW1(
      "ServiceWorker", "ServiceWorkerContextClient::DispatchFetchEvent",
      TRACE_ID_WITH_SCOPE(kServiceWorkerContextClientScope,
                          TRACE_ID_LOCAL(event_id)),
      TRACE_EVENT_FLAG_FLOW_OUT, "url", params->request.url.spec());

  // Set up for navigation preload (FetchEvent#preloadResponse) if needed.
  const bool navigation_preload_sent = !!params->preload_handle;
  if (navigation_preload_sent) {
    SetupNavigationPreload(event_id, params->request.url,
                           std::move(params->preload_handle));
  }

  // Dispatch the event to the service worker execution context.
  blink::WebServiceWorkerRequest web_request;
  ToWebServiceWorkerRequest(
      std::move(params->request), params->request_body_blob_uuid,
      params->request_body_blob_size, std::move(params->request_body_blob),
      params->client_id, std::move(params->request_body_blob_ptrs),
      &web_request, true /* is_for_fetch_event */);
  proxy_->DispatchFetchEvent(event_id, web_request, navigation_preload_sent);
}

void ServiceWorkerContextClient::DispatchNotificationClickEvent(
    const std::string& notification_id,
    const blink::PlatformNotificationData& notification_data,
    int action_index,
    const base::Optional<base::string16>& reply,
    DispatchNotificationClickEventCallback callback) {
  int request_id = context_->timeout_timer->StartEvent(
      CreateAbortCallback(&context_->notification_click_event_callbacks));
  context_->notification_click_event_callbacks.emplace(request_id,
                                                       std::move(callback));
  TRACE_EVENT_WITH_FLOW0(
      "ServiceWorker",
      "ServiceWorkerContextClient::DispatchNotificationClickEvent",
      TRACE_ID_WITH_SCOPE(kServiceWorkerContextClientScope,
                          TRACE_ID_LOCAL(request_id)),
      TRACE_EVENT_FLAG_FLOW_OUT);

  blink::WebString web_reply;
  if (reply)
    web_reply = blink::WebString::FromUTF16(reply.value());

  proxy_->DispatchNotificationClickEvent(
      request_id, blink::WebString::FromUTF8(notification_id),
      ToWebNotificationData(notification_data), action_index, web_reply);
}

void ServiceWorkerContextClient::DispatchNotificationCloseEvent(
    const std::string& notification_id,
    const blink::PlatformNotificationData& notification_data,
    DispatchNotificationCloseEventCallback callback) {
  int request_id = context_->timeout_timer->StartEvent(
      CreateAbortCallback(&context_->notification_close_event_callbacks));
  context_->notification_close_event_callbacks.emplace(request_id,
                                                       std::move(callback));
  TRACE_EVENT_WITH_FLOW0(
      "ServiceWorker",
      "ServiceWorkerContextClient::DispatchNotificationCloseEvent",
      TRACE_ID_WITH_SCOPE(kServiceWorkerContextClientScope,
                          TRACE_ID_LOCAL(request_id)),
      TRACE_EVENT_FLAG_FLOW_OUT);
  proxy_->DispatchNotificationCloseEvent(
      request_id, blink::WebString::FromUTF8(notification_id),
      ToWebNotificationData(notification_data));
}

void ServiceWorkerContextClient::DispatchPushEvent(
    const base::Optional<std::string>& payload,
    DispatchPushEventCallback callback) {
  int request_id = context_->timeout_timer->StartEventWithCustomTimeout(
      CreateAbortCallback(&context_->push_event_callbacks),
      base::TimeDelta::FromSeconds(mojom::kPushEventTimeoutSeconds));
  context_->push_event_callbacks.emplace(request_id, std::move(callback));
  TRACE_EVENT_WITH_FLOW0("ServiceWorker",
                         "ServiceWorkerContextClient::DispatchPushEvent",
                         TRACE_ID_WITH_SCOPE(kServiceWorkerContextClientScope,
                                             TRACE_ID_LOCAL(request_id)),
                         TRACE_EVENT_FLAG_FLOW_OUT);

  // Only set data to be a valid string if the payload had decrypted data.
  blink::WebString data;
  if (payload)
    data = blink::WebString::FromUTF8(*payload);
  proxy_->DispatchPushEvent(request_id, data);
}

void ServiceWorkerContextClient::DispatchCookieChangeEvent(
    const net::CanonicalCookie& cookie,
    ::network::mojom::CookieChangeCause cause,
    DispatchCookieChangeEventCallback callback) {
  int request_id = context_->timeout_timer->StartEvent(
      CreateAbortCallback(&context_->cookie_change_event_callbacks));
  context_->cookie_change_event_callbacks.emplace(request_id,
                                                  std::move(callback));
  TRACE_EVENT_WITH_FLOW0(
      "ServiceWorker", "ServiceWorkerContextClient::DispatchCookieChangeEvent",
      TRACE_ID_WITH_SCOPE(kServiceWorkerContextClientScope,
                          TRACE_ID_LOCAL(request_id)),
      TRACE_EVENT_FLAG_FLOW_OUT);

  // After onion-souping, the conversion below will be done by mojo directly.
  DCHECK(!cookie.IsHttpOnly());
  base::Optional<blink::WebCanonicalCookie> web_cookie_opt =
      blink::WebCanonicalCookie::Create(
          blink::WebString::FromUTF8(cookie.Name()),
          blink::WebString::FromUTF8(cookie.Value()),
          blink::WebString::FromUTF8(cookie.Domain()),
          blink::WebString::FromUTF8(cookie.Path()), cookie.CreationDate(),
          cookie.ExpiryDate(), cookie.LastAccessDate(), cookie.IsSecure(),
          false /* cookie.IsHttpOnly() */,
          static_cast<network::mojom::CookieSameSite>(cookie.SameSite()),
          static_cast<network::mojom::CookiePriority>(cookie.Priority()));
  DCHECK(web_cookie_opt.has_value());

  proxy_->DispatchCookieChangeEvent(request_id, web_cookie_opt.value(), cause);
}

void ServiceWorkerContextClient::Ping(PingCallback callback) {
  std::move(callback).Run();
}

void ServiceWorkerContextClient::SetIdleTimerDelayToZero() {
  DCHECK(blink::ServiceWorkerUtils::IsServicificationEnabled());
  DCHECK(context_);
  DCHECK(context_->timeout_timer);
  context_->timeout_timer->SetIdleTimerDelayToZero();
}

void ServiceWorkerContextClient::OnNavigationPreloadResponse(
    int fetch_event_id,
    std::unique_ptr<blink::WebURLResponse> response,
    mojo::ScopedDataPipeConsumerHandle data_pipe) {
  TRACE_EVENT_WITH_FLOW0(
      "ServiceWorker",
      "ServiceWorkerContextClient::OnNavigationPreloadResponse",
      TRACE_ID_WITH_SCOPE(kServiceWorkerContextClientScope,
                          TRACE_ID_LOCAL(fetch_event_id)),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  proxy_->OnNavigationPreloadResponse(fetch_event_id, std::move(response),
                                      std::move(data_pipe));
}

void ServiceWorkerContextClient::OnNavigationPreloadError(
    int fetch_event_id,
    std::unique_ptr<blink::WebServiceWorkerError> error) {
  TRACE_EVENT_WITH_FLOW0("ServiceWorker",
                         "ServiceWorkerContextClient::OnNavigationPreloadError",
                         TRACE_ID_WITH_SCOPE(kServiceWorkerContextClientScope,
                                             TRACE_ID_LOCAL(fetch_event_id)),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  proxy_->OnNavigationPreloadError(fetch_event_id, std::move(error));
  context_->preload_requests.Remove(fetch_event_id);
}

void ServiceWorkerContextClient::OnNavigationPreloadComplete(
    int fetch_event_id,
    base::TimeTicks completion_time,
    int64_t encoded_data_length,
    int64_t encoded_body_length,
    int64_t decoded_body_length) {
  TRACE_EVENT_WITH_FLOW0(
      "ServiceWorker",
      "ServiceWorkerContextClient::OnNavigationPreloadComplete",
      TRACE_ID_WITH_SCOPE(kServiceWorkerContextClientScope,
                          TRACE_ID_LOCAL(fetch_event_id)),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  proxy_->OnNavigationPreloadComplete(fetch_event_id, completion_time,
                                      encoded_data_length, encoded_body_length,
                                      decoded_body_length);
  context_->preload_requests.Remove(fetch_event_id);
}

void ServiceWorkerContextClient::SetupNavigationPreload(
    int fetch_event_id,
    const GURL& url,
    blink::mojom::FetchEventPreloadHandlePtr preload_handle) {
  auto preload_request = std::make_unique<NavigationPreloadRequest>(
      fetch_event_id, url, std::move(preload_handle));
  context_->preload_requests.AddWithID(std::move(preload_request),
                                       fetch_event_id);
}

void ServiceWorkerContextClient::OnIdleTimeout() {
  // ServiceWorkerTimeoutTimer::did_idle_timeout() returns true if
  // ServiceWorkerContextClient::OnIdleTiemout() has been called. It means
  // termination has been requested.
  DCHECK(RequestedTermination());
  (*instance_host_)
      ->RequestTermination(base::BindOnce(
          &ServiceWorkerContextClient::OnRequestedTermination, GetWeakPtr()));
}

void ServiceWorkerContextClient::OnRequestedTermination(
    bool will_be_terminated) {
  DCHECK(blink::ServiceWorkerUtils::IsServicificationEnabled());
  DCHECK(context_);
  DCHECK(context_->timeout_timer);

  // This worker will be terminated soon. Ignore the message.
  if (will_be_terminated)
    return;

  // Dispatch a dummy event to run all of queued tasks. This updates the
  // idle timer too.
  const int event_id = context_->timeout_timer->StartEvent(base::DoNothing());
  context_->timeout_timer->EndEvent(event_id);
}

bool ServiceWorkerContextClient::RequestedTermination() const {
  return context_->timeout_timer->did_idle_timeout();
}

void ServiceWorkerContextClient::StopWorker() {
  DCHECK(main_thread_task_runner_->RunsTasksInCurrentSequence());
  if (embedded_worker_client_)
    embedded_worker_client_->StopWorker();
}

base::WeakPtr<ServiceWorkerContextClient>
ServiceWorkerContextClient::GetWeakPtr() {
  DCHECK(worker_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(context_);
  return context_->weak_factory.GetWeakPtr();
}

// static
void ServiceWorkerContextClient::ResetThreadSpecificInstanceForTesting() {
  g_worker_client_tls.Pointer()->Set(nullptr);
}

void ServiceWorkerContextClient::SetTimeoutTimerForTesting(
    std::unique_ptr<ServiceWorkerTimeoutTimer> timeout_timer) {
  DCHECK(context_);
  context_->timeout_timer = std::move(timeout_timer);
}

}  // namespace content
