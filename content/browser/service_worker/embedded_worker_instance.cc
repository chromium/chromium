// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/embedded_worker_instance.h"

#include <utility>

#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/feature_list.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/optional.h"
#include "base/task/post_task.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/bad_message.h"
#include "content/browser/devtools/service_worker_devtools_manager.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/service_worker/embedded_worker_registry.h"
#include "content/browser/service_worker/embedded_worker_status.h"
#include "content/browser/service_worker/service_worker_content_settings_proxy_impl.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/url_loader_factory_getter.h"
#include "content/common/content_switches_internal.h"
#include "content/common/renderer.mojom.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/common/url_loader_factory_bundle.mojom.h"
#include "content/common/url_schemes.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/child_process_host.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/renderer_preference_watcher.mojom.h"
#include "ipc/ipc_message.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/service_worker/service_worker_utils.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_object.mojom.h"
#include "third_party/blink/public/web/web_console_message.h"
#include "url/gurl.h"

namespace content {

namespace {

base::Optional<EmbeddedWorkerInstance::CreateNetworkFactoryCallback>&
GetNetworkFactoryCallbackForTest() {
  static base::NoDestructor<
      base::Optional<EmbeddedWorkerInstance::CreateNetworkFactoryCallback>>
      callback;
  return *callback;
}

// When a service worker version's failure count exceeds
// |kMaxSameProcessFailureCount|, the embedded worker is forced to start in a
// new process.
const int kMaxSameProcessFailureCount = 2;

const char kServiceWorkerTerminationCanceledMesage[] =
    "Service Worker termination by a timeout timer was canceled because "
    "DevTools is attached.";

void NotifyWorkerReadyForInspectionOnUI(
    int worker_process_id,
    int worker_route_id,
    blink::mojom::DevToolsAgentHostAssociatedRequest host_request,
    blink::mojom::DevToolsAgentAssociatedPtrInfo devtools_agent_ptr_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ServiceWorkerDevToolsManager::GetInstance()->WorkerReadyForInspection(
      worker_process_id, worker_route_id, std::move(host_request),
      std::move(devtools_agent_ptr_info));
}

void NotifyWorkerDestroyedOnUI(int worker_process_id, int worker_route_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ServiceWorkerDevToolsManager::GetInstance()->WorkerDestroyed(
      worker_process_id, worker_route_id);
}

void NotifyWorkerVersionInstalledOnUI(int worker_process_id,
                                      int worker_route_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ServiceWorkerDevToolsManager::GetInstance()->WorkerVersionInstalled(
      worker_process_id, worker_route_id);
}

void NotifyWorkerVersionDoomedOnUI(int worker_process_id, int worker_route_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ServiceWorkerDevToolsManager::GetInstance()->WorkerVersionDoomed(
      worker_process_id, worker_route_id);
}

// Returns a factory bundle for doing loads on behalf of the specified |rph| and
// |origin|. The returned bundle has a default factory that goes to network and
// if |use_non_network_factories| is true it may also include scheme-specific
// factories that don't go to network.
//
// The network factory does not support reconnection to the network service.
std::unique_ptr<URLLoaderFactoryBundleInfo> CreateFactoryBundle(
    RenderProcessHost* rph,
    const url::Origin& origin,
    bool use_non_network_factories) {
  auto factory_bundle = std::make_unique<URLLoaderFactoryBundleInfo>();
  network::mojom::URLLoaderFactoryPtrInfo default_factory_info;
  if (!GetNetworkFactoryCallbackForTest()) {
    rph->CreateURLLoaderFactory(origin,
                                mojo::MakeRequest(&default_factory_info));
  } else {
    network::mojom::URLLoaderFactoryPtr original_factory;
    rph->CreateURLLoaderFactory(origin, mojo::MakeRequest(&original_factory));
    GetNetworkFactoryCallbackForTest()->Run(
        mojo::MakeRequest(&default_factory_info), rph->GetID(),
        original_factory.PassInterface());
  }
  factory_bundle->default_factory_info() = std::move(default_factory_info);

  if (use_non_network_factories) {
    ContentBrowserClient::NonNetworkURLLoaderFactoryMap non_network_factories;
    GetContentClient()
        ->browser()
        ->RegisterNonNetworkSubresourceURLLoaderFactories(
            rph->GetID(), MSG_ROUTING_NONE, &non_network_factories);

    for (auto& pair : non_network_factories) {
      const std::string& scheme = pair.first;
      std::unique_ptr<network::mojom::URLLoaderFactory> factory =
          std::move(pair.second);

      // To be safe, ignore schemes that aren't allowed to register service
      // workers. We assume that importScripts and fetch() should fail on such
      // schemes.
      if (!base::ContainsValue(GetServiceWorkerSchemes(), scheme))
        continue;
      network::mojom::URLLoaderFactoryPtr factory_ptr;
      mojo::MakeStrongBinding(std::move(factory),
                              mojo::MakeRequest(&factory_ptr));
      factory_bundle->scheme_specific_factory_infos().emplace(
          scheme, factory_ptr.PassInterface());
    }
  }
  return factory_bundle;
}

using SetupProcessCallback = base::OnceCallback<void(
    blink::ServiceWorkerStatusCode,
    mojom::EmbeddedWorkerStartParamsPtr,
    std::unique_ptr<ServiceWorkerProcessManager::AllocatedProcessInfo>,
    std::unique_ptr<EmbeddedWorkerInstance::DevToolsProxy>,
    std::unique_ptr<
        URLLoaderFactoryBundleInfo> /* factory_bundle_for_browser */,
    std::unique_ptr<
        URLLoaderFactoryBundleInfo> /* factory_bundle_for_renderer */,
    blink::mojom::CacheStoragePtrInfo)>;

// Allocates a renderer process for starting a worker and does setup like
// registering with DevTools. Called on the UI thread. Calls |callback| on the
// IO thread. |context| and |weak_context| are only for passing to DevTools and
// must not be dereferenced here on the UI thread.
// S13nServiceWorker:
// This also sets up two URLLoaderFactoryBundles, one for
// ServiceWorkerScriptLoaderFactory and the other is for passing to the
// renderer. These bundles include factories for non-network URLs like
// chrome-extension:// as needed.
void SetupOnUIThread(base::WeakPtr<ServiceWorkerProcessManager> process_manager,
                     bool can_use_existing_process,
                     mojom::EmbeddedWorkerStartParamsPtr params,
                     mojom::EmbeddedWorkerInstanceClientRequest request,
                     ServiceWorkerContextCore* context,
                     base::WeakPtr<ServiceWorkerContextCore> weak_context,
                     SetupProcessCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto process_info =
      std::make_unique<ServiceWorkerProcessManager::AllocatedProcessInfo>();
  std::unique_ptr<EmbeddedWorkerInstance::DevToolsProxy> devtools_proxy;
  std::unique_ptr<URLLoaderFactoryBundleInfo> factory_bundle_for_browser;
  std::unique_ptr<URLLoaderFactoryBundleInfo> factory_bundle_for_renderer;

  if (!process_manager) {
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(
            std::move(callback), blink::ServiceWorkerStatusCode::kErrorAbort,
            std::move(params), std::move(process_info),
            std::move(devtools_proxy), std::move(factory_bundle_for_browser),
            std::move(factory_bundle_for_renderer),
            nullptr /* cache_storage */));
    return;
  }

