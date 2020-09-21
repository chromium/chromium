// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/embedded_worker_instance.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/feature_list.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/optional.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/bad_message.h"
#include "content/browser/data_url_loader_factory.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/devtools/service_worker_devtools_manager.h"
#include "content/browser/net/cross_origin_embedder_policy_reporter.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/service_worker/embedded_worker_status.h"
#include "content/browser/service_worker/service_worker_consts.h"
#include "content/browser/service_worker/service_worker_content_settings_proxy_impl.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_host.h"
#include "content/browser/service_worker/service_worker_script_loader_factory.h"
#include "content/browser/url_loader_factory_getter.h"
#include "content/browser/url_loader_factory_params_helper.h"
#include "content/common/content_switches_internal.h"
#include "content/common/url_schemes.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/child_process_host.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "ipc/ipc_message.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/base/isolation_info.h"
#include "net/cookies/site_for_cookies.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/loader/url_loader_factory_bundle.mojom.h"
#include "third_party/blink/public/mojom/renderer_preference_watcher.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_object.mojom.h"
#include "url/gurl.h"

// TODO(crbug.com/824858): Much of this file, which dealt with thread hops
// between UI and IO, can likely be simplified when the service worker core
// thread moves to the UI thread.

namespace content {

namespace {

// Used for tracing.
constexpr char kEmbeddedWorkerInstanceScope[] = "EmbeddedWorkerInstance";

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
    mojo::PendingRemote<blink::mojom::DevToolsAgent> agent_remote,
    mojo::PendingReceiver<blink::mojom::DevToolsAgentHost> host_receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ServiceWorkerDevToolsManager::GetInstance()->WorkerReadyForInspection(
      worker_process_id, worker_route_id, std::move(agent_remote),
      std::move(host_receiver));
}

void NotifyUpdateCrossOriginEmbedderPolicyOnUI(
    int worker_process_id,
    int worker_route_id,
    network::CrossOriginEmbedderPolicy cross_origin_embedder_policy,
    mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
        coep_reporter) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ServiceWorkerDevToolsManager::GetInstance()->UpdateCrossOriginEmbedderPolicy(
      worker_process_id, worker_route_id,
      std::move(cross_origin_embedder_policy), std::move(coep_reporter));
}

void NotifyWorkerStoppedOnUI(int worker_process_id, int worker_route_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ServiceWorkerDevToolsManager::GetInstance()->WorkerStopped(worker_process_id,
                                                             worker_route_id);
}

void NotifyWorkerVersionInstalledOnUI(int worker_process_id,
                                      int worker_route_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ServiceWorkerDevToolsManager::GetInstance()->WorkerVersionInstalled(
      worker_process_id, worker_route_id);
}

void NotifyWorkerVersionDoomedOnUI(
    int worker_process_id,
    int worker_route_id,
    scoped_refptr<ServiceWorkerContextWrapper> context_wrapper,
    int64_t version_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ServiceWorkerDevToolsManager::GetInstance()->WorkerVersionDoomed(
      worker_process_id, worker_route_id, context_wrapper, version_id);
}

using CreateFactoryBundlesOnUICallback = base::OnceCallback<void(
    std::unique_ptr<blink::PendingURLLoaderFactoryBundle> script_bundle,
    std::unique_ptr<blink::PendingURLLoaderFactoryBundle> subresouce_bundle,
    mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
        coep_reporter,
    mojo::PendingReceiver<blink::mojom::ReportingObserver>
        reporting_observer_receiver)>;
void CreateFactoryBundlesOnUI(int process_id,
                              int routing_id,
                              const GURL& script_url,
                              base::Optional<network::CrossOriginEmbedderPolicy>
                                  cross_origin_embedder_policy,
                              CreateFactoryBundlesOnUICallback callback) {
  mojo::PendingReceiver<blink::mojom::ReportingObserver>
      reporting_observer_receiver;
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto* rph = RenderProcessHost::FromID(process_id);
  if (!rph) {
    // Return nullptr because we can't create a factory bundle because of
    // missing renderer.
    ServiceWorkerContextWrapper::RunOrPostTaskOnCoreThread(
        FROM_HERE, base::BindOnce(std::move(callback), nullptr, nullptr,
                                  mojo::NullRemote(),
                                  std::move(reporting_observer_receiver)));
    return;
  }

  // Create mojo::Remote which is connected to and owns a COEP reporter.
  mojo::Remote<network::mojom::CrossOriginEmbedderPolicyReporter> coep_reporter;
  mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
      coep_reporter_for_devtools;
  mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
      coep_reporter_for_scripts;
  mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
      coep_reporter_for_subresources;

  // |cross_origin_embedder_policy| is nullopt in some unittests.
  // TODO(shimazu): Set COEP in those tests.
  if (cross_origin_embedder_policy) {
    mojo::PendingRemote<blink::mojom::ReportingObserver>
        reporting_observer_remote;
    reporting_observer_receiver =
        reporting_observer_remote.InitWithNewPipeAndPassReceiver();
    auto reporter = std::make_unique<CrossOriginEmbedderPolicyReporter>(
        rph->GetStoragePartition(), script_url,
        cross_origin_embedder_policy->reporting_endpoint,
        cross_origin_embedder_policy->report_only_reporting_endpoint);
    reporter->BindObserver(std::move(reporting_observer_remote));
    mojo::MakeSelfOwnedReceiver(std::move(reporter),
                                coep_reporter.BindNewPipeAndPassReceiver());
    coep_reporter->Clone(
        coep_reporter_for_devtools.InitWithNewPipeAndPassReceiver());
    coep_reporter->Clone(
        coep_reporter_for_scripts.InitWithNewPipeAndPassReceiver());
    coep_reporter->Clone(
        coep_reporter_for_subresources.InitWithNewPipeAndPassReceiver());

    NotifyUpdateCrossOriginEmbedderPolicyOnUI(
        process_id, routing_id, cross_origin_embedder_policy.value(),
        std::move(coep_reporter_for_devtools));
  }

  const url::Origin origin = url::Origin::Create(script_url);
  auto script_bundle = EmbeddedWorkerInstance::CreateFactoryBundleOnUI(
      rph, routing_id, origin, cross_origin_embedder_policy,
      std::move(coep_reporter_for_scripts),
      ContentBrowserClient::URLLoaderFactoryType::kServiceWorkerScript);
  auto subresource_bundle = EmbeddedWorkerInstance::CreateFactoryBundleOnUI(
      rph, routing_id, origin, cross_origin_embedder_policy,
      std::move(coep_reporter_for_subresources),
      ContentBrowserClient::URLLoaderFactoryType::kServiceWorkerSubResource);
  ServiceWorkerContextWrapper::RunOrPostTaskOnCoreThread(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(script_bundle),
                                std::move(subresource_bundle),
                                coep_reporter ? coep_reporter.Unbind()
                                              : mojo::NullRemote(),
                                std::move(reporting_observer_receiver)));
}

