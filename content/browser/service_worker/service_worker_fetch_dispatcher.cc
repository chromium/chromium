// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_fetch_dispatcher.h"

#include <string>
#include <utility>
#include <vector>

#include "base/containers/queue.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/devtools/service_worker_devtools_agent_host.h"
#include "content/browser/devtools/service_worker_devtools_manager.h"
#include "content/browser/loader/navigation_url_loader_impl.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_metrics.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/request_priority.h"
#include "net/http/http_util.h"
#include "net/log/net_log.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/cpp/single_request_url_loader_factory.h"
#include "services/network/public/cpp/url_loader_factory_builder.h"
#include "services/network/public/cpp/wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/early_hints.mojom.h"
#include "third_party/blink/public/common/service_worker/embedded_worker_status.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom.h"

namespace content {

namespace {

// TODO(crbug.com/40268507): When this is enabled, the browser will schedule
// ServiceWorkerFetchDispatcher::ResponseCallback in a high priority task queue.
BASE_FEATURE(kServiceWorkerFetchResponseCallbackUseHighPriority,
             "ServiceWorkerFetchResponseCallbackUseHighPriority",
             base::FEATURE_ENABLED_BY_DEFAULT);

bool g_force_disable_high_priority_fetch_response_callback = false;

void NotifyNavigationPreloadRequestSent(const network::ResourceRequest& request,
                                        const std::pair<int, int>& worker_id,
                                        const std::string& request_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ServiceWorkerDevToolsManager::GetInstance()->NavigationPreloadRequestSent(
      worker_id.first, worker_id.second, request_id, request);
}

void NotifyNavigationPreloadResponseReceived(
    const GURL& url,
    network::mojom::URLResponseHeadPtr response,
    const std::pair<int, int>& worker_id,
    const std::string& request_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ServiceWorkerDevToolsManager::GetInstance()
      ->NavigationPreloadResponseReceived(worker_id.first, worker_id.second,
                                          request_id, url, *response);
}

void NotifyNavigationPreloadCompleted(
    const network::URLLoaderCompletionStatus& status,
    const std::pair<int, int>& worker_id,
    const std::string& request_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ServiceWorkerDevToolsManager::GetInstance()->NavigationPreloadCompleted(
      worker_id.first, worker_id.second, request_id, status);
}

// DelegatingURLLoaderClient is the URLLoaderClient for the navigation preload
// network request. It watches as the response comes in, and pipes the response
// back to the service worker while also doing extra processing like notifying
// DevTools.
class DelegatingURLLoaderClient final : public network::mojom::URLLoaderClient {
 public:
  using WorkerId = std::pair<int, int>;
  explicit DelegatingURLLoaderClient(
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const network::ResourceRequest& request)
      : client_(std::move(client)),
        url_(request.url),
        devtools_enabled_(request.devtools_request_id.has_value()) {
    if (!devtools_enabled_)
      return;
    AddDevToolsCallback(
        base::BindOnce(&NotifyNavigationPreloadRequestSent, request));
  }

  DelegatingURLLoaderClient(const DelegatingURLLoaderClient&) = delete;
  DelegatingURLLoaderClient& operator=(const DelegatingURLLoaderClient&) =
      delete;

  ~DelegatingURLLoaderClient() override {
    if (!completed_) {
      // Let the service worker know that the request has been canceled.
      network::URLLoaderCompletionStatus status;
      status.error_code = net::ERR_ABORTED;
      client_->OnComplete(status);
      if (!devtools_enabled_)
        return;
      AddDevToolsCallback(
          base::BindOnce(&NotifyNavigationPreloadCompleted, status));
    }
  }