  // Get a process.
  blink::ServiceWorkerStatusCode status =
      process_manager->AllocateWorkerProcess(
          params->embedded_worker_id, params->script_url,
          can_use_existing_process, process_info.get());
  if (status != blink::ServiceWorkerStatusCode::kOk) {
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(std::move(callback), status, std::move(params),
                       std::move(process_info), std::move(devtools_proxy),
                       std::move(factory_bundle_for_browser),
                       std::move(factory_bundle_for_renderer),
                       nullptr /* cache_storage */));
    return;
  }
  const int process_id = process_info->process_id;
  RenderProcessHost* rph = RenderProcessHost::FromID(process_id);
  // TODO(falken): This CHECK should no longer fail, so turn to a DCHECK it if
  // crash reports agree. Consider also checking for
  // rph->IsInitializedAndNotDead().
  CHECK(rph);

  // Create cache storage now as an optimization, so the service worker can use
  // the Cache Storage API immediately on startup. Don't do this when
  // byte-to-byte check will be performed on the worker (|pause_after_download|)
  // as most of those workers will have byte-to-byte equality and abort instead
  // of running.
  blink::mojom::CacheStoragePtr cache_storage;
  if (base::FeatureList::IsEnabled(
          blink::features::kEagerCacheStorageSetupForServiceWorkers) &&
      !params->pause_after_download) {
    rph->BindCacheStorage(mojo::MakeRequest(&cache_storage),
                          url::Origin::Create(params->script_url));
  }

  // Bind |request|, which is attached to |EmbeddedWorkerInstance::client_|, to
  // the process. If the process dies, |client_|'s connection error callback
  // will be called on the IO thread.
  if (request.is_pending()) {
    rph->GetRendererInterface()->SetUpEmbeddedWorkerChannelForServiceWorker(
        std::move(request));
  }

  // S13nServiceWorker: Create factory bundles for this worker to do loading.
  // These bundles don't support reconnection to the network service, see
  // below comments.
  if (blink::ServiceWorkerUtils::IsServicificationEnabled()) {
    // For performance, we only create the loader factories for non-http(s)
    // URLs (e.g. chrome-extension://) when the main script URL is
    // non-http(s). We assume an http(s) service worker cannot
    // importScripts or fetch() a non-http(s) URL.
    bool use_non_network_factories = !params->script_url.SchemeIsHTTPOrHTTPS();

    url::Origin origin = url::Origin::Create(params->script_url);

    // The bundle for the browser is passed to ServiceWorkerScriptLoaderFactory
    // and used to request non-installed service worker scripts. It's OK to not
    // support reconnection to then network service because it can only used
    // until the service worker reaches the 'installed' state.
    //
    // TODO(falken): Only make this bundle for non-installed service workers.
    factory_bundle_for_browser =
        CreateFactoryBundle(rph, origin, use_non_network_factories);

    // The bundle for the renderer is passed to the service worker, and
    // used for subresource loading from the service worker (i.e., fetch()).
    // It's OK to not support reconnection to the network service because the
    // service worker terminates itself when the connection breaks, so a new
    // instance can be started.
    factory_bundle_for_renderer =
        CreateFactoryBundle(rph, origin, use_non_network_factories);
  }