using SetupProcessCallback = base::OnceCallback<void(
    blink::ServiceWorkerStatusCode,
    blink::mojom::EmbeddedWorkerStartParamsPtr,
    std::unique_ptr<ServiceWorkerProcessManager::AllocatedProcessInfo>,
    std::unique_ptr<EmbeddedWorkerInstance::DevToolsProxy>,
    std::unique_ptr<
        blink::PendingURLLoaderFactoryBundle> /* factory_bundle_for_new_scripts
                                               */
    ,
    std::unique_ptr<
        blink::PendingURLLoaderFactoryBundle> /* factory_bundle_for_renderer */,
    mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>,
    mojo::PendingReceiver<blink::mojom::ReportingObserver>,
    const base::Optional<base::TimeDelta>& thread_hop_time,
    const base::Optional<base::Time>& ui_post_time)>;

// Allocates a renderer process for starting a worker and does setup like
// registering with DevTools. Called on the UI thread. Calls |callback| on the
// core thread. |context| and |weak_context| are only for passing to DevTools
// and must not be dereferenced here on the UI thread.
//
// This also sets up two URLLoaderFactoryBundles, one for
// ServiceWorkerScriptLoaderFactory and the other is for passing to the
// renderer. |cross_origin_embedder_policy| is respected to make these bundles.
// These bundles include factories for non-network URLs like chrome-extension://
// as needed.
void SetupOnUIThread(
    int embedded_worker_id,
    base::WeakPtr<ServiceWorkerProcessManager> process_manager,
    bool can_use_existing_process,
    const base::Optional<network::CrossOriginEmbedderPolicy>&
        cross_origin_embedder_policy,
    blink::mojom::EmbeddedWorkerStartParamsPtr params,
    mojo::PendingReceiver<blink::mojom::EmbeddedWorkerInstanceClient> receiver,
    scoped_refptr<ServiceWorkerContextWrapper> context_wrapper,
    const base::Optional<base::Time>& io_post_time,
    SetupProcessCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::Optional<base::TimeDelta> thread_hop_time;
  if (!ServiceWorkerContext::IsServiceWorkerOnUIEnabled())
    thread_hop_time = base::Time::Now() - io_post_time.value();

  auto process_info =
      std::make_unique<ServiceWorkerProcessManager::AllocatedProcessInfo>();
  std::unique_ptr<EmbeddedWorkerInstance::DevToolsProxy> devtools_proxy;
  std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
      factory_bundle_for_new_scripts;
  std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
      factory_bundle_for_renderer;
  mojo::PendingReceiver<blink::mojom::ReportingObserver>
      reporting_observer_receiver;

  if (!process_manager) {
    base::Optional<base::Time> ui_post_time;
    if (!ServiceWorkerContext::IsServiceWorkerOnUIEnabled())
      ui_post_time = base::Time::Now();

    ServiceWorkerContextWrapper::RunOrPostTaskOnCoreThread(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  blink::ServiceWorkerStatusCode::kErrorAbort,
                                  std::move(params), std::move(process_info),
                                  std::move(devtools_proxy),
                                  std::move(factory_bundle_for_new_scripts),
                                  std::move(factory_bundle_for_renderer),
                                  /*coep_reporter=*/mojo::NullRemote(),
                                  std::move(reporting_observer_receiver),
                                  thread_hop_time, ui_post_time));
    return;
  }

  // Get a process.
  blink::ServiceWorkerStatusCode status =
      process_manager->AllocateWorkerProcess(
          embedded_worker_id, params->script_url, can_use_existing_process,
          process_info.get());
  if (status != blink::ServiceWorkerStatusCode::kOk) {
    base::Optional<base::Time> ui_post_time;
    if (!ServiceWorkerContext::IsServiceWorkerOnUIEnabled())
      ui_post_time = base::Time::Now();

    ServiceWorkerContextWrapper::RunOrPostTaskOnCoreThread(
        FROM_HERE,
        base::BindOnce(std::move(callback), status, std::move(params),
                       std::move(process_info), std::move(devtools_proxy),
                       std::move(factory_bundle_for_new_scripts),
                       std::move(factory_bundle_for_renderer),
                       /*coep_reporter=*/mojo::NullRemote(),
                       std::move(reporting_observer_receiver), thread_hop_time,
                       ui_post_time));
    return;
  }
  const int process_id = process_info->process_id;
  RenderProcessHost* rph = RenderProcessHost::FromID(process_id);
  // TODO(falken): This CHECK should no longer fail, so turn to a DCHECK it if
  // crash reports agree. Consider also checking for
  // rph->IsInitializedAndNotDead().
  CHECK(rph);

  // Bind |receiver|, which is attached to |EmbeddedWorkerInstance::client_|, to
  // the process. If the process dies, |client_|'s connection error callback
  // will be called on the core thread.
  if (receiver.is_valid())
    rph->BindReceiver(std::move(receiver));

  // Create COEP reporter if COEP value is already available (= this worker is
  // not a worker which is going to be newly registered).
  mojo::Remote<network::mojom::CrossOriginEmbedderPolicyReporter> coep_reporter;
  mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
      coep_reporter_for_devtools;
  mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
      coep_reporter_for_scripts;
  mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
      coep_reporter_for_subresources;
  if (cross_origin_embedder_policy) {
    mojo::PendingRemote<blink::mojom::ReportingObserver>
        reporting_observer_remote;
    reporting_observer_receiver =
        reporting_observer_remote.InitWithNewPipeAndPassReceiver();
    auto reporter = std::make_unique<CrossOriginEmbedderPolicyReporter>(
        rph->GetStoragePartition(), params->script_url,
        cross_origin_embedder_policy->reporting_endpoint,
        cross_origin_embedder_policy->report_only_reporting_endpoint);
    reporter->BindObserver(std::move(reporting_observer_remote));
    mojo::MakeSelfOwnedReceiver(std::move(reporter),
                                coep_reporter.BindNewPipeAndPassReceiver());
    coep_reporter->Clone(
        coep_reporter_for_devtools.InitWithNewPipeAndPassReceiver());
    coep_reporter->Clone(
        coep_reporter_for_scripts.InitWithNewPipeAndPassReceiver());
    coep_reporter->Clone(
        coep_reporter_for_subresources.InitWithNewPipeAndPassReceiver());
  }
  // Register to DevTools and update params accordingly.
  const int routing_id = rph->GetNextRoutingID();
  ServiceWorkerDevToolsManager::GetInstance()->WorkerStarting(
      process_id, routing_id, std::move(context_wrapper),
      params->service_worker_version_id, params->script_url, params->scope,
      params->is_installed, cross_origin_embedder_policy,
      std::move(coep_reporter_for_devtools), &params->devtools_worker_token,
      &params->wait_for_debugger);
  params->service_worker_route_id = routing_id;
  // Create DevToolsProxy here to ensure that the WorkerCreated() call is
  // balanced by DevToolsProxy's destructor calling WorkerStopped().
  devtools_proxy = std::make_unique<EmbeddedWorkerInstance::DevToolsProxy>(
      process_id, routing_id);

  // Create factory bundles for this worker to do loading. These bundles don't
  // support reconnection to the network service, see below comments.
  const url::Origin origin = url::Origin::Create(params->script_url);

  // The bundle for new scripts is passed to ServiceWorkerScriptLoaderFactory
  // and used to request non-installed service worker scripts. It's only needed
  // for non-installed workers. It's OK to not support reconnection to the
  // network service because it can only used until the service worker reaches
  // the 'installed' state.
  if (!params->is_installed) {
    factory_bundle_for_new_scripts =
        EmbeddedWorkerInstance::CreateFactoryBundleOnUI(
            rph, routing_id, origin, cross_origin_embedder_policy,
            std::move(coep_reporter_for_scripts),
            ContentBrowserClient::URLLoaderFactoryType::kServiceWorkerScript);
  }

  // The bundle for the renderer is passed to the service worker, and
  // used for subresource loading from the service worker (i.e., fetch()).
  // It's OK to not support reconnection to the network service because the
  // service worker terminates itself when the connection breaks, so a new
  // instance can be started.
  factory_bundle_for_renderer = EmbeddedWorkerInstance::CreateFactoryBundleOnUI(
      rph, routing_id, origin, cross_origin_embedder_policy,
      std::move(coep_reporter_for_subresources),
      ContentBrowserClient::URLLoaderFactoryType::kServiceWorkerSubResource);

  // TODO(crbug.com/862854): Support changes to
  // blink::mojom::RendererPreferences while the worker is running.
  DCHECK(process_manager->browser_context() || process_manager->IsShutdown());
  params->renderer_preferences = blink::mojom::RendererPreferences::New();
  GetContentClient()->browser()->UpdateRendererPreferencesForWorker(
      process_manager->browser_context(), params->renderer_preferences.get());

  // Create a RendererPreferenceWatcher to observe updates in the preferences.
  mojo::PendingRemote<blink::mojom::RendererPreferenceWatcher> watcher_remote;
  params->preference_watcher_receiver =
      watcher_remote.InitWithNewPipeAndPassReceiver();
  GetContentClient()->browser()->RegisterRendererPreferenceWatcher(
      process_manager->browser_context(), std::move(watcher_remote));

  // Continue to OnSetupCompleted on the core thread.
  base::Optional<base::Time> ui_post_time;
  if (!ServiceWorkerContext::IsServiceWorkerOnUIEnabled())
    ui_post_time = base::Time::Now();
  RunOrPostTaskOnThread(
      FROM_HERE, ServiceWorkerContext::GetCoreThreadId(),
      base::BindOnce(
          std::move(callback), status, std::move(params),
          std::move(process_info), std::move(devtools_proxy),
          std::move(factory_bundle_for_new_scripts),
          std::move(factory_bundle_for_renderer),
          coep_reporter ? coep_reporter.Unbind() : mojo::NullRemote(),
          std::move(reporting_observer_receiver), thread_hop_time,
          ui_post_time));
}