  void MaybeReportToDevTools(WorkerId worker_id, int fetch_event_id) {
    worker_id_ = worker_id;
    devtools_request_id_ = base::StringPrintf("preload-%d", fetch_event_id);
    MaybeRunDevToolsCallbacks();
  }

  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback ack_callback) override {
    client_->OnUploadProgress(current_position, total_size,
                              std::move(ack_callback));
  }
  void OnTransferSizeUpdated(int32_t transfer_size_diff) override {
    network::RecordOnTransferSizeUpdatedUMA(
        network::OnTransferSizeUpdatedFrom::kDelegatingURLLoaderClient);
    client_->OnTransferSizeUpdated(transfer_size_diff);
  }
  void OnReceiveEarlyHints(network::mojom::EarlyHintsPtr early_hints) override {
    client_->OnReceiveEarlyHints(std::move(early_hints));
  }
  void OnReceiveResponse(
      network::mojom::URLResponseHeadPtr head,
      mojo::ScopedDataPipeConsumerHandle body,
      std::optional<mojo_base::BigBuffer> cached_metadata) override {
    if (devtools_enabled_) {
      // Make a deep copy of URLResponseHead before posting it to a task.
      auto deep_copied_response = head.Clone();
      if (head->headers) {
        deep_copied_response->headers =
            base::MakeRefCounted<net::HttpResponseHeaders>(
                head->headers->raw_headers());
      }
      AddDevToolsCallback(
          base::BindOnce(&NotifyNavigationPreloadResponseReceived, url_,
                         std::move(deep_copied_response)));
    }
    client_->OnReceiveResponse(std::move(head), std::move(body),
                               std::move(cached_metadata));
  }
  void OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                         network::mojom::URLResponseHeadPtr head) override {
    if (devtools_enabled_) {
      // Make a deep copy of URLResponseHead before posting it to a task.
      auto deep_copied_response = head.Clone();
      if (head->headers) {
        deep_copied_response->headers =
            base::MakeRefCounted<net::HttpResponseHeaders>(
                head->headers->raw_headers());
      }
      AddDevToolsCallback(
          base::BindOnce(&NotifyNavigationPreloadResponseReceived, url_,
                         std::move(deep_copied_response)));
      network::URLLoaderCompletionStatus status;
      AddDevToolsCallback(
          base::BindOnce(&NotifyNavigationPreloadCompleted, status));
    }
    completed_ = true;
    // When the server returns a redirect response, we only send
    // OnReceiveRedirect IPC and don't send OnComplete IPC. The service worker
    // will clean up the preload request when OnReceiveRedirect() is called.
    client_->OnReceiveRedirect(redirect_info, std::move(head));
  }
  void OnComplete(const network::URLLoaderCompletionStatus& status) override {
    if (completed_)
      return;
    completed_ = true;
    client_->OnComplete(status);
    if (!devtools_enabled_)
      return;
    AddDevToolsCallback(
        base::BindOnce(&NotifyNavigationPreloadCompleted, status));
  }

  void Bind(mojo::PendingRemote<network::mojom::URLLoaderClient>* remote) {
    receiver_.Bind(remote->InitWithNewPipeAndPassReceiver());
  }

 private:
  void MaybeRunDevToolsCallbacks() {
    if (!worker_id_ || !devtools_enabled_)
      return;

    scoped_refptr<base::SequencedTaskRunner> task_runner =
        base::SequencedTaskRunner::GetCurrentDefault();
    while (!devtools_callbacks.empty()) {
      task_runner->PostTask(
          FROM_HERE, base::BindOnce(std::move(devtools_callbacks.front()),
                                    *worker_id_, devtools_request_id_));
      devtools_callbacks.pop();
    }
  }
  void AddDevToolsCallback(
      base::OnceCallback<void(const WorkerId&, const std::string&)> callback) {
    devtools_callbacks.push(std::move(callback));
    MaybeRunDevToolsCallbacks();
  }

  mojo::Receiver<network::mojom::URLLoaderClient> receiver_{this};
  mojo::Remote<network::mojom::URLLoaderClient> client_;
  bool completed_ = false;
  const GURL url_;
  const bool devtools_enabled_;

  std::optional<std::pair<int, int>> worker_id_;
  std::string devtools_request_id_;
  base::queue<base::OnceCallback<void(const WorkerId&, const std::string&)>>
      devtools_callbacks;
};

using EventType = ServiceWorkerMetrics::EventType;
EventType RequestDestinationToEventType(
    network::mojom::RequestDestination destination) {
  switch (destination) {
    case network::mojom::RequestDestination::kDocument:
      return EventType::FETCH_MAIN_FRAME;
    case network::mojom::RequestDestination::kIframe:
      return EventType::FETCH_SUB_FRAME;
    case network::mojom::RequestDestination::kFencedframe:
      return EventType::FETCH_FENCED_FRAME;
    case network::mojom::RequestDestination::kSharedWorker:
      return EventType::FETCH_SHARED_WORKER;
    case network::mojom::RequestDestination::kServiceWorker:
      return EventType::FETCH_SUB_RESOURCE;
    default:
      return EventType::FETCH_SUB_RESOURCE;
  }
}