  // Register to DevTools and update params accordingly.
  // TODO(dgozman): we can now remove this routing id and use something else
  // as id when talking to ServiceWorkerDevToolsManager.
  const int routing_id = rph->GetNextRoutingID();
  ServiceWorkerDevToolsManager::GetInstance()->WorkerCreated(
      process_id, routing_id, context, weak_context,
      params->service_worker_version_id, params->script_url, params->scope,
      params->is_installed, &params->devtools_worker_token,
      &params->wait_for_debugger);
  params->worker_devtools_agent_route_id = routing_id;
  // Create DevToolsProxy here to ensure that the WorkerCreated() call is
  // balanced by DevToolsProxy's destructor calling WorkerDestroyed().
  devtools_proxy = std::make_unique<EmbeddedWorkerInstance::DevToolsProxy>(
      process_id, routing_id);

  // TODO(crbug.com/862854): Support changes to RendererPreferences while the
  // worker is running.
  DCHECK(process_manager->browser_context() || process_manager->IsShutdown());
  GetContentClient()->browser()->UpdateRendererPreferencesForWorker(
      process_manager->browser_context(), &params->renderer_preferences);

  // Create a RendererPreferenceWatcher to observe updates in the preferences.
  mojom::RendererPreferenceWatcherPtr watcher_ptr;
  params->preference_watcher_request = mojo::MakeRequest(&watcher_ptr);
  GetContentClient()->browser()->RegisterRendererPreferenceWatcherForWorkers(
      process_manager->browser_context(), std::move(watcher_ptr));

  // Continue to OnSetupCompleted on the IO thread.
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(std::move(callback), status, std::move(params),
                     std::move(process_info), std::move(devtools_proxy),
                     std::move(factory_bundle_for_browser),
                     std::move(factory_bundle_for_renderer),
                     cache_storage.PassInterface()));
}

bool HasSentStartWorker(EmbeddedWorkerInstance::StartingPhase phase) {
  switch (phase) {
    case EmbeddedWorkerInstance::NOT_STARTING:
    case EmbeddedWorkerInstance::ALLOCATING_PROCESS:
      return false;
    case EmbeddedWorkerInstance::SENT_START_WORKER:
    case EmbeddedWorkerInstance::SCRIPT_DOWNLOADING:
    case EmbeddedWorkerInstance::SCRIPT_READ_STARTED:
    case EmbeddedWorkerInstance::SCRIPT_READ_FINISHED:
    case EmbeddedWorkerInstance::SCRIPT_STREAMING:
    case EmbeddedWorkerInstance::SCRIPT_LOADED:
    case EmbeddedWorkerInstance::SCRIPT_EVALUATION:
      return true;
    case EmbeddedWorkerInstance::STARTING_PHASE_MAX_VALUE:
      NOTREACHED();
  }
  return false;
}

}  // namespace

// Created on UI thread and moved to IO thread. Proxies notifications to
// DevToolsManager that lives on UI thread. Owned by EmbeddedWorkerInstance.
class EmbeddedWorkerInstance::DevToolsProxy {
 public:
  DevToolsProxy(int process_id, int agent_route_id)
      : process_id_(process_id),
        agent_route_id_(agent_route_id) {}

  ~DevToolsProxy() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI},
                             base::BindOnce(NotifyWorkerDestroyedOnUI,
                                            process_id_, agent_route_id_));
  }

  void NotifyWorkerReadyForInspection(
      blink::mojom::DevToolsAgentHostAssociatedRequest host_request,
      blink::mojom::DevToolsAgentAssociatedPtrInfo devtools_agent_ptr_info) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(NotifyWorkerReadyForInspectionOnUI, process_id_,
                       agent_route_id_, std::move(host_request),
                       std::move(devtools_agent_ptr_info)));
  }

  void NotifyWorkerVersionInstalled() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI},
                             base::BindOnce(NotifyWorkerVersionInstalledOnUI,
                                            process_id_, agent_route_id_));
  }

  void NotifyWorkerVersionDoomed() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI},
                             base::BindOnce(NotifyWorkerVersionDoomedOnUI,
                                            process_id_, agent_route_id_));
  }

  bool ShouldNotifyWorkerStopIgnored() const {
    return !worker_stop_ignored_notified_;
  }

  void WorkerStopIgnoredNotified() { worker_stop_ignored_notified_ = true; }

  int agent_route_id() const { return agent_route_id_; }

 private:
  const int process_id_;
  const int agent_route_id_;
  bool worker_stop_ignored_notified_ = false;

  DISALLOW_COPY_AND_ASSIGN(DevToolsProxy);
};

// A handle for a renderer process managed by ServiceWorkerProcessManager on the
// UI thread. Lives on the IO thread.
class EmbeddedWorkerInstance::WorkerProcessHandle {
 public:
  WorkerProcessHandle(
      const base::WeakPtr<ServiceWorkerProcessManager>& process_manager,
      int embedded_worker_id,
      int process_id)
      : process_manager_(process_manager),
        embedded_worker_id_(embedded_worker_id),
        process_id_(process_id) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    DCHECK_NE(ChildProcessHost::kInvalidUniqueID, process_id_);
  }

  ~WorkerProcessHandle() {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(&ServiceWorkerProcessManager::ReleaseWorkerProcess,
                       process_manager_, embedded_worker_id_));
  }

  int process_id() const { return process_id_; }

 private:
  // Can be dereferenced on the UI thread only.
  base::WeakPtr<ServiceWorkerProcessManager> process_manager_;

  const int embedded_worker_id_;
  const int process_id_;

  DISALLOW_COPY_AND_ASSIGN(WorkerProcessHandle);
};