bool HasSentStartWorker(EmbeddedWorkerInstance::StartingPhase phase) {
  switch (phase) {
    case EmbeddedWorkerInstance::NOT_STARTING:
    case EmbeddedWorkerInstance::ALLOCATING_PROCESS:
      return false;
    case EmbeddedWorkerInstance::SENT_START_WORKER:
    case EmbeddedWorkerInstance::SCRIPT_DOWNLOADING:
    case EmbeddedWorkerInstance::SCRIPT_STREAMING:
    case EmbeddedWorkerInstance::SCRIPT_LOADED:
    case EmbeddedWorkerInstance::SCRIPT_EVALUATION:
      return true;
    case EmbeddedWorkerInstance::STARTING_PHASE_MAX_VALUE:
      NOTREACHED();
  }
  return false;
}

void NotifyForegroundServiceWorkerOnUIThread(bool added, int process_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  RenderProcessHost* rph = RenderProcessHost::FromID(process_id);
  if (!rph)
    return;

  if (added)
    rph->OnForegroundServiceWorkerAdded();
  else
    rph->OnForegroundServiceWorkerRemoved();
}

void BindCacheStorageOnUIThread(
    int process_id,
    url::Origin origin,
    network::CrossOriginEmbedderPolicy cross_origin_embedder_policy,
    mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
        coep_reporter,
    mojo::PendingReceiver<blink::mojom::CacheStorage> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto* rph = RenderProcessHost::FromID(process_id);
  if (!rph)
    return;

  rph->BindCacheStorage(cross_origin_embedder_policy, std::move(coep_reporter),
                        origin, std::move(receiver));
}

}  // namespace

// Created on the UI thread when the worker version is allcated a render process
// and then moved to the core thread. It is destroyed when the worker stops.
// Proxies notifications to DevToolsManager that lives on UI thread.
// Owned by EmbeddedWorkerInstance.
class EmbeddedWorkerInstance::DevToolsProxy {
 public:
  DevToolsProxy(int process_id, int agent_route_id)
      : process_id_(process_id),
        agent_route_id_(agent_route_id),
        ui_task_runner_(GetUIThreadTaskRunner({})) {}

  ~DevToolsProxy() {
    DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
    if (ServiceWorkerContext::IsServiceWorkerOnUIEnabled()) {
      NotifyWorkerStoppedOnUI(process_id_, agent_route_id_);
    } else {
      ui_task_runner_->PostTask(
          FROM_HERE, base::BindOnce(NotifyWorkerStoppedOnUI, process_id_,
                                    agent_route_id_));
    }
  }