const net::NetworkTrafficAnnotationTag kNavigationPreloadTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("service_worker_navigation_preload",
                                        R"(
    semantics {
      sender: "Service Worker Navigation Preload"
      description:
        "This request is issued by a navigation to fetch the content of the "
        "page that is being navigated to, in the case where a service worker "
        "has been registered for the page and is using the Navigation Preload "
        "API."
      trigger:
        "Navigating Chrome (by clicking on a link, bookmark, history item, "
        "using session restore, etc)."
      data:
        "Arbitrary site-controlled data can be included in the URL, HTTP "
        "headers, and request body. Requests may include cookies and "
        "site-specific credentials."
      destination: WEBSITE
    }
    policy {
      cookies_allowed: YES
      cookies_store: "user"
      setting:
        "This request can be prevented by disabling service workers, which can "
        "be done by disabling cookie and site data under Settings, Content "
        "Settings, Cookies."
      chrome_policy {
        URLBlocklist {
          URLBlocklist: { entries: '*' }
        }
      }
      chrome_policy {
        URLAllowlist {
          URLAllowlist { }
        }
      }
    }
    comments:
      "Chrome would be unable to use service workers if this feature were "
      "disabled, which could result in a degraded experience for websites that "
      "register a service worker. Using either URLBlocklist or URLAllowlist "
      "policies (or a combination of both) limits the scope of these requests."
)");

// A copy of RenderFrameHostImpl's GrantFileAccess since there's not a great
// central place to put this.
//
// Abuse is prevented, because the files listed in ResourceRequestBody are
// validated earlier by navigation or ResourceDispatcherHost machinery before
// ServiceWorkerFetchDispatcher is used to send the request to a service worker.
void GrantFileAccessToProcess(int process_id,
                              const std::vector<base::FilePath>& file_paths) {
  ChildProcessSecurityPolicyImpl* policy =
      ChildProcessSecurityPolicyImpl::GetInstance();

  for (const auto& file : file_paths) {
    if (!policy->CanReadFile(process_id, file))
      policy->GrantReadFile(process_id, file);
  }
}

// Creates the network URLLoaderFactory for the navigation preload request.
scoped_refptr<network::SharedURLLoaderFactory>
CreateNetworkFactoryForNavigationPreload(FrameTreeNode& frame_tree_node,
                                         StoragePartitionImpl& partition) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // We ignore the value of |bypass_redirect_checks_unused| since a redirect is
  // just relayed to the service worker where preloadResponse is resolved as
  // redirect.
  bool bypass_redirect_checks_unused;

  // Consult the embedder.
  mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>
      header_client;
  network::URLLoaderFactoryBuilder factory_builder;
  // Here we give nullptr for |factory_override|, because CORS is no-op
  // for navigations.
  GetContentClient()->browser()->WillCreateURLLoaderFactory(
      partition.browser_context(), frame_tree_node.current_frame_host(),
      frame_tree_node.current_frame_host()->GetProcess()->GetID(),
      ContentBrowserClient::URLLoaderFactoryType::kNavigation, url::Origin(),
      net::IsolationInfo(),
      frame_tree_node.navigation_request()->GetNavigationId(),
      ukm::SourceIdObj::FromInt64(
          frame_tree_node.navigation_request()->GetNextPageUkmSourceId()),
      factory_builder, &header_client, &bypass_redirect_checks_unused,
      /*disable_secure_dns=*/nullptr, /*factory_override=*/nullptr,
      GetUIThreadTaskRunner({BrowserTaskType::kNavigationNetworkResponse}));

  // Make the network factory.
  return base::MakeRefCounted<network::WrapperSharedURLLoaderFactory>(
      NavigationURLLoaderImpl::CreateURLLoaderFactoryWithHeaderClient(
          std::move(header_client), std::move(factory_builder), &partition));
}

}  // namespace