// A task to allocate a worker process and to send a start worker message. This
// is created on EmbeddedWorkerInstance::Start(), owned by the instance and
// destroyed on EmbeddedWorkerInstance::OnScriptEvaluated().
// We can abort starting worker by destroying this task anytime during the
// sequence.
// Lives on the IO thread.
class EmbeddedWorkerInstance::StartTask {
 public:
  enum class ProcessAllocationState { NOT_ALLOCATED, ALLOCATING, ALLOCATED };

  StartTask(EmbeddedWorkerInstance* instance,
            const GURL& script_url,
            mojom::EmbeddedWorkerInstanceClientRequest request,
            base::TimeTicks start_time)
      : instance_(instance),
        request_(std::move(request)),
        state_(ProcessAllocationState::NOT_ALLOCATED),
        is_installed_(false),
        started_during_browser_startup_(false),
        skip_recording_startup_time_(instance_->devtools_attached()),
        start_time_(start_time),
        weak_factory_(this) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    TRACE_EVENT_NESTABLE_ASYNC_BEGIN1("ServiceWorker",
                                      "EmbeddedWorkerInstance::Start", this,
                                      "Script", script_url.spec());
  }

  ~StartTask() {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    if (did_send_start_) {
      TRACE_EVENT_NESTABLE_ASYNC_END0("ServiceWorker",
                                      "INITIALIZING_ON_RENDERER", this);
    }

    TRACE_EVENT_NESTABLE_ASYNC_END0("ServiceWorker",
                                    "EmbeddedWorkerInstance::Start", this);

    if (!instance_->context_)
      return;

    switch (state_) {
      case ProcessAllocationState::NOT_ALLOCATED:
        // Not necessary to release a process.
        break;
      case ProcessAllocationState::ALLOCATING:
        // Abort half-baked process allocation on the UI thread.
        base::PostTaskWithTraits(
            FROM_HERE, {BrowserThread::UI},
            base::BindOnce(&ServiceWorkerProcessManager::ReleaseWorkerProcess,
                           instance_->context_->process_manager()->AsWeakPtr(),
                           instance_->embedded_worker_id()));
        break;
      case ProcessAllocationState::ALLOCATED:
        // Otherwise, the process will be released by EmbeddedWorkerInstance.
        break;
    }

    // Don't have to abort |sent_start_callback_| here. The caller of
    // EmbeddedWorkerInstance::Start(), that is, ServiceWorkerVersion does not
    // expect it when the start worker sequence is canceled by Stop() because
    // the callback, ServiceWorkerVersion::OnStartSentAndScriptEvaluated(),
    // could drain valid start requests queued in the version. After the worker
    // is stopped, the version attempts to restart the worker if there are
    // requests in the queue. See ServiceWorkerVersion::OnStoppedInternal() for
    // details.
    // TODO(crbug.com/859912): Reconsider this bizarre layering.
  }

  base::TimeTicks start_time() const { return start_time_; }

  void set_start_worker_sent_time(base::TimeTicks time) {
    start_worker_sent_time_ = time;
  }
  base::TimeTicks start_worker_sent_time() const {
    return start_worker_sent_time_;
  }

  void set_skip_recording_startup_time() {
    skip_recording_startup_time_ = true;
  }
  bool skip_recording_startup_time() const {
    return skip_recording_startup_time_;
  }

  void Start(mojom::EmbeddedWorkerStartParamsPtr params,
             StatusCallback sent_start_callback) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    DCHECK(instance_->context_);
    base::WeakPtr<ServiceWorkerContextCore> context = instance_->context_;
    state_ = ProcessAllocationState::ALLOCATING;
    sent_start_callback_ = std::move(sent_start_callback);
    is_installed_ = params->is_installed;

    if (!GetContentClient()->browser()->IsBrowserStartupComplete())
      started_during_browser_startup_ = true;

    bool can_use_existing_process =
        context->GetVersionFailureCount(params->service_worker_version_id) <
        kMaxSameProcessFailureCount;
    DCHECK_EQ(params->embedded_worker_id, instance_->embedded_worker_id_);
    TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("ServiceWorker", "ALLOCATING_PROCESS",
                                      this);
    base::WeakPtr<ServiceWorkerProcessManager> process_manager =
        context->process_manager()->AsWeakPtr();

    // Hop to the UI thread for process allocation and setup. We will continue
    // on the IO thread in StartTask::OnSetupCompleted().
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(
            &SetupOnUIThread, process_manager, can_use_existing_process,
            std::move(params), std::move(request_), context.get(), context,
            base::BindOnce(&StartTask::OnSetupCompleted,
                           weak_factory_.GetWeakPtr(), process_manager)));
  }

  bool is_installed() const { return is_installed_; }

 private:
  void OnSetupCompleted(
      base::WeakPtr<ServiceWorkerProcessManager> process_manager,
      blink::ServiceWorkerStatusCode status,
      mojom::EmbeddedWorkerStartParamsPtr params,
      std::unique_ptr<ServiceWorkerProcessManager::AllocatedProcessInfo>
          process_info,
      std::unique_ptr<EmbeddedWorkerInstance::DevToolsProxy> devtools_proxy,
      std::unique_ptr<URLLoaderFactoryBundleInfo> factory_bundle_for_browser,
      std::unique_ptr<URLLoaderFactoryBundleInfo> factory_bundle_for_renderer,
      blink::mojom::CacheStoragePtrInfo cache_storage) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);

    std::unique_ptr<WorkerProcessHandle> process_handle;
    if (status == blink::ServiceWorkerStatusCode::kOk) {
      // If we allocated a process, WorkerProcessHandle has to be created before
      // returning to ensure the process is eventually released.
      process_handle = std::make_unique<WorkerProcessHandle>(
          process_manager, instance_->embedded_worker_id(),
          process_info->process_id);

      if (!instance_->context_)
        status = blink::ServiceWorkerStatusCode::kErrorAbort;
    }

    if (status != blink::ServiceWorkerStatusCode::kOk) {
      TRACE_EVENT_NESTABLE_ASYNC_END1(
          "ServiceWorker", "ALLOCATING_PROCESS", this, "Error",
          blink::ServiceWorkerStatusToString(status));
      instance_->OnSetupFailed(std::move(sent_start_callback_), status);
      // |this| may be destroyed.
      return;
    }

    ServiceWorkerMetrics::StartSituation start_situation =
        process_info->start_situation;
    TRACE_EVENT_NESTABLE_ASYNC_END1(
        "ServiceWorker", "ALLOCATING_PROCESS", this, "StartSituation",
        ServiceWorkerMetrics::StartSituationToString(start_situation));
    if (is_installed_) {
      ServiceWorkerMetrics::RecordProcessCreated(
          start_situation == ServiceWorkerMetrics::StartSituation::NEW_PROCESS);
    }

    if (started_during_browser_startup_)
      start_situation = ServiceWorkerMetrics::StartSituation::DURING_STARTUP;

    // Notify the instance that a process is allocated.
    state_ = ProcessAllocationState::ALLOCATED;
    instance_->OnProcessAllocated(std::move(process_handle), start_situation);

    // Notify the instance that it is registered to the DevTools manager.
    instance_->OnRegisteredToDevToolsManager(std::move(devtools_proxy),
                                             params->wait_for_debugger);

    // S13nServiceWorker: Build the URLLoaderFactory for loading new scripts.
    scoped_refptr<network::SharedURLLoaderFactory> factory_for_new_scripts;
    if (blink::ServiceWorkerUtils::IsServicificationEnabled()) {
      DCHECK(factory_bundle_for_browser);
      factory_for_new_scripts = base::MakeRefCounted<URLLoaderFactoryBundle>(
          std::move(factory_bundle_for_browser));

      // Send the factory bundle for subresource loading from the service worker
      // (i.e. fetch()).
      DCHECK(factory_bundle_for_renderer);
      params->subresource_loader_factories =
          std::move(factory_bundle_for_renderer);
    }

    instance_->SendStartWorker(std::move(params),
                               std::move(factory_for_new_scripts),
                               std::move(cache_storage));
    std::move(sent_start_callback_).Run(blink::ServiceWorkerStatusCode::kOk);

    TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("ServiceWorker",
                                      "INITIALIZING_ON_RENDERER", this);
    did_send_start_ = true;
    // |this|'s work is done here, but |instance_| still uses its state until
    // startup is complete.
  }

  // |instance_| must outlive |this|.
  EmbeddedWorkerInstance* instance_;

  // Ownership is transferred by a PostTask() call after process allocation.
  mojom::EmbeddedWorkerInstanceClientRequest request_;

  StatusCallback sent_start_callback_;
  bool did_send_start_ = false;
  ProcessAllocationState state_;

  // Used for UMA.
  bool is_installed_;
  bool started_during_browser_startup_;
  bool skip_recording_startup_time_;
  base::TimeTicks start_time_;
  base::TimeTicks start_worker_sent_time_;

  base::WeakPtrFactory<StartTask> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(StartTask);
};

