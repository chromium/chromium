// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/service_worker/service_worker_context_client.h"

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/debug/alias.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "content/public/common/content_features.h"
#include "content/public/common/referrer.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/public/renderer/worker_thread.h"
#include "content/renderer/mojo/blink_interface_registry_impl.h"
#include "content/renderer/renderer_blink_platform_impl.h"
#include "content/renderer/service_worker/embedded_worker_instance_client_impl.h"
#include "content/renderer/service_worker/service_worker_type_converters.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/cpp/wrapper_shared_url_loader_factory.h"
#include "third_party/blink/public/common/messaging/message_port_channel.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/blob/blob.mojom.h"
#include "third_party/blink/public/mojom/browser_interface_broker.mojom.h"
#include "third_party/blink/public/mojom/loader/request_context_frame_type.mojom.h"
#include "third_party/blink/public/mojom/renderer_preference_watcher.mojom.h"
#include "third_party/blink/public/mojom/service_worker/controller_service_worker.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_client.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_object.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"
#include "third_party/blink/public/mojom/worker/subresource_loader_updater.mojom.h"
#include "third_party/blink/public/platform/child_url_loader_factory_bundle.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_error.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_fetch_context.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/url_conversion.h"
#include "third_party/blink/public/platform/web_http_body.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/public/web/modules/service_worker/web_navigation_preload_request.h"
#include "third_party/blink/public/web/modules/service_worker/web_service_worker_context_client.h"
#include "third_party/blink/public/web/modules/service_worker/web_service_worker_context_proxy.h"

using blink::WebURLRequest;
using blink::MessagePortChannel;

namespace content {

namespace {

constexpr char kServiceWorkerContextClientScope[] =
    "ServiceWorkerContextClient";

std::string ComposeAlreadyInstalledString(bool is_starting_installed_worker) {
  return is_starting_installed_worker ? "AlreadyInstalled" : "NewlyInstalled";
}

}  // namespace

// Holds data that needs to be bound to the worker context on the
// worker thread.
struct ServiceWorkerContextClient::WorkerContextData {
  explicit WorkerContextData(ServiceWorkerContextClient* owner)
      : weak_factory(owner), proxy_weak_factory(owner->proxy_.get()) {}

  ~WorkerContextData() { DCHECK(thread_checker.CalledOnValidThread()); }

  // Inflight navigation preload requests.
  base::IDMap<std::unique_ptr<blink::WebNavigationPreloadRequest>>
      preload_requests;