// ResponseCallback is owned by the callback that is passed to
// ServiceWorkerVersion::StartRequest*(), and held in pending_requests_
// until FinishRequest() is called.
class ServiceWorkerFetchDispatcher::ResponseCallback
    : public blink::mojom::ServiceWorkerFetchResponseCallback {
 public:
  ResponseCallback(
      mojo::PendingReceiver<blink::mojom::ServiceWorkerFetchResponseCallback>
          receiver,
      base::WeakPtr<ServiceWorkerFetchDispatcher> fetch_dispatcher,
      ServiceWorkerVersion* version)
      : receiver_(
            this,
            std::move(receiver),
            (!g_force_disable_high_priority_fetch_response_callback &&
                     base::FeatureList::IsEnabled(
                         kServiceWorkerFetchResponseCallbackUseHighPriority)
                 ? GetUIThreadTaskRunner(
                       {BrowserTaskType::kServiceWorkerStorageControlResponse})
                 : base::SequencedTaskRunner::GetCurrentDefault())),
        fetch_dispatcher_(fetch_dispatcher),
        version_(version) {
    receiver_.set_disconnect_handler(base::BindOnce(
        &ResponseCallback::OnDisconnected, base::Unretained(this)));
  }

  ResponseCallback(const ResponseCallback&) = delete;
  ResponseCallback& operator=(const ResponseCallback&) = delete;

  ~ResponseCallback() override { DCHECK(fetch_event_id_.has_value()); }

  void set_fetch_event_id(int id) {
    DCHECK(!fetch_event_id_);
    fetch_event_id_ = id;
  }

  void OnDisconnected() {
    version_->FinishRequest(fetch_event_id_.value(), /*was_handled=*/false);
    // HandleResponse() is not needed to be called here because
    // OnFetchEventFinished() should be called with an error code when
    // disconnecting without response, and it lets the request failed.

    // Do not add code here because FinishRequest() removes `this`.
  }

  // Implements blink::mojom::ServiceWorkerFetchResponseCallback.
  void OnResponse(
      blink::mojom::FetchAPIResponsePtr response,
      blink::mojom::ServiceWorkerFetchEventTimingPtr timing) override {
    HandleResponse(fetch_dispatcher_, version_, fetch_event_id_,
                   std::move(response), nullptr /* body_as_stream */,
                   FetchEventResult::kGotResponse, std::move(timing));
  }
  void OnResponseStream(
      blink::mojom::FetchAPIResponsePtr response,
      blink::mojom::ServiceWorkerStreamHandlePtr body_as_stream,
      blink::mojom::ServiceWorkerFetchEventTimingPtr timing) override {
    HandleResponse(fetch_dispatcher_, version_, fetch_event_id_,
                   std::move(response), std::move(body_as_stream),
                   FetchEventResult::kGotResponse, std::move(timing));
  }
  void OnFallback(
      std::optional<network::DataElementChunkedDataPipe> request_body,
      blink::mojom::ServiceWorkerFetchEventTimingPtr timing) override {
    HandleResponse(fetch_dispatcher_, version_, fetch_event_id_,
                   blink::mojom::FetchAPIResponse::New(),
                   nullptr /* body_as_stream */,
                   FetchEventResult::kShouldFallback, std::move(timing));
  }

 private:
  // static as version->FinishRequest will remove the calling ResponseCallback
  // instance.
  static void HandleResponse(
      base::WeakPtr<ServiceWorkerFetchDispatcher> fetch_dispatcher,
      ServiceWorkerVersion* version,
      std::optional<int> fetch_event_id,
      blink::mojom::FetchAPIResponsePtr response,
      blink::mojom::ServiceWorkerStreamHandlePtr body_as_stream,
      FetchEventResult fetch_result,
      blink::mojom::ServiceWorkerFetchEventTimingPtr timing) {
    if (!version->FinishRequest(fetch_event_id.value(),
                                fetch_result == FetchEventResult::kGotResponse))
      NOTREACHED_IN_MIGRATION() << "Should only receive one reply per event";
    // |fetch_dispatcher| is null if the URLRequest was killed.
    if (!fetch_dispatcher)
      return;
    fetch_dispatcher->DidFinish(fetch_event_id.value(), fetch_result,
                                std::move(response), std::move(body_as_stream),
                                std::move(timing));
  }

  mojo::Receiver<blink::mojom::ServiceWorkerFetchResponseCallback> receiver_;
  base::WeakPtr<ServiceWorkerFetchDispatcher> fetch_dispatcher_;
  // Owns |this| via pending_requests_.
  raw_ptr<ServiceWorkerVersion> version_;
  // Must be set to a non-nullopt value before the corresponding mojo
  // handle is passed to the other end (i.e. before any of OnResponse*
  // is called).
  std::optional<int> fetch_event_id_;
};