EmbeddedWorkerInstance::~EmbeddedWorkerInstance() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(status_ == EmbeddedWorkerStatus::STOPPING ||
         status_ == EmbeddedWorkerStatus::STOPPED)
      << static_cast<int>(status_);
  devtools_proxy_.reset();
  if (registry_->GetWorker(embedded_worker_id_))
    registry_->RemoveWorker(process_id(), embedded_worker_id_);
  process_handle_.reset();
}

void EmbeddedWorkerInstance::Start(mojom::EmbeddedWorkerStartParamsPtr params,
                                   StatusCallback callback) {
  DCHECK(context_);
  restart_count_++;
  DCHECK_EQ(EmbeddedWorkerStatus::STOPPED, status_);

  DCHECK(!params->pause_after_download || !params->is_installed);
  DCHECK_NE(blink::mojom::kInvalidServiceWorkerVersionId,
            params->service_worker_version_id);

  auto start_time = base::TimeTicks::Now();
  status_ = EmbeddedWorkerStatus::STARTING;
  starting_phase_ = ALLOCATING_PROCESS;
  network_accessed_for_script_ = false;

  for (auto& observer : listener_list_)
    observer.OnStarting();

  params->embedded_worker_id = embedded_worker_id_;
  params->worker_devtools_agent_route_id = MSG_ROUTING_NONE;
  params->wait_for_debugger = false;
  params->v8_cache_options = GetV8CacheOptions();

  mojom::EmbeddedWorkerInstanceClientRequest request =
      mojo::MakeRequest(&client_);
  client_.set_connection_error_handler(
      base::BindOnce(&EmbeddedWorkerInstance::Detach, base::Unretained(this)));
  inflight_start_task_.reset(
      new StartTask(this, params->script_url, std::move(request), start_time));
  inflight_start_task_->Start(std::move(params), std::move(callback));
}