  base::ThreadChecker thread_checker;
  base::WeakPtrFactory<ServiceWorkerContextClient> weak_factory;
  base::WeakPtrFactory<blink::WebServiceWorkerContextProxy> proxy_weak_factory;
};

ServiceWorkerContextClient::ServiceWorkerContextClient(
    int64_t service_worker_version_id,
    const GURL& service_worker_scope,
    const GURL& script_url,
    bool is_starting_installed_worker,
    const blink::RendererPreferences& renderer_preferences,
    mojo::PendingReceiver<blink::mojom::ServiceWorker> service_worker_receiver,
    mojo::PendingReceiver<blink::mojom::ControllerServiceWorker>
        controller_receiver,
    mojo::PendingAssociatedRemote<blink::mojom::EmbeddedWorkerInstanceHost>
        instance_host,
    mojo::PendingReceiver<service_manager::mojom::InterfaceProvider>
        pending_interface_provider_receiver,
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
    const std::vector<std::string>& cors_exempt_header_list,
    const blink::StorageKey& storage_key,
    const blink::ServiceWorkerToken& service_worker_token)
    : service_worker_version_id_(service_worker_version_id),
      service_worker_scope_(service_worker_scope),
      script_url_(script_url),
      is_starting_installed_worker_(is_starting_installed_worker),
      script_url_to_skip_throttling_(script_url_to_skip_throttling),
      renderer_preferences_(renderer_preferences),
      preference_watcher_receiver_(std::move(preference_watcher_receiver)),
      initiator_thread_task_runner_(std::move(initiator_thread_task_runner)),
      proxy_(nullptr),
      pending_service_worker_receiver_(std::move(service_worker_receiver)),
      controller_receiver_(std::move(controller_receiver)),
      pending_interface_provider_receiver_(
          std::move(pending_interface_provider_receiver)),
      pending_subresource_loader_updater_(
          std::move(subresource_loader_updater)),
      owner_(owner),
      start_timing_(std::move(start_timing)),
      service_worker_route_id_(service_worker_route_id),
      cors_exempt_header_list_(cors_exempt_header_list),
      storage_key_(storage_key),
      service_worker_token_(service_worker_token) {
  DCHECK(initiator_thread_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(owner_);
  DCHECK(subresource_loaders);
  instance_host_ =
      mojo::SharedAssociatedRemote<blink::mojom::EmbeddedWorkerInstanceHost>(
          std::move(instance_host), initiator_thread_task_runner_);

  // At the time of writing, there is no need for associated interfaces.
  blink_interface_registry_ = std::make_unique<BlinkInterfaceRegistryImpl>(
      registry_.GetWeakPtr(), /*associated_interface_registry=*/nullptr);

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

  loader_factories_ = base::MakeRefCounted<blink::ChildURLLoaderFactoryBundle>(
      std::make_unique<blink::ChildPendingURLLoaderFactoryBundle>(
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
  // Speculative fix on the memory leak.
  // We ensure `instance_host_` is reset before `initiator_thread_task_runner_`
  // is shut down (crbug.com/1409993).
  instance_host_.reset();
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
      std::move(browser_interface_broker), blink_interface_registry_.get(),
      initiator_thread_task_runner_);
}

blink::WebEmbeddedWorker& ServiceWorkerContextClient::worker() {
  DCHECK(initiator_thread_task_runner_->RunsTasksInCurrentSequence());
  return *worker_;
}

void ServiceWorkerContextClient::GetInterface(
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  if (registry_.TryBindInterface(interface_name, &interface_pipe))
    return;
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
  base::UmaHistogramTimes(
      base::StrCat(
          {"ServiceWorker.LoadTopLevelScript.FailedToFetchClassicScript.",
           ComposeAlreadyInstalledString(is_starting_installed_worker_),
           ".Time"}),
      base::TimeTicks::Now() - top_level_script_loading_start_time_);
  TRACE_EVENT_NESTABLE_ASYNC_END1("ServiceWorker", "LOAD_SCRIPT", this,
                                  "Status", "FailedToFetchClassicScript");
  // The caller is responsible for terminating the thread which
  // eventually destroys |this|.
}

void ServiceWorkerContextClient::FailedToFetchModuleScript() {
  DCHECK(worker_task_runner_->RunsTasksInCurrentSequence());
  base::UmaHistogramTimes(
      base::StrCat(
          {"ServiceWorker.LoadTopLevelScript.FailedToFetchModuleScript.",
           ComposeAlreadyInstalledString(is_starting_installed_worker_),
           ".Time"}),
      base::TimeTicks::Now() - top_level_script_loading_start_time_);
  TRACE_EVENT_NESTABLE_ASYNC_END1("ServiceWorker", "LOAD_SCRIPT", this,
                                  "Status", "FailedToFetchModuleScript");
  // The caller is responsible for terminating the thread which
  // eventually destroys |this|.
}

void ServiceWorkerContextClient::WorkerScriptLoadedOnWorkerThread() {
  DCHECK(worker_task_runner_->RunsTasksInCurrentSequence());
  instance_host_->OnScriptLoaded();
  base::UmaHistogramTimes(
      base::StrCat(
          {"ServiceWorker.LoadTopLevelScript.Succeeded.",
           ComposeAlreadyInstalledString(is_starting_installed_worker_),
           ".Time"}),
      base::TimeTicks::Now() - top_level_script_loading_start_time_);
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

  DCHECK(pending_interface_provider_receiver_.is_valid());
  interface_provider_receiver_.Bind(
      std::move(pending_interface_provider_receiver_));

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
      script_url_, service_worker_token_);
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

  // After WillDestroyWorkerContext is called, the ServiceWorkerContext
  // is destroyed, so destroy InterfaceProvider here and clear the
  // BinderRegistry to stop any future interface requests. InterfaceProvider is
  // bound on the worker task runner and therefore, should be destroyed on the
  // worker task runner.
  interface_provider_receiver_.reset();
  registry_.clear();

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

  // TODO(bashi): Consider changing WebServiceWorkerFetchContext to take
  // URLLoaderFactoryInfo.
  auto pending_script_loader_factory =
      std::make_unique<network::WrapperPendingSharedURLLoaderFactory>(std::move(
          service_worker_provider_info_->script_loader_factory_remote));

  blink::WebVector<blink::WebString> web_cors_exempt_header_list(
      cors_exempt_header_list_.size());
  base::ranges::transform(
      cors_exempt_header_list_, web_cors_exempt_header_list.begin(),
      [](const auto& header) { return blink::WebString::FromLatin1(header); });

  return blink::WebServiceWorkerFetchContext::Create(
      renderer_preferences_, script_url_, loader_factories_->PassInterface(),
      std::move(pending_script_loader_factory), script_url_to_skip_throttling_,
      GetContentClient()->renderer()->CreateURLLoaderThrottleProvider(
          blink::URLLoaderThrottleProviderType::kWorker),
      GetContentClient()
          ->renderer()
          ->CreateWebSocketHandshakeThrottleProvider(),
      std::move(preference_watcher_receiver_),
      std::move(pending_subresource_loader_updater_),
      web_cors_exempt_header_list, storage_key_.IsThirdPartyContext());
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
  // |context_| owns WebNavigationPreloadRequest which calls this.
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
  // |context_| owns WebNavigationPreloadRequest which calls this.
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

  instance_host_->OnStarted(
      status, proxy_->FetchHandlerType(), proxy_->HasHidEventHandlers(),
      proxy_->HasUsbEventHandlers(), WorkerThread::GetCurrentId(),
      std::move(start_timing_));

  TRACE_EVENT_NESTABLE_ASYNC_END0("ServiceWorker", "ServiceWorkerContextClient",
                                  this);
}

void ServiceWorkerContextClient::SetupNavigationPreload(
    int fetch_event_id,
    const blink::WebURL& url,
    blink::CrossVariantMojoReceiver<
        network::mojom::URLLoaderClientInterfaceBase>
        preload_url_loader_client_receiver) {
  DCHECK(worker_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(context_);
  auto preload_request = blink::WebNavigationPreloadRequest::Create(
      this, fetch_event_id, url, std::move(preload_url_loader_client_receiver));
  context_->preload_requests.AddWithID(std::move(preload_request),
                                       fetch_event_id);
}

void ServiceWorkerContextClient::RequestTermination(
    RequestTerminationCallback callback) {
  DCHECK(worker_task_runner_->RunsTasksInCurrentSequence());
  instance_host_->RequestTermination(std::move(callback));
}

bool ServiceWorkerContextClient::ShouldNotifyServiceWorkerOnWebSocketActivity(
    v8::Local<v8::Context> context) {
  return GetContentClient()
      ->renderer()
      ->ShouldNotifyServiceWorkerOnWebSocketActivity(context);
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