// This class keeps the URL loader related assets alive while the FetchEvent is
// ongoing in the service worker.
class ServiceWorkerFetchDispatcher::URLLoaderAssets
    : public base::RefCounted<ServiceWorkerFetchDispatcher::URLLoaderAssets> {
 public:
  // Non-NetworkService.
  URLLoaderAssets(
      std::unique_ptr<network::mojom::URLLoaderFactory> url_loader_factory,
      std::unique_ptr<DelegatingURLLoaderClient> url_loader_client)
      : url_loader_factory_(std::move(url_loader_factory)),
        url_loader_client_(std::move(url_loader_client)) {}
  // NetworkService.
  URLLoaderAssets(
      scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory,
      mojo::PendingRemote<network::mojom::URLLoader> url_loader,
      std::unique_ptr<DelegatingURLLoaderClient> url_loader_client)
      : shared_url_loader_factory_(std::move(shared_url_loader_factory)),
        url_loader_(std::move(url_loader)),
        url_loader_client_(std::move(url_loader_client)) {}

  URLLoaderAssets(const URLLoaderAssets&) = delete;
  URLLoaderAssets& operator=(const URLLoaderAssets&) = delete;

  void MaybeReportToDevTools(std::pair<int, int> worker_id,
                             int fetch_event_id) {
    url_loader_client_->MaybeReportToDevTools(worker_id, fetch_event_id);
  }

 private:
  friend class base::RefCounted<URLLoaderAssets>;
  virtual ~URLLoaderAssets() {}

  // Non-NetworkService:
  std::unique_ptr<network::mojom::URLLoaderFactory> url_loader_factory_;

  // NetworkService:
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
  mojo::PendingRemote<network::mojom::URLLoader> url_loader_;

  // Both:
  std::unique_ptr<DelegatingURLLoaderClient> url_loader_client_;
};

ServiceWorkerFetchDispatcher::ServiceWorkerFetchDispatcher(
    blink::mojom::FetchAPIRequestPtr request,
    network::mojom::RequestDestination destination,
    const std::string& client_id,
    const std::string& resulting_client_id,
    scoped_refptr<ServiceWorkerVersion> version,
    base::OnceClosure prepare_callback,
    FetchCallback fetch_callback)
    : request_(std::move(request)),
      client_id_(client_id),
      resulting_client_id_(resulting_client_id),
      version_(std::move(version)),
      destination_(destination),
      prepare_callback_(std::move(prepare_callback)),
      fetch_callback_(std::move(fetch_callback)) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!request_->blob);
  TRACE_EVENT_WITH_FLOW1(
      "ServiceWorker",
      "ServiceWorkerFetchDispatcher::ServiceWorkerFetchDispatcher",
      TRACE_ID_LOCAL(this), TRACE_EVENT_FLAG_FLOW_OUT, "event_type",
      ServiceWorkerMetrics::EventTypeToString(GetEventType()));
}

ServiceWorkerFetchDispatcher::~ServiceWorkerFetchDispatcher() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT_WITH_FLOW0(
      "ServiceWorker",
      "ServiceWorkerFetchDispatcher::~ServiceWorkerFetchDispatcher",
      TRACE_ID_LOCAL(this), TRACE_EVENT_FLAG_FLOW_IN);
}

void ServiceWorkerFetchDispatcher::Run() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(version_->status() == ServiceWorkerVersion::ACTIVATING ||
         version_->status() == ServiceWorkerVersion::ACTIVATED)
      << version_->status();
  TRACE_EVENT_WITH_FLOW0("ServiceWorker", "ServiceWorkerFetchDispatcher::Run",
                         TRACE_ID_LOCAL(this),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);

  if (version_->status() == ServiceWorkerVersion::ACTIVATING) {
    version_->RegisterStatusChangeCallback(
        base::BindOnce(&ServiceWorkerFetchDispatcher::DidWaitForActivation,
                       weak_factory_.GetWeakPtr()));
    return;
  }
  StartWorker();
}

void ServiceWorkerFetchDispatcher::DidWaitForActivation() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT_WITH_FLOW0("ServiceWorker",
                         "ServiceWorkerFetchDispatcher::DidWaitForActivation",
                         TRACE_ID_LOCAL(this),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  StartWorker();
}

void ServiceWorkerFetchDispatcher::StartWorker() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT_WITH_FLOW0("ServiceWorker",
                         "ServiceWorkerFetchDispatcher::StartWorker",
                         TRACE_ID_LOCAL(this),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);

  // We might be REDUNDANT if a new worker started activating and kicked us out
  // before we could finish activation.
  if (version_->status() != ServiceWorkerVersion::ACTIVATED) {
    DCHECK_EQ(ServiceWorkerVersion::REDUNDANT, version_->status());
    DidFail(blink::ServiceWorkerStatusCode::kErrorActivateWorkerFailed);
    return;
  }

  if (version_->running_status() == blink::EmbeddedWorkerStatus::kRunning) {
    DispatchFetchEvent();
    return;
  }

  version_->RunAfterStartWorker(
      GetEventType(),
      base::BindOnce(&ServiceWorkerFetchDispatcher::DidStartWorker,
                     weak_factory_.GetWeakPtr()));

  if (version_->is_endpoint_ready()) {
    // For an active service worker, the endpoint becomes ready synchronously
    // with StartWorker(). In that case, we can dispatch FetchEvent immediately.
    DispatchFetchEvent();
  }
}