void EmbeddedWorkerInstance::Stop() {
  DCHECK(status_ == EmbeddedWorkerStatus::STARTING ||
         status_ == EmbeddedWorkerStatus::RUNNING)
      << static_cast<int>(status_);

  // Abort an inflight start task.
  inflight_start_task_.reset();

  // Don't send the StopWorker message if the StartWorker message hasn't
  // been sent.
  if (status_ == EmbeddedWorkerStatus::STARTING &&
      !HasSentStartWorker(starting_phase())) {
    ReleaseProcess();
    for (auto& observer : listener_list_)
      observer.OnStopped(EmbeddedWorkerStatus::STARTING /* old_status */);
    return;
  }

  client_->StopWorker();
  status_ = EmbeddedWorkerStatus::STOPPING;
  for (auto& observer : listener_list_)
    observer.OnStopping();
}

void EmbeddedWorkerInstance::StopIfNotAttachedToDevTools() {
  if (devtools_attached_) {
    if (devtools_proxy_) {
      // Check ShouldNotifyWorkerStopIgnored not to show the same message
      // multiple times in DevTools.
      if (devtools_proxy_->ShouldNotifyWorkerStopIgnored()) {
        AddMessageToConsole(blink::WebConsoleMessage::kLevelVerbose,
                            kServiceWorkerTerminationCanceledMesage);
        devtools_proxy_->WorkerStopIgnoredNotified();
      }
    }
    return;
  }
  Stop();
}

void EmbeddedWorkerInstance::ResumeAfterDownload() {
  if (process_id() == ChildProcessHost::kInvalidUniqueID ||
      status_ != EmbeddedWorkerStatus::STARTING) {
    return;
  }
  DCHECK(client_.is_bound());
  client_->ResumeAfterDownload();
}

EmbeddedWorkerInstance::EmbeddedWorkerInstance(
    base::WeakPtr<ServiceWorkerContextCore> context,
    ServiceWorkerVersion* owner_version,
    int embedded_worker_id)
    : context_(context),
      registry_(context->embedded_worker_registry()),
      owner_version_(owner_version),
      embedded_worker_id_(embedded_worker_id),
      status_(EmbeddedWorkerStatus::STOPPED),
      starting_phase_(NOT_STARTING),
      restart_count_(0),
      thread_id_(kInvalidEmbeddedWorkerThreadId),
      instance_host_binding_(this),
      devtools_attached_(false),
      network_accessed_for_script_(false),
      weak_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

void EmbeddedWorkerInstance::OnProcessAllocated(
    std::unique_ptr<WorkerProcessHandle> handle,
    ServiceWorkerMetrics::StartSituation start_situation) {
  DCHECK_EQ(EmbeddedWorkerStatus::STARTING, status_);
  DCHECK(!process_handle_);

  process_handle_ = std::move(handle);
  start_situation_ = start_situation;
  for (auto& observer : listener_list_)
    observer.OnProcessAllocated();
}

void EmbeddedWorkerInstance::OnRegisteredToDevToolsManager(
    std::unique_ptr<DevToolsProxy> devtools_proxy,
    bool wait_for_debugger) {
  if (devtools_proxy) {
    DCHECK(!devtools_proxy_);
    devtools_proxy_ = std::move(devtools_proxy);
  }
  if (wait_for_debugger)
    inflight_start_task_->set_skip_recording_startup_time();
  for (auto& observer : listener_list_)
    observer.OnRegisteredToDevToolsManager();
}

void EmbeddedWorkerInstance::SendStartWorker(
    mojom::EmbeddedWorkerStartParamsPtr params,
    scoped_refptr<network::SharedURLLoaderFactory> factory,
    blink::mojom::CacheStoragePtrInfo cache_storage) {
  DCHECK(context_);
  DCHECK(params->service_worker_request.is_pending());
  DCHECK(params->controller_request.is_pending());
  DCHECK(!instance_host_binding_.is_bound());
  instance_host_binding_.Bind(mojo::MakeRequest(&params->instance_host));

  blink::mojom::WorkerContentSettingsProxyPtr content_settings_proxy_ptr_info;
  content_settings_ = std::make_unique<ServiceWorkerContentSettingsProxyImpl>(
      params->script_url, context_,
      mojo::MakeRequest(&params->content_settings_proxy));

  const bool is_script_streaming = !params->installed_scripts_info.is_null();
  inflight_start_task_->set_start_worker_sent_time(base::TimeTicks::Now());

  // The host must be alive as long as |params->provider_info| is alive.
  DCHECK(owner_version_->provider_host());
  params->provider_info =
      owner_version_->provider_host()->CompleteStartWorkerPreparation(
          process_id(), std::move(factory), std::move(params->provider_info));
  params->provider_info->cache_storage = std::move(cache_storage);

  client_->StartWorker(std::move(params));
  registry_->BindWorkerToProcess(process_id(), embedded_worker_id());

  starting_phase_ = is_script_streaming ? SCRIPT_STREAMING : SENT_START_WORKER;
  for (auto& observer : listener_list_)
    observer.OnStartWorkerMessageSent();
}