  void NotifyWorkerReadyForInspection(
      mojo::PendingRemote<blink::mojom::DevToolsAgent> agent_remote,
      mojo::PendingReceiver<blink::mojom::DevToolsAgentHost> host_receiver) {
    DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
    if (ServiceWorkerContext::IsServiceWorkerOnUIEnabled()) {
      NotifyWorkerReadyForInspectionOnUI(process_id_, agent_route_id_,
                                         std::move(agent_remote),
                                         std::move(host_receiver));
    } else {
      ui_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(NotifyWorkerReadyForInspectionOnUI, process_id_,
                         agent_route_id_, std::move(agent_remote),
                         std::move(host_receiver)));
    }
  }

  void NotifyWorkerVersionInstalled() {
    DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
    if (ServiceWorkerContext::IsServiceWorkerOnUIEnabled()) {
      NotifyWorkerVersionInstalledOnUI(process_id_, agent_route_id_);
    } else {
      ui_task_runner_->PostTask(FROM_HERE,
                                base::BindOnce(NotifyWorkerVersionInstalledOnUI,
                                               process_id_, agent_route_id_));
    }
  }

  bool ShouldNotifyWorkerStopIgnored() const {
    return !worker_stop_ignored_notified_;
  }

  void WorkerStopIgnoredNotified() { worker_stop_ignored_notified_ = true; }

  int agent_route_id() const { return agent_route_id_; }

 private:
  const int process_id_;
  const int agent_route_id_;
  const scoped_refptr<base::TaskRunner> ui_task_runner_;
  bool worker_stop_ignored_notified_ = false;

  DISALLOW_COPY_AND_ASSIGN(DevToolsProxy);
};

// Tracks how long a service worker runs for, for UMA purposes.
class EmbeddedWorkerInstance::ScopedLifetimeTracker {
 public:
  ScopedLifetimeTracker() : start_ticks_(base::TimeTicks::Now()) {}

  ~ScopedLifetimeTracker() {
    if (!start_ticks_.is_null()) {
      ServiceWorkerMetrics::RecordRuntime(base::TimeTicks::Now() -
                                          start_ticks_);
    }
  }

  // Called when DevTools was attached to the worker. Ensures no metric is
  // recorded for this worker.
  void Abort() { start_ticks_ = base::TimeTicks(); }

 private:
  base::TimeTicks start_ticks_;

  DISALLOW_COPY_AND_ASSIGN(ScopedLifetimeTracker);
};

// A handle for a renderer process managed by ServiceWorkerProcessManager on the
// UI thread. Lives on the core thread.
class EmbeddedWorkerInstance::WorkerProcessHandle {
 public:
  WorkerProcessHandle(
      const base::WeakPtr<ServiceWorkerProcessManager>& process_manager,
      int embedded_worker_id,
      int process_id,
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner)
      : process_manager_(process_manager),
        embedded_worker_id_(embedded_worker_id),
        process_id_(process_id),
        ui_task_runner_(std::move(ui_task_runner)) {
    DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
    DCHECK_NE(ChildProcessHost::kInvalidUniqueID, process_id_);
  }

  ~WorkerProcessHandle() {
    DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
    if (ServiceWorkerContext::IsServiceWorkerOnUIEnabled()) {
      process_manager_->ReleaseWorkerProcess(embedded_worker_id_);
    } else {
      ui_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&ServiceWorkerProcessManager::ReleaseWorkerProcess,
                         process_manager_, embedded_worker_id_));
    }
  }

  int process_id() const { return process_id_; }

 private:
  // Can be dereferenced on the UI thread only.
  base::WeakPtr<ServiceWorkerProcessManager> process_manager_;

  const int embedded_worker_id_;
  const int process_id_;
  const scoped_refptr<base::SequencedTaskRunner> ui_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(WorkerProcessHandle);
};

// A task to allocate a worker process and to send a start worker message. This
// is created on EmbeddedWorkerInstance::Start(), owned by the instance and
// destroyed on EmbeddedWorkerInstance::OnScriptEvaluated().
// We can abort starting worker by destroying this task anytime during the
// sequence.
// Lives on the core thread.
class EmbeddedWorkerInstance::StartTask {
 public:
  enum class ProcessAllocationState { NOT_ALLOCATED, ALLOCATING, ALLOCATED };

  StartTask(EmbeddedWorkerInstance* instance,
            const GURL& script_url,
            mojo::PendingReceiver<blink::mojom::EmbeddedWorkerInstanceClient>
                receiver,
            base::TimeTicks start_time)
      : instance_(instance),
        receiver_(std::move(receiver)),
        state_(ProcessAllocationState::NOT_ALLOCATED),
        is_installed_(false),
        started_during_browser_startup_(false),
        skip_recording_startup_time_(instance_->devtools_attached()),
        start_time_(start_time) {
    DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
    TRACE_EVENT_WITH_FLOW1(
        "ServiceWorker", "EmbeddedWorkerInstance::StartTask::StartTask",
        TRACE_ID_WITH_SCOPE(kEmbeddedWorkerInstanceScope,
                            instance_->embedded_worker_id()),
        TRACE_EVENT_FLAG_FLOW_OUT, "Script", script_url.spec());
  }