void ServiceWorkerFetchDispatcher::DidStartWorker(
    blink::ServiceWorkerStatusCode status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT_WITH_FLOW0("ServiceWorker",
                         "ServiceWorkerFetchDispatcher::DidStartWorker",
                         TRACE_ID_LOCAL(this),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);

  if (status != blink::ServiceWorkerStatusCode::kOk) {
    DidFail(status);
    return;
  }

  if (!IsEventDispatched()) {
    DispatchFetchEvent();
  }
}

void ServiceWorkerFetchDispatcher::DispatchFetchEvent() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(blink::EmbeddedWorkerStatus::kStarting == version_->running_status() ||
         blink::EmbeddedWorkerStatus::kRunning == version_->running_status())
      << "Worker stopped too soon after it was started.";
  TRACE_EVENT_WITH_FLOW0("ServiceWorker",
                         "ServiceWorkerFetchDispatcher::DispatchFetchEvent",
                         TRACE_ID_LOCAL(this),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  // Grant the service worker's process access to files in the request body.
  if (request_->body) {
    GrantFileAccessToProcess(version_->embedded_worker()->process_id(),
                             request_->body->GetReferencedFiles());
  }

  // Run callback to say that the fetch event will be dispatched.
  DCHECK(prepare_callback_);
  std::move(prepare_callback_).Run();

  // Set up for receiving the response.
  mojo::PendingRemote<blink::mojom::ServiceWorkerFetchResponseCallback>
      pending_response_callback;
  auto response_callback = std::make_unique<ResponseCallback>(
      pending_response_callback.InitWithNewPipeAndPassReceiver(),
      weak_factory_.GetWeakPtr(), version_.get());
  ResponseCallback* response_callback_rawptr = response_callback.get();

  // Set up the fetch event.
  int fetch_event_id = version_->StartRequest(
      GetEventType(),
      base::BindOnce(&ServiceWorkerFetchDispatcher::DidFailToDispatch,
                     weak_factory_.GetWeakPtr(), std::move(response_callback)));
  int event_finish_id = version_->StartRequest(
      ServiceWorkerMetrics::EventType::FETCH_WAITUNTIL, base::DoNothing());
  response_callback_rawptr->set_fetch_event_id(fetch_event_id);

  // Report navigation preload to DevTools if needed.
  if (url_loader_assets_) {
    url_loader_assets_->MaybeReportToDevTools(
        std::make_pair(
            version_->embedded_worker()->process_id(),
            version_->embedded_worker()->worker_devtools_agent_route_id()),
        fetch_event_id);
  }

  // Dispatch the fetch event.
  auto params = blink::mojom::DispatchFetchEventParams::New();
  params->request = std::move(request_);
  params->client_id = client_id_;
  params->resulting_client_id = resulting_client_id_;
  params->preload_url_loader_client_receiver =
      std::move(preload_url_loader_client_receiver_);
  if (race_network_request_token_) {
    params->request->service_worker_race_network_request_token =
        race_network_request_token_;
  }
  if (race_network_request_loader_factory_) {
    params->race_network_request_loader_factory =
        std::move(race_network_request_loader_factory_);
  }

  // |endpoint()| is owned by |version_|. So it is safe to pass the
  // unretained raw pointer of |version_| to OnFetchEventFinished callback.
  // Pass |url_loader_assets_| to the callback to keep the URL loader related
  // assets alive while the FetchEvent is ongoing in the service worker.
  version_->endpoint()->DispatchFetchEventForMainResource(
      std::move(params), std::move(pending_response_callback),
      base::BindOnce(&ServiceWorkerFetchDispatcher::OnFetchEventFinished,
                     weak_factory_.GetWeakPtr(),
                     base::Unretained(version_.get()), event_finish_id,
                     url_loader_assets_));
}

void ServiceWorkerFetchDispatcher::DidFailToDispatch(
    std::unique_ptr<ResponseCallback> response_callback,
    blink::ServiceWorkerStatusCode status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DidFail(status);
}