void EmbeddedWorkerInstance::RequestTermination(
    RequestTerminationCallback callback) {
  if (!blink::ServiceWorkerUtils::IsServicificationEnabled()) {
    mojo::ReportBadMessage(
        "Invalid termination request: RequestTermination() was called but "
        "S13nServiceWorker is not enabled");
    std::move(callback).Run(true /* will_be_terminated */);
    return;
  }

  if (status() != EmbeddedWorkerStatus::RUNNING &&
      status() != EmbeddedWorkerStatus::STOPPING) {
    mojo::ReportBadMessage(
        "Invalid termination request: Termination should be requested during "
        "running or stopping");
    std::move(callback).Run(true /* will_be_terminated */);
    return;
  }

  std::move(callback).Run(owner_version_->OnRequestTermination());
}

void EmbeddedWorkerInstance::CountFeature(blink::mojom::WebFeature feature) {
  owner_version_->CountFeature(feature);
}

void EmbeddedWorkerInstance::OnReadyForInspection() {
  if (!devtools_proxy_)
    return;
  blink::mojom::DevToolsAgentHostAssociatedPtrInfo host_ptr_info;
  blink::mojom::DevToolsAgentHostAssociatedRequest host_request =
      mojo::MakeRequest(&host_ptr_info);
  blink::mojom::DevToolsAgentAssociatedPtrInfo devtools_agent_ptr_info;
  client_->BindDevToolsAgent(std::move(host_ptr_info),
                             mojo::MakeRequest(&devtools_agent_ptr_info));
  devtools_proxy_->NotifyWorkerReadyForInspection(
      std::move(host_request), std::move(devtools_agent_ptr_info));
}

void EmbeddedWorkerInstance::OnScriptReadStarted() {
  starting_phase_ = SCRIPT_READ_STARTED;
}

void EmbeddedWorkerInstance::OnScriptReadFinished() {
  starting_phase_ = SCRIPT_READ_FINISHED;
}

void EmbeddedWorkerInstance::OnScriptLoaded() {
  if (!inflight_start_task_)
    return;

  // Renderer side has started to launch the worker thread.
  starting_phase_ = SCRIPT_LOADED;
  owner_version_->OnMainScriptLoaded();
  // |this| may be destroyed by the callback.
}

void EmbeddedWorkerInstance::OnWorkerVersionInstalled() {
  if (devtools_proxy_)
    devtools_proxy_->NotifyWorkerVersionInstalled();
}

void EmbeddedWorkerInstance::OnWorkerVersionDoomed() {
  if (devtools_proxy_)
    devtools_proxy_->NotifyWorkerVersionDoomed();
}

void EmbeddedWorkerInstance::OnScriptEvaluationStart() {
  if (!inflight_start_task_)
    return;

  starting_phase_ = SCRIPT_EVALUATION;
  for (auto& observer : listener_list_)
    observer.OnScriptEvaluationStart();
}

void EmbeddedWorkerInstance::OnStarted(
    blink::mojom::ServiceWorkerStartStatus start_status,
    int thread_id,
    mojom::EmbeddedWorkerStartTimingPtr start_timing) {
  if (!(start_timing->start_worker_received_time <=
            start_timing->script_evaluation_start_time &&
        start_timing->script_evaluation_start_time <=
            start_timing->script_evaluation_end_time)) {
    mojo::ReportBadMessage("EWI_BAD_START_TIMING");
    return;
  }

  if (!registry_->OnWorkerStarted(process_id(), embedded_worker_id_))
    return;
  // Stop was requested before OnStarted was sent back from the worker. Just
  // pretend startup didn't happen, so observers don't try to use the running
  // worker as it will stop soon.
  if (status_ == EmbeddedWorkerStatus::STOPPING)
    return;

  if (inflight_start_task_->is_installed() &&
      !inflight_start_task_->skip_recording_startup_time()) {
    ServiceWorkerMetrics::StartTimes times;
    times.local_start = inflight_start_task_->start_time();
    times.local_start_worker_sent =
        inflight_start_task_->start_worker_sent_time();
    times.remote_start_worker_received =
        start_timing->start_worker_received_time;
    times.remote_script_evaluation_start =
        start_timing->script_evaluation_start_time;
    times.remote_script_evaluation_end =
        start_timing->script_evaluation_end_time;
    times.local_end = base::TimeTicks::Now();

    ServiceWorkerMetrics::RecordStartWorkerTiming(times, start_situation_);
  }

  DCHECK_EQ(EmbeddedWorkerStatus::STARTING, status_);
  status_ = EmbeddedWorkerStatus::RUNNING;
  thread_id_ = thread_id;
  inflight_start_task_.reset();
  for (auto& observer : listener_list_) {
    observer.OnStarted(start_status);
    // |this| may be destroyed here. Fortunately we know there is only one
    // observer in production code.
  }
}

void EmbeddedWorkerInstance::OnStopped() {
  registry_->OnWorkerStopped(process_id(), embedded_worker_id_);

  EmbeddedWorkerStatus old_status = status_;
  ReleaseProcess();
  for (auto& observer : listener_list_)
    observer.OnStopped(old_status);
}

