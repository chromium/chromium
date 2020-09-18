// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/service_worker/service_worker_context_client.h"

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/check_op.h"
#include "base/debug/alias.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/common/content_features.h"
#include "content/public/common/network_service_util.h"
#include "content/public/common/referrer.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/public/renderer/document_state.h"
#include "content/public/renderer/worker_thread.h"
#include "content/renderer/loader/child_url_loader_factory_bundle.h"
#include "content/renderer/loader/web_url_loader_impl.h"
#include "content/renderer/loader/web_url_request_util.h"
#include "content/renderer/renderer_blink_platform_impl.h"
#include "content/renderer/service_worker/embedded_worker_instance_client_impl.h"
#include "content/renderer/service_worker/navigation_preload_request.h"
#include "content/renderer/service_worker/service_worker_fetch_context_impl.h"
#include "content/renderer/service_worker/service_worker_type_converters.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/cpp/wrapper_shared_url_loader_factory.h"
#include "third_party/blink/public/common/messaging/message_port_channel.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"
#include "third_party/blink/public/mojom/blob/blob.mojom.h"
#include "third_party/blink/public/mojom/browser_interface_broker.mojom.h"
#include "third_party/blink/public/mojom/loader/request_context_frame_type.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_client.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_object.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_error.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/url_conversion.h"
#include "third_party/blink/public/platform/web_http_body.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/public/web/modules/service_worker/web_service_worker_context_client.h"
#include "third_party/blink/public/web/modules/service_worker/web_service_worker_context_proxy.h"

using blink::WebURLRequest;
using blink::MessagePortChannel;

namespace content {

namespace {

constexpr char kServiceWorkerContextClientScope[] =
    "ServiceWorkerContextClient";

}  // namespace

// Holds data that needs to be bound to the worker context on the
// worker thread.
struct ServiceWorkerContextClient::WorkerContextData {
  explicit WorkerContextData(ServiceWorkerContextClient* owner)
      : weak_factory(owner), proxy_weak_factory(owner->proxy_) {}

  ~WorkerContextData() { DCHECK(thread_checker.CalledOnValidThread()); }

  // Inflight navigation preload requests.
  base::IDMap<std::unique_ptr<NavigationPreloadRequest>> preload_requests;