  ~StartTask() {
    DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
    TRACE_EVENT_WITH_FLOW0("ServiceWorker",
                           "EmbeddedWorkerInstance::StartTask::~StartTask",
                           TRACE_ID_WITH_SCOPE(kEmbeddedWorkerInstanceScope,
                                               instance_->embedded_worker_id()),
                           TRACE_EVENT_FLAG_FLOW_IN);
    if (!instance_->context_)
      return;

    switch (state_) {
      case ProcessAllocationState::NOT_ALLOCATED:
        // Not necessary to release a process.
        break;
      case ProcessAllocationState::ALLOCATING:
        // Abort half-baked process allocation on the UI thread.
        instance_->ui_task_runner_->PostTask(
            FROM_HERE,
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
  base::TimeDelta thread_hop_time() const { return thread_hop_time_; }

  void set_skip_recording_startup_time() {
    skip_recording_startup_time_ = true;
  }
  bool skip_recording_startup_time() const {
    return skip_recording_startup_time_;
  }

  void Start(blink::mojom::EmbeddedWorkerStartParamsPtr params,
             const base::Optional<network::CrossOriginEmbedderPolicy>&
                 cross_origin_embedder_policy,
             StatusCallback sent_start_callback) {
    DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
    DCHECK(instance_->context_);
    TRACE_EVENT_WITH_FLOW0(
        "ServiceWorker", "EmbeddedWorkerInstance::StartTask::Start",
        TRACE_ID_WITH_SCOPE(kEmbeddedWorkerInstanceScope,
                            instance_->embedded_worker_id()),
        TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);

    base::WeakPtr<ServiceWorkerContextCore> context = instance_->context_;
    state_ = ProcessAllocationState::ALLOCATING;
    sent_start_callback_ = std::move(sent_start_callback);
    is_installed_ = params->is_installed;

    if (!GetContentClient()->browser()->IsBrowserStartupComplete())
      started_during_browser_startup_ = true;

    bool can_use_existing_process =
        context->GetVersionFailureCount(params->service_worker_version_id) <
        kMaxSameProcessFailureCount;
    base::WeakPtr<ServiceWorkerProcessManager> process_manager =
        context->process_manager()->AsWeakPtr();

    // Perform process allocation and setup on the UI thread. We will continue
    // on the core thread in StartTask::OnSetupCompleted().
    if (ServiceWorkerContext::IsServiceWorkerOnUIEnabled()) {
      SetupOnUIThread(
          instance_->embedded_worker_id(), process_manager,
          can_use_existing_process, cross_origin_embedder_policy,
          std::move(params), std::move(receiver_),
          base::WrapRefCounted(context->wrapper()), base::nullopt,
          base::BindOnce(&StartTask::OnSetupCompleted,
                         weak_factory_.GetWeakPtr(), process_manager));
    } else {
      instance_->ui_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(
              &SetupOnUIThread, instance_->embedded_worker_id(),
              process_manager, can_use_existing_process,
              cross_origin_embedder_policy, std::move(params),
              std::move(receiver_), base::WrapRefCounted(context->wrapper()),
              base::make_optional<base::Time>(base::Time::Now()),
              base::BindOnce(&StartTask::OnSetupCompleted,
                             weak_factory_.GetWeakPtr(), process_manager)));
    }
  }

  bool is_installed() const { return is_installed_; }

 private:
  void OnSetupCompleted(
      base::WeakPtr<ServiceWorkerProcessManager> process_manager,
      blink::ServiceWorkerStatusCode status,
      blink::mojom::EmbeddedWorkerStartParamsPtr params,
      std::unique_ptr<ServiceWorkerProcessManager::AllocatedProcessInfo>
          process_info,
      std::unique_ptr<EmbeddedWorkerInstance::DevToolsProxy> devtools_proxy,
      std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
          factory_bundle_for_new_scripts,
      std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
          factory_bundle_for_renderer,
      mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
          coep_reporter,
      mojo::PendingReceiver<blink::mojom::ReportingObserver>
          reporting_observer_receiver,
      const base::Optional<base::TimeDelta>& thread_hop_time,
      const base::Optional<base::Time>& ui_post_time) {
    DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

    if (reporting_observer_receiver) {
      instance_->owner_version_->set_reporting_observer_receiver(
          std::move(reporting_observer_receiver));
    }

    if (!ServiceWorkerContext::IsServiceWorkerOnUIEnabled())
      thread_hop_time_ =
          thread_hop_time.value() + (base::Time::Now() - ui_post_time.value());

    std::unique_ptr<WorkerProcessHandle> process_handle;
    if (status == blink::ServiceWorkerStatusCode::kOk) {
      // If we allocated a process, WorkerProcessHandle has to be created before
      // returning to ensure the process is eventually released.
      process_handle = std::make_unique<WorkerProcessHandle>(
          process_manager, instance_->embedded_worker_id(),
          process_info->process_id, instance_->ui_task_runner_);

      if (!instance_->context_)
        status = blink::ServiceWorkerStatusCode::kErrorAbort;
    }

    if (status != blink::ServiceWorkerStatusCode::kOk) {
      TRACE_EVENT_WITH_FLOW1(
          "ServiceWorker",
          "EmbeddedWorkerInstance::StartTask::OnSetupCompleted",
          TRACE_ID_WITH_SCOPE(kEmbeddedWorkerInstanceScope,
                              instance_->embedded_worker_id()),
          TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "Error",
          blink::ServiceWorkerStatusToString(status));
      instance_->OnSetupFailed(std::move(sent_start_callback_), status);
      // |this| may be destroyed.
      return;
    }

    ServiceWorkerMetrics::StartSituation start_situation =
        process_info->start_situation;
    TRACE_EVENT_WITH_FLOW1(
        "ServiceWorker", "EmbeddedWorkerInstance::StartTask::OnSetupCompleted",
        TRACE_ID_WITH_SCOPE(kEmbeddedWorkerInstanceScope,
                            instance_->embedded_worker_id()),
        TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "StartSituation",
        ServiceWorkerMetrics::StartSituationToString(start_situation));

    if (started_during_browser_startup_)
      start_situation = ServiceWorkerMetrics::StartSituation::DURING_STARTUP;

    // Notify the instance that a process is allocated.
    state_ = ProcessAllocationState::ALLOCATED;
    instance_->OnProcessAllocated(std::move(process_handle), start_situation);

    // Notify the instance that it is registered to the DevTools manager.
    instance_->OnRegisteredToDevToolsManager(std::move(devtools_proxy),
                                             params->wait_for_debugger);

    // Send the factory bundle for subresource loading from the service worker
    // (i.e. fetch()).
    DCHECK(factory_bundle_for_renderer);
    params->subresource_loader_factories =
        std::move(factory_bundle_for_renderer);

    // Build the URLLoaderFactory for loading new scripts, it's only needed if
    // this is a non-installed service worker.
    DCHECK(factory_bundle_for_new_scripts || is_installed_);
    if (factory_bundle_for_new_scripts) {
      params->provider_info->script_loader_factory_remote =
          instance_->MakeScriptLoaderFactoryRemote(
              std::move(factory_bundle_for_new_scripts));
    }

    // Bind COEP reporter created on the UI thread, which has the onwership of
    // the instance. The |coep_reporter| might be null when the COEP value is
    // not known because the main script has not been loaded yet. In that case,
    // COEP reporter will be bound after the main script is loaded.
    if (coep_reporter) {
      instance_->coep_reporter_.Bind(std::move(coep_reporter));
    }

    // Create cache storage now as an optimization, so the service worker can
    // use the Cache Storage API immediately on startup.
    if (base::FeatureList::IsEnabled(
            blink::features::kEagerCacheStorageSetupForServiceWorkers)) {
      instance_->BindCacheStorage(params->provider_info->cache_storage
                                      .InitWithNewPipeAndPassReceiver());
    }

    instance_->SendStartWorker(std::move(params));
    std::move(sent_start_callback_).Run(blink::ServiceWorkerStatusCode::kOk);

    // |this|'s work is done here, but |instance_| still uses its state until
    // startup is complete.
  }

  // |instance_| must outlive |this|.
  EmbeddedWorkerInstance* instance_;

  // Ownership is transferred by a PostTask() call after process allocation.
  mojo::PendingReceiver<blink::mojom::EmbeddedWorkerInstanceClient> receiver_;

  StatusCallback sent_start_callback_;
  ProcessAllocationState state_;

  // Used for UMA.
  bool is_installed_;
  bool started_during_browser_startup_;
  bool skip_recording_startup_time_;
  base::TimeTicks start_time_;
  base::TimeTicks start_worker_sent_time_;
  base::TimeDelta thread_hop_time_;

  base::WeakPtrFactory<StartTask> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(StartTask);
};

EmbeddedWorkerInstance::~EmbeddedWorkerInstance() {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  ReleaseProcess();
}

void EmbeddedWorkerInstance::Start(
    blink::mojom::EmbeddedWorkerStartParamsPtr params,
    StatusCallback callback) {
  DCHECK(context_);
  restart_count_++;
  DCHECK_EQ(EmbeddedWorkerStatus::STOPPED, status_);

  DCHECK_NE(blink::mojom::kInvalidServiceWorkerVersionId,
            params->service_worker_version_id);

  auto start_time = base::TimeTicks::Now();
  status_ = EmbeddedWorkerStatus::STARTING;
  starting_phase_ = ALLOCATING_PROCESS;
  network_accessed_for_script_ = false;
  token_ = blink::ServiceWorkerToken();

  for (auto& observer : listener_list_)
    observer.OnStarting();

  // service_worker_route_id will be set later in SetupOnUIThread
  params->service_worker_route_id = MSG_ROUTING_NONE;
  params->wait_for_debugger = false;
  params->subresource_loader_updater =
      subresource_loader_updater_.BindNewPipeAndPassReceiver();
  params->service_worker_token = token_.value();

  // TODO(https://crbug.com/978694): Consider a reset flow since new mojo types
  // check is_bound strictly.
  client_.reset();

  mojo::PendingReceiver<blink::mojom::EmbeddedWorkerInstanceClient> receiver =
      client_.BindNewPipeAndPassReceiver();
  client_.set_disconnect_handler(
      base::BindOnce(&EmbeddedWorkerInstance::Detach, base::Unretained(this)));
  inflight_start_task_.reset(
      new StartTask(this, params->script_url, std::move(receiver), start_time));
  inflight_start_task_->Start(std::move(params),
                              owner_version_->cross_origin_embedder_policy(),
                              std::move(callback));
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
        owner_version_->MaybeReportConsoleMessageToInternals(
            blink::mojom::ConsoleMessageLevel::kVerbose,
            kServiceWorkerTerminationCanceledMesage);
        devtools_proxy_->WorkerStopIgnoredNotified();
      }
    }
    return;
  }
  Stop();
}