void EmbeddedWorkerInstance::Detach() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (status() == EmbeddedWorkerStatus::STOPPED)
    return;
  registry_->DetachWorker(process_id(), embedded_worker_id());

  EmbeddedWorkerStatus old_status = status_;
  ReleaseProcess();
  for (auto& observer : listener_list_)
    observer.OnDetached(old_status);
}

base::WeakPtr<EmbeddedWorkerInstance> EmbeddedWorkerInstance::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void EmbeddedWorkerInstance::OnReportException(
    const base::string16& error_message,
    int line_number,
    int column_number,
    const GURL& source_url) {
  for (auto& observer : listener_list_) {
    observer.OnReportException(error_message, line_number, column_number,
                               source_url);
  }
}

void EmbeddedWorkerInstance::OnReportConsoleMessage(
    int source_identifier,
    int message_level,
    const base::string16& message,
    int line_number,
    const GURL& source_url) {
  for (auto& observer : listener_list_) {
    observer.OnReportConsoleMessage(source_identifier, message_level, message,
                                    line_number, source_url);
  }
}

int EmbeddedWorkerInstance::process_id() const {
  if (process_handle_)
    return process_handle_->process_id();
  return ChildProcessHost::kInvalidUniqueID;
}

int EmbeddedWorkerInstance::worker_devtools_agent_route_id() const {
  if (devtools_proxy_)
    return devtools_proxy_->agent_route_id();
  return MSG_ROUTING_NONE;
}

void EmbeddedWorkerInstance::AddObserver(Listener* listener) {
  listener_list_.AddObserver(listener);
}

void EmbeddedWorkerInstance::RemoveObserver(Listener* listener) {
  listener_list_.RemoveObserver(listener);
}

void EmbeddedWorkerInstance::SetDevToolsAttached(bool attached) {
  devtools_attached_ = attached;
  if (!attached)
    return;
  if (inflight_start_task_)
    inflight_start_task_->set_skip_recording_startup_time();
  registry_->AbortLifetimeTracking(embedded_worker_id_);
}

void EmbeddedWorkerInstance::OnNetworkAccessedForScriptLoad() {
  starting_phase_ = SCRIPT_DOWNLOADING;
  network_accessed_for_script_ = true;
}

void EmbeddedWorkerInstance::ReleaseProcess() {
  // Abort an inflight start task.
  inflight_start_task_.reset();

  instance_host_binding_.Close();
  devtools_proxy_.reset();
  process_handle_.reset();
  status_ = EmbeddedWorkerStatus::STOPPED;
  starting_phase_ = NOT_STARTING;
  thread_id_ = kInvalidEmbeddedWorkerThreadId;
}

void EmbeddedWorkerInstance::OnSetupFailed(
    StatusCallback callback,
    blink::ServiceWorkerStatusCode status) {
  EmbeddedWorkerStatus old_status = status_;
  ReleaseProcess();
  base::WeakPtr<EmbeddedWorkerInstance> weak_this = weak_factory_.GetWeakPtr();
  std::move(callback).Run(status);
  if (weak_this && old_status != EmbeddedWorkerStatus::STOPPED) {
    for (auto& observer : weak_this->listener_list_)
      observer.OnStopped(old_status);
  }
}

void EmbeddedWorkerInstance::AddMessageToConsole(
    blink::WebConsoleMessage::Level level,
    const std::string& message) {
  if (process_id() == ChildProcessHost::kInvalidUniqueID)
    return;
  DCHECK(client_.is_bound());
  client_->AddMessageToConsole(level, message);
}

// static
std::string EmbeddedWorkerInstance::StatusToString(
    EmbeddedWorkerStatus status) {
  switch (status) {
    case EmbeddedWorkerStatus::STOPPED:
      return "STOPPED";
    case EmbeddedWorkerStatus::STARTING:
      return "STARTING";
    case EmbeddedWorkerStatus::RUNNING:
      return "RUNNING";
    case EmbeddedWorkerStatus::STOPPING:
      return "STOPPING";
  }
  NOTREACHED() << static_cast<int>(status);
  return std::string();
}

// static
std::string EmbeddedWorkerInstance::StartingPhaseToString(StartingPhase phase) {
  switch (phase) {
    case NOT_STARTING:
      return "Not in STARTING status";
    case ALLOCATING_PROCESS:
      return "Allocating process";
    case SENT_START_WORKER:
      return "Sent StartWorker message to renderer";
    case SCRIPT_DOWNLOADING:
      return "Script downloading";
    case SCRIPT_LOADED:
      return "Script loaded";
    case SCRIPT_READ_STARTED:
      return "Script read started";
    case SCRIPT_READ_FINISHED:
      return "Script read finished";
    case SCRIPT_STREAMING:
      return "Script streaming";
    case SCRIPT_EVALUATION:
      return "Script evaluation";
    case STARTING_PHASE_MAX_VALUE:
      NOTREACHED();
  }
  NOTREACHED() << phase;
  return std::string();
}

// static
void EmbeddedWorkerInstance::SetNetworkFactoryForTesting(
    const CreateNetworkFactoryCallback& create_network_factory_callback) {
  GetNetworkFactoryCallbackForTest() = create_network_factory_callback;
}

}  // namespace content