  base::ThreadChecker thread_checker;
  base::WeakPtrFactory<ServiceWorkerContextClient> weak_factory;
  base::WeakPtrFactory<blink::WebServiceWorkerContextProxy> proxy_weak_factory;
};

ServiceWorkerContextClient::ServiceWorkerContextClient(
    int64_t service_worker_version_id,
    const GURL& service_worker_scope,
    const GURL& script_url,
    bool is_starting_installed_worker,
    blink::mojom::RendererPreferencesPtr renderer_preferences,
    mojo::PendingReceiver<blink::mojom::ServiceWorker> service_worker_receiver,
    mojo::PendingReceiver<blink::mojom::ControllerServiceWorker>
        controller_receiver,
    mojo::PendingAssociatedRemote<blink::mojom::EmbeddedWorkerInstanceHost>
        instance_host,
    blink::mojom::ServiceWorkerProviderInfoForStartWorkerPtr provider_info,
    EmbeddedWorkerInstanceClientImpl* owner,
    blink::mojom::EmbeddedWorkerStartTimingPtr start_timing,
    mojo::PendingReceiver<blink::mojom::RendererPreferenceWatcher>
        preference_watcher_receiver,
    std::unique_ptr<blink::PendingURLLoaderFactoryBundle> subresource_loaders,
    mojo::PendingReceiver<blink::mojom::SubresourceLoaderUpdater>
        subresource_loader_updater,
    const GURL& script_url_to_skip_throttling,
    scoped_refptr<base::SingleThreadTaskRunner> initiator_thread_task_runner,
    int32_t service_worker_route_id,
    const std::vector<std::string>& cors_exempt_header_list)
    : service_worker_version_id_(service_worker_version_id),
      service_worker_scope_(service_worker_scope),
      script_url_(script_url),
      is_starting_installed_worker_(is_starting_installed_worker),
      script_url_to_skip_throttling_(script_url_to_skip_throttling),
      renderer_preferences_(std::move(renderer_preferences)),
      preference_watcher_receiver_(std::move(preference_watcher_receiver)),
      initiator_thread_task_runner_(std::move(initiator_thread_task_runner)),
      proxy_(nullptr),
      pending_service_worker_receiver_(std::move(service_worker_receiver)),
      controller_receiver_(std::move(controller_receiver)),
      pending_subresource_loader_updater_(
          std::move(subresource_loader_updater)),
      owner_(owner),
      start_timing_(std::move(start_timing)),
      service_worker_route_id_(service_worker_route_id),
      cors_exempt_header_list_(cors_exempt_header_list) {
  DCHECK(initiator_thread_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(owner_);
  DCHECK(subresource_loaders);
  instance_host_ =
      mojo::SharedAssociatedRemote<blink::mojom::EmbeddedWorkerInstanceHost>(
          std::move(instance_host), initiator_thread_task_runner_);

  if (IsOutOfProcessNetworkService()) {
    // If the network service crashes, this worker self-terminates, so it can
    // be restarted later with a connection to the restarted network
    // service.
    // Note that the default factory is the network service factory. It's set
    // on the start worker sequence.
    network_service_disconnect_handler_holder_.Bind(
        std::move(subresource_loaders->pending_default_factory()));
    network_service_disconnect_handler_holder_->Clone(
        subresource_loaders->pending_default_factory()
            .InitWithNewPipeAndPassReceiver());
    network_service_disconnect_handler_holder_.set_disconnect_handler(
        base::BindOnce(&ServiceWorkerContextClient::StopWorkerOnInitiatorThread,
                       base::Unretained(this)));
  }

  loader_factories_ = base::MakeRefCounted<ChildURLLoaderFactoryBundle>(
      std::make_unique<ChildPendingURLLoaderFactoryBundle>(
          std::move(subresource_loaders)));

  service_worker_provider_info_ = std::move(provider_info);

  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1("ServiceWorker",
                                    "ServiceWorkerContextClient", this,
                                    "script_url", script_url_.spec());
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1(
      "ServiceWorker", "LOAD_SCRIPT", this, "Source",
      (is_starting_installed_worker_ ? "InstalledScriptsManager"
                                     : "ResourceLoader"));
}

ServiceWorkerContextClient::~ServiceWorkerContextClient() {
  DCHECK(initiator_thread_task_runner_->RunsTasksInCurrentSequence());
}

void ServiceWorkerContextClient::StartWorkerContextOnInitiatorThread(
    std::unique_ptr<blink::WebEmbeddedWorker> worker,
    std::unique_ptr<blink::WebEmbeddedWorkerStartData> start_data,
    std::unique_ptr<blink::WebServiceWorkerInstalledScriptsManagerParams>
        installed_scripts_manager_params,
    mojo::PendingRemote<blink::mojom::WorkerContentSettingsProxy>
        content_settings,
    mojo::PendingRemote<blink::mojom::CacheStorage> cache_storage,
    mojo::PendingRemote<blink::mojom::BrowserInterfaceBroker>
        browser_interface_broker) {
  DCHECK(initiator_thread_task_runner_->RunsTasksInCurrentSequence());
  worker_ = std::move(worker);
  worker_->StartWorkerContext(
      std::move(start_data), std::move(installed_scripts_manager_params),
      std::move(content_settings), std::move(cache_storage),
      std::move(browser_interface_broker), initiator_thread_task_runner_);
}

blink::WebEmbeddedWorker& ServiceWorkerContextClient::worker() {
  DCHECK(initiator_thread_task_runner_->RunsTasksInCurrentSequence());
  return *worker_;
}

void ServiceWorkerContextClient::WorkerReadyForInspectionOnInitiatorThread(
    blink::CrossVariantMojoRemote<blink::mojom::DevToolsAgentInterfaceBase>
        devtools_agent_remote,
    blink::CrossVariantMojoReceiver<
        blink::mojom::DevToolsAgentHostInterfaceBase>
        devtools_agent_host_receiver) {
  DCHECK(initiator_thread_task_runner_->RunsTasksInCurrentSequence());
  instance_host_->OnReadyForInspection(std::move(devtools_agent_remote),
                                       std::move(devtools_agent_host_receiver));
}

void ServiceWorkerContextClient::FailedToFetchClassicScript() {
  DCHECK(worker_task_runner_->RunsTasksInCurrentSequence());
  TRACE_EVENT_NESTABLE_ASYNC_END1("ServiceWorker", "LOAD_SCRIPT", this,
                                  "Status", "FailedToFetchClassicScript");
  // The caller is responsible for terminating the thread which
  // eventually destroys |this|.
}

void ServiceWorkerContextClient::FailedToFetchModuleScript() {
  DCHECK(worker_task_runner_->RunsTasksInCurrentSequence());
  TRACE_EVENT_NESTABLE_ASYNC_END1("ServiceWorker", "LOAD_SCRIPT", this,
                                  "Status", "FailedToFetchModuleScript");
  // The caller is responsible for terminating the thread which
  // eventually destroys |this|.
}

void ServiceWorkerContextClient::WorkerScriptLoadedOnWorkerThread() {
  DCHECK(worker_task_runner_->RunsTasksInCurrentSequence());
  instance_host_->OnScriptLoaded();
  TRACE_EVENT_NESTABLE_ASYNC_END0("ServiceWorker", "LOAD_SCRIPT", this);
}

void ServiceWorkerContextClient::WorkerContextStarted(
    blink::WebServiceWorkerContextProxy* proxy,
    scoped_refptr<base::SequencedTaskRunner> worker_task_runner) {
  DCHECK(!initiator_thread_task_runner_->RunsTasksInCurrentSequence())
      << "service worker started on the initiator thread instead of a worker "
         "thread";
  DCHECK(worker_task_runner->RunsTasksInCurrentSequence());
  DCHECK(!worker_task_runner_);
  worker_task_runner_ = std::move(worker_task_runner);
  DCHECK(!proxy_);
  proxy_ = proxy;

  context_ = std::make_unique<WorkerContextData>(this);

  DCHECK(pending_service_worker_receiver_.is_valid());
  proxy_->BindServiceWorker(std::move(pending_service_worker_receiver_));

  DCHECK(controller_receiver_.is_valid());
  proxy_->BindControllerServiceWorker(std::move(controller_receiver_));

  GetContentClient()
      ->renderer()
      ->DidInitializeServiceWorkerContextOnWorkerThread(
          proxy_, service_worker_scope_, script_url_);
}

void ServiceWorkerContextClient::WillEvaluateScript(
    v8::Local<v8::Context> v8_context) {
  DCHECK(worker_task_runner_->RunsTasksInCurrentSequence());
  start_timing_->script_evaluation_start_time = base::TimeTicks::Now();

  // Temporary CHECK for https://crbug.com/881100
  int64_t t0 =
      start_timing_->start_worker_received_time.since_origin().InMicroseconds();
  int64_t t1 = start_timing_->script_evaluation_start_time.since_origin()
                   .InMicroseconds();
  base::debug::Alias(&t0);
  base::debug::Alias(&t1);
  CHECK_LE(start_timing_->start_worker_received_time,
           start_timing_->script_evaluation_start_time);

  instance_host_->OnScriptEvaluationStart();

  DCHECK(proxy_);
  GetContentClient()->renderer()->WillEvaluateServiceWorkerOnWorkerThread(
      proxy_, v8_context, service_worker_version_id_, service_worker_scope_,
      script_url_);
}

void ServiceWorkerContextClient::DidEvaluateScript(bool success) {
  DCHECK(worker_task_runner_->RunsTasksInCurrentSequence());
  start_timing_->script_evaluation_end_time = base::TimeTicks::Now();

  // Temporary CHECK for https://crbug.com/881100
  int64_t t0 = start_timing_->script_evaluation_start_time.since_origin()
                   .InMicroseconds();
  int64_t t1 =
      start_timing_->script_evaluation_end_time.since_origin().InMicroseconds();
  base::debug::Alias(&t0);
  base::debug::Alias(&t1);
  CHECK_LE(start_timing_->script_evaluation_start_time,
           start_timing_->script_evaluation_end_time);

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
}

void ServiceWorkerContextClient::WillInitializeWorkerContext() {
  GetContentClient()
      ->renderer()
      ->WillInitializeServiceWorkerContextOnWorkerThread();
}

void ServiceWorkerContextClient::WillDestroyWorkerContext(
    v8::Local<v8::Context> context) {
  DCHECK(worker_task_runner_->RunsTasksInCurrentSequence());

  // At this point WillStopCurrentWorkerThread is already called, so
  // worker_task_runner_->RunsTasksInCurrentSequence() returns false
  // (while we're still on the worker thread).
  proxy_ = nullptr;

  // We have to clear callbacks now, as they need to be freed on the
  // same thread.
  context_.reset();

  GetContentClient()->renderer()->WillDestroyServiceWorkerContextOnWorkerThread(
      context, service_worker_version_id_, service_worker_scope_, script_url_);
}

void ServiceWorkerContextClient::WorkerContextDestroyed() {
  DCHECK(worker_task_runner_->RunsTasksInCurrentSequence());

  instance_host_->OnStopped();

  // base::Unretained is safe because |owner_| does not destroy itself until
  // WorkerContextDestroyed is called.
  initiator_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&EmbeddedWorkerInstanceClientImpl::WorkerContextDestroyed,
                     base::Unretained(owner_)));
}