EmbeddedWorkerInstance::EmbeddedWorkerInstance(
    ServiceWorkerVersion* owner_version)
    : context_(owner_version->context()),
      owner_version_(owner_version),
      embedded_worker_id_(context_->GetNextEmbeddedWorkerId()),
      status_(EmbeddedWorkerStatus::STOPPED),
      starting_phase_(NOT_STARTING),
      restart_count_(0),
      thread_id_(ServiceWorkerConsts::kInvalidEmbeddedWorkerThreadId),
      devtools_attached_(false),
      network_accessed_for_script_(false),
      foreground_notified_(false),
      ui_task_runner_(GetUIThreadTaskRunner({})) {
  DCHECK(owner_version_);
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  DCHECK(context_);
}

void EmbeddedWorkerInstance::OnProcessAllocated(
    std::unique_ptr<WorkerProcessHandle> handle,
    ServiceWorkerMetrics::StartSituation start_situation) {
  DCHECK_EQ(EmbeddedWorkerStatus::STARTING, status_);
  DCHECK(!process_handle_);

  process_handle_ = std::move(handle);

  UpdateForegroundPriority();

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
    blink::mojom::EmbeddedWorkerStartParamsPtr params) {
  DCHECK(context_);
  DCHECK(params->service_worker_receiver.is_valid());
  DCHECK(params->controller_receiver.is_valid());
  DCHECK(!instance_host_receiver_.is_bound());

  instance_host_receiver_.Bind(
      params->instance_host.InitWithNewEndpointAndPassReceiver());

  content_settings_ =
      base::SequenceBound<ServiceWorkerContentSettingsProxyImpl>(
          GetUIThreadTaskRunner({}), params->script_url,
          scoped_refptr<ServiceWorkerContextWrapper>(context_->wrapper()),
          params->content_settings_proxy.InitWithNewPipeAndPassReceiver());

  const bool is_script_streaming = !params->installed_scripts_info.is_null();
  inflight_start_task_->set_start_worker_sent_time(base::TimeTicks::Now());

  // The host must be alive as long as |params->provider_info| is alive.
  owner_version_->worker_host()->CompleteStartWorkerPreparation(
      process_id(), params->provider_info->browser_interface_broker
                        .InitWithNewPipeAndPassReceiver());

  // TODO(bashi): Always pass a valid outside fetch client settings object.
  // See crbug.com/937177.
  if (!params->outside_fetch_client_settings_object) {
    params->outside_fetch_client_settings_object =
        blink::mojom::FetchClientSettingsObject::New(
            network::mojom::ReferrerPolicy::kDefault,
            /*outgoing_referrer=*/params->script_url,
            blink::mojom::InsecureRequestsPolicy::kDoNotUpgrade);
  }

  client_->StartWorker(std::move(params));

  starting_phase_ = is_script_streaming ? SCRIPT_STREAMING : SENT_START_WORKER;
  for (auto& observer : listener_list_)
    observer.OnStartWorkerMessageSent();
}

void EmbeddedWorkerInstance::RequestTermination(
    RequestTerminationCallback callback) {
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

void EmbeddedWorkerInstance::OnReadyForInspection(
    mojo::PendingRemote<blink::mojom::DevToolsAgent> agent_remote,
    mojo::PendingReceiver<blink::mojom::DevToolsAgentHost> host_receiver) {
  if (!devtools_proxy_)
    return;
  devtools_proxy_->NotifyWorkerReadyForInspection(std::move(agent_remote),
                                                  std::move(host_receiver));
}

void EmbeddedWorkerInstance::OnScriptLoaded() {
  if (!inflight_start_task_)
    return;

  // Renderer side has started to launch the worker thread.
  starting_phase_ = SCRIPT_LOADED;
  owner_version_->OnMainScriptLoaded();
}

void EmbeddedWorkerInstance::OnWorkerVersionInstalled() {
  if (devtools_proxy_)
    devtools_proxy_->NotifyWorkerVersionInstalled();
}

void EmbeddedWorkerInstance::OnWorkerVersionDoomed() {
  if (ServiceWorkerContext::IsServiceWorkerOnUIEnabled()) {
    NotifyWorkerVersionDoomedOnUI(process_id(),
                                  worker_devtools_agent_route_id(),
                                  base::WrapRefCounted(context_->wrapper()),
                                  owner_version_->version_id());
  } else {
    ui_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(NotifyWorkerVersionDoomedOnUI, process_id(),
                                  worker_devtools_agent_route_id(),
                                  base::WrapRefCounted(context_->wrapper()),
                                  owner_version_->version_id()));
  }
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
    bool has_fetch_handler,
    int thread_id,
    blink::mojom::EmbeddedWorkerStartTimingPtr start_timing) {
  TRACE_EVENT0("ServiceWorker", "EmbeddedWorkerInstance::OnStarted");
  if (!(start_timing->start_worker_received_time <=
            start_timing->script_evaluation_start_time &&
        start_timing->script_evaluation_start_time <=
            start_timing->script_evaluation_end_time)) {
    mojo::ReportBadMessage("EWI_BAD_START_TIMING");
    return;
  }

  if (!devtools_attached_)
    lifetime_tracker_ = std::make_unique<ScopedLifetimeTracker>();

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
    times.thread_hop_time = inflight_start_task_->thread_hop_time();

    ServiceWorkerMetrics::RecordStartWorkerTiming(times, start_situation_);
  }

  DCHECK_EQ(EmbeddedWorkerStatus::STARTING, status_);
  status_ = EmbeddedWorkerStatus::RUNNING;
  thread_id_ = thread_id;
  inflight_start_task_.reset();
  for (auto& observer : listener_list_) {
    observer.OnStarted(start_status, has_fetch_handler);
    // |this| may be destroyed here. Fortunately we know there is only one
    // observer in production code.
  }
}