void ServiceWorkerFetchDispatcher::DidFail(
    blink::ServiceWorkerStatusCode status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(blink::ServiceWorkerStatusCode::kOk, status);
  TRACE_EVENT_WITH_FLOW1(
      "ServiceWorker", "ServiceWorkerFetchDispatcher::DidFail",
      TRACE_ID_LOCAL(this),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "status", status);
  RunCallback(status, FetchEventResult::kShouldFallback,
              blink::mojom::FetchAPIResponse::New(),
              nullptr /* body_as_stream */, nullptr /* timing */);
}

void ServiceWorkerFetchDispatcher::DidFinish(
    int request_id,
    FetchEventResult fetch_result,
    blink::mojom::FetchAPIResponsePtr response,
    blink::mojom::ServiceWorkerStreamHandlePtr body_as_stream,
    blink::mojom::ServiceWorkerFetchEventTimingPtr timing) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT_WITH_FLOW0("ServiceWorker",
                         "ServiceWorkerFetchDispatcher::DidFinish",
                         TRACE_ID_LOCAL(this),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  RunCallback(blink::ServiceWorkerStatusCode::kOk, fetch_result,
              std::move(response), std::move(body_as_stream),
              std::move(timing));
}

void ServiceWorkerFetchDispatcher::RunCallback(
    blink::ServiceWorkerStatusCode status,
    FetchEventResult fetch_result,
    blink::mojom::FetchAPIResponsePtr response,
    blink::mojom::ServiceWorkerStreamHandlePtr body_as_stream,
    blink::mojom::ServiceWorkerFetchEventTimingPtr timing) {
  // Fetch dispatcher can be completed at this point due to a failure of
  // starting up a worker. In that case, let's simply ignore it.
  if (!fetch_callback_)
    return;

  std::move(fetch_callback_)
      .Run(status, fetch_result, std::move(response), std::move(body_as_stream),
           std::move(timing), version_);
}

// static
const char* ServiceWorkerFetchDispatcher::FetchEventResultToSuffix(
    FetchEventResult result) {
  // Don't change these returned strings. They are written (in hashed form) into
  // logs.
  switch (result) {
    case ServiceWorkerFetchDispatcher::FetchEventResult::kShouldFallback:
      return "_SHOULD_FALLBACK";
    case ServiceWorkerFetchDispatcher::FetchEventResult::kGotResponse:
      return "_GOT_RESPONSE";
  }
  NOTREACHED_IN_MIGRATION()
      << "Got unexpected fetch event result:" << static_cast<int>(result);
  return "error";
}