void ServiceWorkerContextClient::CountFeature(
    blink::mojom::WebFeature feature) {
  instance_host_->CountFeature(feature);
}

void ServiceWorkerContextClient::ReportException(
    const blink::WebString& error_message,
    int line_number,
    int column_number,
    const blink::WebString& source_url) {
  instance_host_->OnReportException(error_message.Utf16(), line_number,
                                    column_number,
                                    blink::WebStringToGURL(source_url));
}

void ServiceWorkerContextClient::ReportConsoleMessage(
    blink::mojom::ConsoleMessageSource source,
    blink::mojom::ConsoleMessageLevel level,
    const blink::WebString& message,
    int line_number,
    const blink::WebString& source_url) {
  instance_host_->OnReportConsoleMessage(source, level, message.Utf16(),
                                         line_number,
                                         blink::WebStringToGURL(source_url));
}

scoped_refptr<blink::WebServiceWorkerFetchContext>
ServiceWorkerContextClient::CreateWorkerFetchContextOnInitiatorThread() {
  DCHECK(initiator_thread_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(preference_watcher_receiver_.is_valid());

  // TODO(bashi): Consider changing ServiceWorkerFetchContextImpl to take
  // URLLoaderFactoryInfo.
  auto pending_script_loader_factory =
      std::make_unique<network::WrapperPendingSharedURLLoaderFactory>(std::move(
          service_worker_provider_info_->script_loader_factory_remote));

  return base::MakeRefCounted<ServiceWorkerFetchContextImpl>(
      *renderer_preferences_, script_url_, loader_factories_->PassInterface(),
      std::move(pending_script_loader_factory), script_url_to_skip_throttling_,
      GetContentClient()->renderer()->CreateURLLoaderThrottleProvider(
          URLLoaderThrottleProviderType::kWorker),
      GetContentClient()
          ->renderer()
          ->CreateWebSocketHandshakeThrottleProvider(),
      std::move(preference_watcher_receiver_),
      std::move(pending_subresource_loader_updater_), service_worker_route_id_,
      cors_exempt_header_list_);
}

void ServiceWorkerContextClient::OnNavigationPreloadResponse(
    int fetch_event_id,
    std::unique_ptr<blink::WebURLResponse> response,
    mojo::ScopedDataPipeConsumerHandle data_pipe) {
  DCHECK(worker_task_runner_->RunsTasksInCurrentSequence());
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
  DCHECK(worker_task_runner_->RunsTasksInCurrentSequence());
  // |context_| owns NavigationPreloadRequest which calls this.
  DCHECK(context_);
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
  DCHECK(worker_task_runner_->RunsTasksInCurrentSequence());
  // |context_| owns NavigationPreloadRequest which calls this.
  DCHECK(context_);
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

void ServiceWorkerContextClient::SendWorkerStarted(
    blink::mojom::ServiceWorkerStartStatus status) {
  DCHECK(worker_task_runner_->RunsTasksInCurrentSequence());
  // |context_| is valid because this task was posted to |worker_task_runner_|.
  DCHECK(context_);

  if (GetContentClient()->renderer()) {  // nullptr in unit_tests.
    GetContentClient()->renderer()->DidStartServiceWorkerContextOnWorkerThread(
        service_worker_version_id_, service_worker_scope_, script_url_);
  }

  // Temporary DCHECK for https://crbug.com/881100
  int64_t t0 =
      start_timing_->start_worker_received_time.since_origin().InMicroseconds();
  int64_t t1 = start_timing_->script_evaluation_start_time.since_origin()
                   .InMicroseconds();
  int64_t t2 =
      start_timing_->script_evaluation_end_time.since_origin().InMicroseconds();
  base::debug::Alias(&t0);
  base::debug::Alias(&t1);
  base::debug::Alias(&t2);
  CHECK_LE(start_timing_->start_worker_received_time,
           start_timing_->script_evaluation_start_time);
  CHECK_LE(start_timing_->script_evaluation_start_time,
           start_timing_->script_evaluation_end_time);

  instance_host_->OnStarted(status, proxy_->HasFetchHandler(),
                            WorkerThread::GetCurrentId(),
                            std::move(start_timing_));

  TRACE_EVENT_NESTABLE_ASYNC_END0("ServiceWorker", "ServiceWorkerContextClient",
                                  this);
}

void ServiceWorkerContextClient::SetupNavigationPreload(
    int fetch_event_id,
    const blink::WebURL& url,
    std::unique_ptr<blink::WebFetchEventPreloadHandle> preload_handle) {
  DCHECK(worker_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(context_);
  auto preload_request = std::make_unique<NavigationPreloadRequest>(
      this, fetch_event_id, GURL(url),
      blink::mojom::FetchEventPreloadHandle::New(
          std::move(preload_handle->url_loader),
          std::move(preload_handle->url_loader_client_receiver)));
  context_->preload_requests.AddWithID(std::move(preload_request),
                                       fetch_event_id);
}

void ServiceWorkerContextClient::RequestTermination(
    RequestTerminationCallback callback) {
  DCHECK(worker_task_runner_->RunsTasksInCurrentSequence());
  instance_host_->RequestTermination(std::move(callback));
}

void ServiceWorkerContextClient::StopWorkerOnInitiatorThread() {
  DCHECK(initiator_thread_task_runner_->RunsTasksInCurrentSequence());
  owner_->StopWorker();
}

base::WeakPtr<ServiceWorkerContextClient>
ServiceWorkerContextClient::GetWeakPtr() {
  DCHECK(worker_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(context_);
  return context_->weak_factory.GetWeakPtr();
}

}  // namespace content