void EmbeddedWorkerInstance::OnStopped() {
  EmbeddedWorkerStatus old_status = status_;
  ReleaseProcess();
  for (auto& observer : listener_list_)
    observer.OnStopped(old_status);
}

void EmbeddedWorkerInstance::Detach() {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  if (status() == EmbeddedWorkerStatus::STOPPED)
    return;

  EmbeddedWorkerStatus old_status = status_;
  ReleaseProcess();
  for (auto& observer : listener_list_)
    observer.OnDetached(old_status);
}

void EmbeddedWorkerInstance::UpdateForegroundPriority() {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  if (process_handle_ &&
      owner_version_->ShouldRequireForegroundPriority(process_id())) {
    NotifyForegroundServiceWorkerAdded();
  } else {
    NotifyForegroundServiceWorkerRemoved();
  }
}

void EmbeddedWorkerInstance::UpdateLoaderFactories(
    std::unique_ptr<blink::PendingURLLoaderFactoryBundle> script_bundle,
    std::unique_ptr<blink::PendingURLLoaderFactoryBundle> subresource_bundle) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  DCHECK(subresource_loader_updater_.is_bound());

  // It's set to nullptr when the caller wants to update script bundle only.
  if (subresource_bundle) {
    subresource_loader_updater_->UpdateSubresourceLoaderFactories(
        std::move(subresource_bundle));
  }

  if (script_loader_factory_) {
    static_cast<ServiceWorkerScriptLoaderFactory*>(
        script_loader_factory_->impl())
        ->Update(base::MakeRefCounted<blink::URLLoaderFactoryBundle>(
            std::move(script_bundle)));
  }
}

void EmbeddedWorkerInstance::BindCacheStorage(
    mojo::PendingReceiver<blink::mojom::CacheStorage> receiver) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  pending_cache_storage_receivers_.push_back(std::move(receiver));
  BindCacheStorageInternal();
}

base::WeakPtr<EmbeddedWorkerInstance> EmbeddedWorkerInstance::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

// Returns a factory bundle for doing loads on behalf of the specified |rph| and
// |origin|. The returned bundle has a default factory that goes to network and
// it may also include scheme-specific factories that don't go to network.
//
// The network factory does not support reconnection to the network service.
std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
EmbeddedWorkerInstance::CreateFactoryBundleOnUI(
    RenderProcessHost* rph,
    int routing_id,
    const url::Origin& origin,
    const base::Optional<network::CrossOriginEmbedderPolicy>&
        cross_origin_embedder_policy,
    mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
        coep_reporter,
    ContentBrowserClient::URLLoaderFactoryType factory_type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto factory_bundle =
      std::make_unique<blink::PendingURLLoaderFactoryBundle>();
  mojo::PendingReceiver<network::mojom::URLLoaderFactory>
      default_factory_receiver = factory_bundle->pending_default_factory()
                                     .InitWithNewPipeAndPassReceiver();
  network::mojom::URLLoaderFactoryParamsPtr factory_params =
      URLLoaderFactoryParamsHelper::CreateForWorker(
          rph, origin,
          net::IsolationInfo::Create(
              net::IsolationInfo::RedirectMode::kUpdateNothing, origin, origin,
              net::SiteForCookies::FromOrigin(origin)),
          std::move(coep_reporter),
          "EmbeddedWorkerInstance::CreateFactoryBundlesOnUI");
  bool bypass_redirect_checks = false;

  DCHECK(factory_type ==
             ContentBrowserClient::URLLoaderFactoryType::kServiceWorkerScript ||
         factory_type == ContentBrowserClient::URLLoaderFactoryType::
                             kServiceWorkerSubResource);

  // See if the default factory needs to be tweaked by the embedder.
  GetContentClient()->browser()->WillCreateURLLoaderFactory(
      rph->GetBrowserContext(), nullptr /* frame_host */, rph->GetID(),
      factory_type, origin, base::nullopt /* navigation_id */,
      base::kInvalidUkmSourceId, &default_factory_receiver,
      &factory_params->header_client, &bypass_redirect_checks,
      nullptr /* disable_secure_dns */, &factory_params->factory_override);
  devtools_instrumentation::WillCreateURLLoaderFactoryForServiceWorker(
      rph, routing_id, &factory_params->factory_override);

  factory_params->client_security_state =
      network::mojom::ClientSecurityState::New();

  // Without PlzServiceWorker, the COEP header might no be known initially for
  // new ServiceWorker. The default COEP header is used instead here. Later, the
  // subresource loader factories will be updated with the correct COEP header.
  // See: https://chromium-review.googlesource.com/c/chromium/src/+/2029403
  factory_params->client_security_state->cross_origin_embedder_policy =
      cross_origin_embedder_policy ? cross_origin_embedder_policy.value()
                                   : network::CrossOriginEmbedderPolicy();

  rph->CreateURLLoaderFactory(std::move(default_factory_receiver),
                              std::move(factory_params));

  factory_bundle->set_bypass_redirect_checks(bypass_redirect_checks);

  ContentBrowserClient::NonNetworkURLLoaderFactoryDeprecatedMap
      non_network_uniquely_owned_factories;
  ContentBrowserClient::NonNetworkURLLoaderFactoryMap non_network_factories;
  non_network_factories[url::kDataScheme] = DataURLLoaderFactory::Create();
  GetContentClient()
      ->browser()
      ->RegisterNonNetworkSubresourceURLLoaderFactories(
          rph->GetID(), MSG_ROUTING_NONE, &non_network_uniquely_owned_factories,
          &non_network_factories);

  for (auto& pair : non_network_uniquely_owned_factories) {
    const std::string& scheme = pair.first;
    std::unique_ptr<network::mojom::URLLoaderFactory> factory =
        std::move(pair.second);

    // To be safe, ignore schemes that aren't allowed to register service
    // workers. We assume that importScripts and fetch() should fail on such
    // schemes.
    if (!base::Contains(GetServiceWorkerSchemes(), scheme))
      continue;
    mojo::PendingRemote<network::mojom::URLLoaderFactory> factory_remote;
    mojo::MakeSelfOwnedReceiver(
        std::move(factory), factory_remote.InitWithNewPipeAndPassReceiver());
    factory_bundle->pending_scheme_specific_factories().emplace(
        scheme, std::move(factory_remote));
  }

  for (auto& pair : non_network_factories) {
    const std::string& scheme = pair.first;
    mojo::PendingRemote<network::mojom::URLLoaderFactory>& pending_remote =
        pair.second;

    // To be safe, ignore schemes that aren't allowed to register service
    // workers. We assume that importScripts and fetch() should fail on such
    // schemes.
    if (!base::Contains(GetServiceWorkerSchemes(), scheme))
      continue;

    factory_bundle->pending_scheme_specific_factories().emplace(
        scheme, std::move(pending_remote));
  }

  return factory_bundle;
}