bool ServiceWorkerFetchDispatcher::MaybeStartNavigationPreload(
    const network::ResourceRequest& original_request,
    scoped_refptr<ServiceWorkerContextWrapper> context_wrapper,
    FrameTreeNodeId frame_tree_node_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (destination_ != network::mojom::RequestDestination::kDocument &&
      destination_ != network::mojom::RequestDestination::kIframe &&
      destination_ != network::mojom::RequestDestination::kFencedframe) {
    return false;
  }
  if (!version_->navigation_preload_state().enabled)
    return false;
  // TODO(horo): Currently NavigationPreload doesn't support request body.
  if (request_->body)
    return false;

  network::ResourceRequest resource_request(original_request);
  if (destination_ == network::mojom::RequestDestination::kDocument) {
    resource_request.resource_type = static_cast<int>(
        blink::mojom::ResourceType::kNavigationPreloadMainFrame);
  } else {
    DCHECK(destination_ == network::mojom::RequestDestination::kIframe ||
           destination_ == network::mojom::RequestDestination::kFencedframe);
    resource_request.resource_type = static_cast<int>(
        blink::mojom::ResourceType::kNavigationPreloadSubFrame);
  }

  resource_request.skip_service_worker = true;
  resource_request.do_not_prompt_for_login = true;

  DCHECK(net::HttpUtil::IsValidHeaderValue(
      version_->navigation_preload_state().header));
  resource_request.headers.SetHeader(
      "Service-Worker-Navigation-Preload",
      version_->navigation_preload_state().header);

  // Create the network factory.
  scoped_refptr<network::SharedURLLoaderFactory> factory =
      CreateNetworkURLLoaderFactory(context_wrapper, frame_tree_node_id);

  // Create the DelegatingURLLoaderClient, which becomes the
  // URLLoaderClient for the navigation preload network request.
  mojo::PendingRemote<network::mojom::URLLoaderClient> inner_url_loader_client;
  preload_url_loader_client_receiver_ =
      inner_url_loader_client.InitWithNewPipeAndPassReceiver();
  auto url_loader_client = std::make_unique<DelegatingURLLoaderClient>(
      std::move(inner_url_loader_client), resource_request);

  // Start the network request for the URL using the network factory.
  // TODO(falken): What to do about routing_id.
  mojo::PendingRemote<network::mojom::URLLoaderClient>
      url_loader_client_to_pass;
  url_loader_client->Bind(&url_loader_client_to_pass);
  mojo::PendingRemote<network::mojom::URLLoader> url_loader;

  // Allow the embedder to intercept the URLLoader request if necessary. This
  // must be a synchronous decision by the embedder. In the future, we may wish
  // to support asynchronous decisions using |URLLoaderRequestInterceptor| in
  // the same fashion that they are used for navigation requests.
  ContentBrowserClient::URLLoaderRequestHandler embedder_url_loader_handler =
      GetContentClient()
          ->browser()
          ->CreateURLLoaderHandlerForServiceWorkerNavigationPreload(
              frame_tree_node_id, resource_request);

  if (!embedder_url_loader_handler.is_null()) {
    factory = base::MakeRefCounted<network::SingleRequestURLLoaderFactory>(
        std::move(embedder_url_loader_handler));
  }

  factory->CreateLoaderAndStart(
      url_loader.InitWithNewPipeAndPassReceiver(),
      GlobalRequestID::MakeBrowserInitiated().request_id,
      network::mojom::kURLLoadOptionNone, resource_request,
      std::move(url_loader_client_to_pass),
      net::MutableNetworkTrafficAnnotationTag(
          kNavigationPreloadTrafficAnnotation));

  DCHECK(!url_loader_assets_);
  url_loader_assets_ = base::MakeRefCounted<URLLoaderAssets>(
      std::move(factory), std::move(url_loader), std::move(url_loader_client));
  return true;
}

ServiceWorkerMetrics::EventType ServiceWorkerFetchDispatcher::GetEventType()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return RequestDestinationToEventType(destination_);
}

bool ServiceWorkerFetchDispatcher::IsEventDispatched() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return request_.is_null();
}

// static
scoped_refptr<network::SharedURLLoaderFactory>
ServiceWorkerFetchDispatcher::CreateNetworkURLLoaderFactory(
    scoped_refptr<ServiceWorkerContextWrapper> context_wrapper,
    FrameTreeNodeId frame_tree_node_id) {
  // TODO(crbug.com/40260328): Require the caller to pass in a FrameTreeNode
  // directly, or figure out why it's OK for it to be null.
  // TODO(falken): Can `navigation_request` check be a DCHECK now that the
  // caller does not post a task to this function?
  auto* frame_tree_node = FrameTreeNode::GloballyFindByID(frame_tree_node_id);
  auto* storage_partition = context_wrapper->storage_partition();
  if (frame_tree_node && storage_partition &&
      frame_tree_node->navigation_request()) {
    return CreateNetworkFactoryForNavigationPreload(*frame_tree_node,
                                                    *storage_partition);
  }

  // The navigation was cancelled. Just drop the request. Otherwise, we might
  // go to network without consulting the embedder first, which would break
  // guarantees.
  mojo::PendingRemote<network::mojom::URLLoaderFactory> network_factory;
  return base::MakeRefCounted<network::WrapperSharedURLLoaderFactory>(
      std::move(network_factory));
}

void ServiceWorkerFetchDispatcher::OnFetchEventFinished(
    base::WeakPtr<ServiceWorkerFetchDispatcher> fetch_dispatcher,
    ServiceWorkerVersion* version,
    int event_finish_id,
    scoped_refptr<URLLoaderAssets> url_loader_assets,
    blink::mojom::ServiceWorkerEventStatus status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (fetch_dispatcher &&
      status == blink::mojom::ServiceWorkerEventStatus::TIMEOUT) {
    fetch_dispatcher->DidFail(blink::ServiceWorkerStatusCode::kErrorTimeout);
  }
  version->FinishRequest(
      event_finish_id,
      status != blink::mojom::ServiceWorkerEventStatus::ABORTED);
}

// static
void ServiceWorkerFetchDispatcher::
    ForceDisableHighPriorityFetchResponseCallbackForTesting(
        bool force_disable) {
  g_force_disable_high_priority_fetch_response_callback = force_disable;
}

}  // namespace content