void EmbeddedWorkerInstance::CreateFactoryBundles(
    CreateFactoryBundlesCallback callback) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  RunOrPostTaskOnThread(
      FROM_HERE, BrowserThread::UI,
      base::BindOnce(
          &CreateFactoryBundlesOnUI, process_id(),
          worker_devtools_agent_route_id(), owner_version_->script_url(),
          owner_version_->cross_origin_embedder_policy(),
          base::BindOnce(&EmbeddedWorkerInstance::OnCreatedFactoryBundles,
                         AsWeakPtr(), std::move(callback))));
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
    blink::mojom::ConsoleMessageSource source,
    blink::mojom::ConsoleMessageLevel message_level,
    const base::string16& message,
    int line_number,
    const GURL& source_url) {
  for (auto& observer : listener_list_) {
    observer.OnReportConsoleMessage(source, message_level, message, line_number,
                                    source_url);
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
  AbortLifetimeTracking();
}

void EmbeddedWorkerInstance::AbortLifetimeTracking() {
  if (lifetime_tracker_) {
    lifetime_tracker_->Abort();
    lifetime_tracker_.reset();
  }
}

void EmbeddedWorkerInstance::OnNetworkAccessedForScriptLoad() {
  starting_phase_ = SCRIPT_DOWNLOADING;
  network_accessed_for_script_ = true;
}

void EmbeddedWorkerInstance::ReleaseProcess() {
  // Abort an inflight start task.
  inflight_start_task_.reset();

  NotifyForegroundServiceWorkerRemoved();

  instance_host_receiver_.reset();
  devtools_proxy_.reset();
  process_handle_.reset();
  lifetime_tracker_.reset();
  subresource_loader_updater_.reset();
  coep_reporter_.reset();
  status_ = EmbeddedWorkerStatus::STOPPED;
  starting_phase_ = NOT_STARTING;
  thread_id_ = ServiceWorkerConsts::kInvalidEmbeddedWorkerThreadId;
  token_ = base::nullopt;
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

void EmbeddedWorkerInstance::NotifyForegroundServiceWorkerAdded() {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

  if (!process_handle_ || foreground_notified_)
    return;

  foreground_notified_ = true;

  if (ServiceWorkerContext::IsServiceWorkerOnUIEnabled()) {
    NotifyForegroundServiceWorkerOnUIThread(true /* added */, process_id());
  } else {
    ui_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&NotifyForegroundServiceWorkerOnUIThread,
                                  true /* added */, process_id()));
  }
}

void EmbeddedWorkerInstance::NotifyForegroundServiceWorkerRemoved() {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

  if (!process_handle_ || !foreground_notified_)
    return;

  foreground_notified_ = false;

  if (ServiceWorkerContext::IsServiceWorkerOnUIEnabled()) {
    NotifyForegroundServiceWorkerOnUIThread(false /* added */, process_id());
  } else {
    ui_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&NotifyForegroundServiceWorkerOnUIThread,
                                  false /* added */, process_id()));
  }
}

mojo::PendingRemote<network::mojom::URLLoaderFactory>
EmbeddedWorkerInstance::MakeScriptLoaderFactoryRemote(
    std::unique_ptr<blink::PendingURLLoaderFactoryBundle> script_bundle) {
  mojo::PendingRemote<network::mojom::URLLoaderFactory>
      script_loader_factory_remote;

  auto script_bundle_factory =
      base::MakeRefCounted<blink::URLLoaderFactoryBundle>(
          std::move(script_bundle));
  script_loader_factory_ = mojo::MakeSelfOwnedReceiver(
      std::make_unique<ServiceWorkerScriptLoaderFactory>(
          context_, owner_version_->worker_host()->GetWeakPtr(),
          std::move(script_bundle_factory)),
      script_loader_factory_remote.InitWithNewPipeAndPassReceiver());

  return script_loader_factory_remote;
}

void EmbeddedWorkerInstance::BindCacheStorageInternal() {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  // Without PlzServiceWorker, the COEP header might not be known initially.
  // The in-flight CacheStorage requests are kept until the main script has
  // loaded the headers and the COEP one is known.
  if (!owner_version_->cross_origin_embedder_policy())
    return;

  network::CrossOriginEmbedderPolicy coep =
      owner_version_->cross_origin_embedder_policy().value();

  for (auto& receiver : pending_cache_storage_receivers_) {
    mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
        coep_reporter_remote;
    if (coep_reporter_) {
      coep_reporter_->Clone(
          coep_reporter_remote.InitWithNewPipeAndPassReceiver());
    }

    RunOrPostTaskOnThread(
        FROM_HERE, BrowserThread::UI,
        base::BindOnce(content::BindCacheStorageOnUIThread, process_id(),
                       owner_version_->origin(), coep,
                       std::move(coep_reporter_remote), std::move(receiver)));
  }
  pending_cache_storage_receivers_.clear();
}

void EmbeddedWorkerInstance::OnCreatedFactoryBundles(
    CreateFactoryBundlesCallback callback,
    std::unique_ptr<blink::PendingURLLoaderFactoryBundle> script_bundle,
    std::unique_ptr<blink::PendingURLLoaderFactoryBundle> subresource_bundle,
    mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
        coep_reporter,
    mojo::PendingReceiver<blink::mojom::ReportingObserver>
        reporting_observer_receiver) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

  if (coep_reporter) {
    coep_reporter_.Bind(std::move(coep_reporter));
  }
  if (reporting_observer_receiver) {
    owner_version_->set_reporting_observer_receiver(
        std::move(reporting_observer_receiver));
  }
  BindCacheStorageInternal();
  std::move(callback).Run(std::move(script_bundle),
                          std::move(subresource_bundle));
}

}  // namespace content
