// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/service_worker/service_worker_task_queue.h"

#include <memory>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

#include "base/auto_reset.h"
#include "base/check.h"
#include "base/containers/contains.h"
#include "base/containers/map_util.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/syslog_logging.h"
#include "base/timer/timer.h"
#include "base/types/cxx23_to_underlying.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/console_message.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/service_worker_running_info.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_error.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/renderer_startup_helper.h"
#include "extensions/browser/service_worker/service_worker_task_queue_factory.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/file_util.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/common/manifest_handlers/incognito_info.h"
#include "net/base/backoff_entry.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration_options.mojom.h"
#include "url/origin.h"

using content::BrowserContext;

namespace extensions {

namespace {

// A preference key storing the information about an extension that was
// activated and has a registered worker based background page.
const char kPrefServiceWorkerRegistrationInfo[] =
    "service_worker_registration_info";

// The extension version of the registered service worker.
const char kServiceWorkerVersion[] = "version";

// The backoff policy for a worker registration/start retry.
constexpr net::BackoffEntry::Policy kRetryBackoffPolicy = {
    .num_errors_to_ignore = 0,
    .initial_delay_ms = 250,
    .multiply_factor = 2.0,
    .jitter_factor = 0.1,
    .maximum_backoff_ms = -1,
    .entry_lifetime_ms = -1,
    .always_use_initial_delay = false,
};
// Maximum number of retries for registration and start requests.
constexpr int kMaxRetries = 3;

ServiceWorkerTaskQueue::TestObserver* g_test_observer = nullptr;

}  // namespace

ServiceWorkerTaskQueue::ServiceWorkerTaskQueue(BrowserContext* browser_context)
    : browser_context_(browser_context) {}

ServiceWorkerTaskQueue::~ServiceWorkerTaskQueue() {
  for (const auto& entry : observing_worker_contexts_) {
    entry.first->RemoveSyncObserver(this);
  }
}

// Manages registration/start retry attempts with exponential backoff.
struct ServiceWorkerTaskQueue::RetryState {
  explicit RetryState(const net::BackoffEntry::Policy* policy);

  net::BackoffEntry backoff_entry;
  base::OneShotTimer timer;
};

ServiceWorkerTaskQueue::RetryState::RetryState(
    const net::BackoffEntry::Policy* policy)
    : backoff_entry(policy) {}

ServiceWorkerTaskQueue::TestObserver::TestObserver() = default;

ServiceWorkerTaskQueue::TestObserver::~TestObserver() = default;

// static
ServiceWorkerTaskQueue* ServiceWorkerTaskQueue::Get(BrowserContext* context) {
  return ServiceWorkerTaskQueueFactory::GetForBrowserContext(context);
}

bool ServiceWorkerTaskQueue::IsStartWorkerFailureUnexpected(
    blink::ServiceWorkerStatusCode status) {
  if (status != blink::ServiceWorkerStatusCode::kErrorAbort) {
    return true;
  }

  return browser_context_shutting_down_;
}

void ServiceWorkerTaskQueue::RendererDidInitializeServiceWorkerContext(
    int render_process_id,
    const ExtensionId& extension_id,
    int64_t service_worker_version_id,
    int thread_id,
    const blink::ServiceWorkerToken& service_worker_token) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  ExtensionRegistry* registry = ExtensionRegistry::Get(browser_context_);
  DCHECK(registry);
  const Extension* extension =
      registry->enabled_extensions().GetByID(extension_id);
  // The caller should have validated that the extension is still enabled.
  CHECK(extension);

  content::RenderProcessHost* process_host =
      content::RenderProcessHost::FromID(render_process_id);
  // The caller should have validated that the RenderProcessHost is still
  // active.
  CHECK(process_host);

  util::InitializeFileSchemeAccessForExtension(render_process_id, extension_id,
                                               browser_context_);
  // TODO(jlulejian): Do we need to start tracking this in initialization or
  // could we start in `RendererDidStartServiceWorkerContext()` instead since
  // this is for a running (started) worker?
  ProcessManager::Get(browser_context_)
      ->StartTrackingServiceWorkerRunningInstance(
          {extension_id, render_process_id, service_worker_version_id,
           thread_id, service_worker_token});
  RendererStartupHelperFactory::GetForBrowserContext(browser_context_)
      ->ActivateExtensionInProcess(*extension, process_host);

  if (g_test_observer) {
    g_test_observer->RendererDidInitializeServiceWorkerContext(extension_id);
  }
}

void ServiceWorkerTaskQueue::RendererDidStartServiceWorkerContext(
    int render_process_id,
    const ExtensionId& extension_id,
    const base::UnguessableToken& activation_token,
    const GURL& service_worker_scope,
    int64_t service_worker_version_id,
    int thread_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto [worker_state, context_id] =
      GetWorkerStateForActivation(extension_id, activation_token);
  if (worker_state) {
    const WorkerId worker_id = {extension_id, render_process_id,
                                service_worker_version_id, thread_id};
    worker_state->RendererDidStartServiceWorkerContext(context_id, worker_id);
  }
}

void ServiceWorkerTaskQueue::RenderProcessForWorkerExited(
    const WorkerId& worker_id) {
  if (auto activation_token =
          GetCurrentActivationToken(worker_id.extension_id)) {
    auto [worker_state, context_id] =
        GetWorkerStateForActivation(worker_id.extension_id, *activation_token);
    if (worker_state) {
      worker_state->Reset();
    }
  }
}

void ServiceWorkerTaskQueue::RendererDidStopServiceWorkerContext(
    int render_process_id,
    const ExtensionId& extension_id,
    const base::UnguessableToken& activation_token,
    const GURL& service_worker_scope,
    int64_t service_worker_version_id,
    int thread_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const WorkerId worker_id = {extension_id, render_process_id,
                              service_worker_version_id, thread_id};
  auto [worker_state, context_id] =
      GetWorkerStateForActivation(extension_id, activation_token);
  if (worker_state) {
    worker_state->RendererDidStopServiceWorkerContext(worker_id,
                                                      service_worker_scope);
    if (g_test_observer) {
      g_test_observer->RendererDidStopServiceWorkerContext(
          context_id.extension_id);
    }
  }
}

// static
void ServiceWorkerTaskQueue::SetObserverForTest(TestObserver* observer) {
  g_test_observer = observer;
}

bool ServiceWorkerTaskQueue::ShouldEnqueueTask(
    BrowserContext* context,
    const Extension* extension) const {
  // If the `OptimizeServiceWorkerStartRequests` feature is enabled, returns
  // false (don't queue) if the worker is ready, indicating the caller should
  // dispatch immediately. Returns true (queue) otherwise. If the feature is
  // disabled, always returns true.
  if (base::FeatureList::IsEnabled(
          extensions_features::kOptimizeServiceWorkerStartRequests)) {
    return !IsReadyToRunTasks(context, extension);
  }
  return true;
}

bool ServiceWorkerTaskQueue::IsReadyToRunTasks(
    content::BrowserContext* context,
    const Extension* extension) const {
  if (!extension) {
    // TODO(crbug.com/339908207): Create tests for this once crash is confirmed
    // fixed.
    // An extension may have been unloaded when this runs.
    return false;
  }

  auto activation_token = GetCurrentActivationToken(extension->id());

  if (!activation_token) {
    // Extension is not active so the worker should not be running.
    return false;
  }

  const SequencedContextId context_id(
      extension->id(), browser_context_->UniqueId(), *activation_token);
  const ServiceWorkerState* worker_state = GetWorkerState(context_id);

  // If this feature is enabled, the worker state should be reliable
  // and the single source of truth for worker readiness.
  if (base::FeatureList::IsEnabled(
          extensions_features::kOptimizeServiceWorkerStartRequests)) {
    return worker_state && worker_state->IsReady();
  }

  if (!worker_state || !worker_state->worker_id()) {
    // Assume the worker has not been started. It is likely in
    // blink::EmbeddedWorkerStatus::(kStarting|kStopped) status.
    return false;
  }
  // We must check both states since the worker could begin stopping and call
  // `RendererDidStopServiceWorkerContext` after
  // `ServiceWorkerState::BrowserState::kReady`.
  return (worker_state->browser_state() ==
          ServiceWorkerState::BrowserState::kReady) &&
         (worker_state->renderer_state() ==
          ServiceWorkerState::RendererState::kActive);
}

void ServiceWorkerTaskQueue::AddPendingTask(
    const LazyContextId& lazy_context_id,
    PendingTask task) {
  DCHECK(lazy_context_id.IsForServiceWorker());
  base::UmaHistogramBoolean(
      "Extensions.ServiceWorkerBackground.AddPendingTaskForRunningWorker3",
      IsReadyToRunTasks(
          browser_context_,
          extensions::ExtensionRegistry::Get(browser_context_)
              ->GetInstalledExtension(lazy_context_id.extension_id())));

  // TODO(lazyboy): Do we need to handle incognito context?

  auto activation_token =
      GetCurrentActivationToken(lazy_context_id.extension_id());
  DCHECK(activation_token)
      << "Trying to add pending task to an inactive extension: "
      << lazy_context_id.extension_id();
  const SequencedContextId context_id = {
      lazy_context_id.extension_id(),
      lazy_context_id.browser_context()->UniqueId(), *activation_token};

  if (!base::Contains(worker_registered_, context_id)) {
    // If the worker hasn't finished registration, wait for it to complete. The
    // worker can't be started until a registration is found for it in the
    // //content layer. `DidRegisterServiceWorker()` will start the worker to
    // run the `task` later.
    // TODO(crbug.com/40276609): consider moving registration check logic into
    // `ServiceWorkerState`, since registration could be considered part of
    // starting.
    AddPendingTaskForContext(std::move(task), context_id);
    return;
  }

  ServiceWorkerState* worker_state = GetWorkerState(context_id);
  DCHECK(worker_state);

  if (base::FeatureList::IsEnabled(
          extensions_features::kOptimizeServiceWorkerStartRequests) &&
      worker_state->IsReady()) {
    DispatchTasksImmediately(context_id, base::span_from_ref(task));
  } else {
    // This is either the behavior always (OptimizeServiceWorkerStartRequests
    // off) or (when OptimizeServiceWorkerStartRequests on) when the worker is
    // not ready: queue task and maybe start.
    AddPendingTaskForContext(std::move(task), context_id);
    MaybeStartWorker(worker_state, context_id);
  }
}

void ServiceWorkerTaskQueue::DispatchTasksImmediately(
    const SequencedContextId& context_id,
    base::span<PendingTask> tasks) {
  ServiceWorkerState* worker_state = GetWorkerState(context_id);
  DCHECK(worker_state);
  DCHECK(!base::FeatureList::IsEnabled(
             extensions_features::kOptimizeServiceWorkerStartRequests) ||
         worker_state->IsReady());

  const auto& worker_id = *worker_state->worker_id();
  LazyContextTaskQueue::ContextInfo context_info(
      context_id.extension_id,
      content::RenderProcessHost::FromID(worker_id.render_process_id),
      worker_id.version_id, worker_id.thread_id,
      Extension::GetServiceWorkerScopeFromExtensionId(context_id.extension_id));

  for (auto& task : tasks) {
    std::move(task).Run(
        std::make_unique<LazyContextTaskQueue::ContextInfo>(context_info));
  }
}

void ServiceWorkerTaskQueue::ActivateExtension(const Extension* extension) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // TODO(crbug.com/362791965): Enable this check once it is no longer possible
  // to activate an extension when the browser context is shutting down.
  // CHECK(!browser_context_shutting_down_);

  const ExtensionId extension_id = extension->id();
  base::UnguessableToken activation_token = base::UnguessableToken::Create();
  activation_tokens_[extension_id] = activation_token;
  const SequencedContextId context_id = {
      extension_id, browser_context_->UniqueId(), activation_token};
  DCHECK(!base::Contains(worker_state_map_, context_id));

  content::ServiceWorkerContext* service_worker_context =
      GetServiceWorkerContext(extension->id());
  StartObserving(service_worker_context);

  auto [worker_state_iter, inserted] = worker_state_map_.try_emplace(
      context_id,
      std::make_unique<ServiceWorkerState>(
          service_worker_context, ProcessManager::Get(browser_context_)));
  if (inserted) {
    worker_state_observations_.AddObservation(worker_state_iter->second.get());
  }
  pending_tasks_map_.try_emplace(context_id);

  // Note: version.IsValid() = false implies we didn't have any prefs stored.
  base::Version version = RetrieveRegisteredServiceWorkerVersion(extension_id);
  const bool service_worker_already_registered =
      version.IsValid() && version == extension->version();
  if (g_test_observer) {
    g_test_observer->OnActivateExtension(extension_id,
                                         !service_worker_already_registered);
  }

  DCHECK(!base::Contains(worker_registered_, context_id));
  if (service_worker_already_registered) {
    worker_registered_.insert(context_id);
    VerifyRegistration(service_worker_context, context_id, extension->url());
    return;
  }

  RegisterServiceWorker(RegistrationReason::REGISTER_ON_EXTENSION_LOAD,
                        context_id, *extension);
}

void ServiceWorkerTaskQueue::VerifyRegistration(
    content::ServiceWorkerContext* service_worker_context,
    const SequencedContextId& context_id,
    const GURL& scope) {
  service_worker_context->CheckHasServiceWorker(
      scope, blink::StorageKey::CreateFirstParty(url::Origin::Create(scope)),
      base::BindOnce(&ServiceWorkerTaskQueue::DidVerifyRegistration,
                     weak_factory_.GetWeakPtr(), context_id));
}

void ServiceWorkerTaskQueue::Shutdown() {
  browser_context_shutting_down_ = true;
}

void ServiceWorkerTaskQueue::OnWorkerStart(const SequencedContextId& context_id,
                                           const WorkerId& worker_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!IsCurrentActivation(context_id.extension_id, context_id.token)) {
    // Extension run with |activation_token| was already deactivated.
    // TODO(lazyboy): Add a DCHECK that the worker in question is actually
    // shutting down soon.
    DCHECK(!GetWorkerState(context_id));
    return;
  }

  // Clear any pending start retry attempts now that the worker has started.
  // If there were retries attempted, emit metrics about the success.
  ClearRetryState(
      context_id.token, worker_start_retries_,
      "Extensions.ServiceWorkerBackground.StartWorkerRetryAttemptsResult",
      /*success=*/true);

  if (g_test_observer) {
    g_test_observer->DidStartWorker(context_id.extension_id);
  }

  if (!HasPendingTasks(context_id)) {
    return;
  }

  std::vector<PendingTask> tasks;
  std::swap(GetOrAddPendingTasks(context_id), tasks);
  DispatchTasksImmediately(context_id, tasks);
}

void ServiceWorkerTaskQueue::OnWorkerStartFail(
    const SequencedContextId& context_id,
    base::Time start_time,
    content::StatusCodeResponse status) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!IsCurrentActivation(context_id.extension_id, context_id.token)) {
    // This can happen when the registration got unregistered right before we
    // tried to start it. See crbug.com/999027 for details.
    DCHECK(!GetWorkerState(context_id));
    // In that case, we expect `DeactivateExtension` to have been called
    // already, and for the registration records to have already been cleared.
    DCHECK(!pending_storage_registrations_.contains(context_id.extension_id));
    // We expect `DeactivateExtension` to have cleaned up the start attempts.
    DCHECK(!worker_start_retries_.contains(context_id.token));
    return;
  }

  // If the start failed due to transient reasons, retry starting the worker.
  if (base::FeatureList::IsEnabled(
          extensions_features::kExtensionsServiceWorkerStartRetry) &&
      IsStartFailureRetryable(status.status_code)) {
    if (ScheduleRetry(context_id.token, worker_start_retries_,
                      base::BindOnce(&ServiceWorkerTaskQueue::RetryStartWorker,
                                     weak_factory_.GetWeakPtr(), context_id))) {
      return;
    }
  }

  // Retries exhausted or non-transient error. Proceed with failure handling.
  // Clean up the retries entry for this context. If there were retries
  // attempted, emit metrics about the ultimate failure.
  ClearRetryState(
      context_id.token, worker_start_retries_,
      "Extensions.ServiceWorkerBackground.StartWorkerRetryAttemptsResult",
      /*success=*/false);

  if (IsStartWorkerFailureUnexpected(status.status_code)) {
    base::UmaHistogramBoolean(
        "Extensions.ServiceWorkerBackground.StartWorkerStatus", false);
    base::UmaHistogramEnumeration(
        "Extensions.ServiceWorkerBackground.StartWorker_FailStatus",
        status.status_code);
    base::UmaHistogramTimes(
        "Extensions.ServiceWorkerBackground.StartWorkerTime_Fail",
        base::Time::Now() - start_time);
    LOG(ERROR)
        << "DidStartWorkerFail " << context_id.extension_id << ": "
        << static_cast<std::underlying_type_t<blink::ServiceWorkerStatusCode>>(
               status.status_code);
  }

  if (g_test_observer) {
    std::vector<PendingTask>* tasks = pending_tasks(context_id);
    g_test_observer->DidStartWorkerFail(
        context_id.extension_id, tasks ? tasks->size() : 0, status.status_code);
  }

  RunAndClearPendingTasksWithNullContext(context_id);
  // TODO(crbug.com/40680422): Needs more thought: extension would be in
  // perma-broken state after this as the registration wouldn't be stored if
  // this happens.

  // If there was a pending registration for this extension, erase it.
  pending_storage_registrations_.erase(context_id.extension_id);
}

void ServiceWorkerTaskQueue::OnWorkerStop(int64_t version_id,
                                          const GURL& scope) {
  // TODO(crbug.com/40936639): Confirming this is true in order to allow for
  // synchronous notification of this status change.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Stop tracking the worker for extension API purposes.
  const ExtensionId& extension_id = scope.GetHost();
  ProcessManager::Get(browser_context_)
      ->StopTrackingServiceWorkerRunningInstance(extension_id, version_id);

  if (g_test_observer) {
    g_test_observer->UntrackServiceWorkerState(scope);
  }
}

void ServiceWorkerTaskQueue::RegisterServiceWorker(
    RegistrationReason reason,
    const SequencedContextId& context_id,
    const Extension& extension) {
  GURL script_url =
      BackgroundInfo::GetBackgroundServiceWorkerScriptURL(&extension);
  blink::mojom::ServiceWorkerRegistrationOptions option;
  if (BackgroundInfo::GetBackgroundServiceWorkerType(&extension) ==
      BackgroundServiceWorkerType::kModule) {
    option.type = blink::mojom::ScriptType::kModule;
  }
  option.scope = extension.url();

  content::ServiceWorkerContext* service_worker_context =
      GetServiceWorkerContext(extension.id());
  service_worker_context->RegisterServiceWorker(
      script_url,
      blink::StorageKey::CreateFirstParty(url::Origin::Create(option.scope)),
      option,
      base::BindOnce(&ServiceWorkerTaskQueue::DidRegisterServiceWorker,
                     weak_factory_.GetWeakPtr(), context_id, reason,
                     base::Time::Now()));
}

void ServiceWorkerTaskQueue::DeactivateExtension(const Extension* extension) {
  const ExtensionId extension_id = extension->id();
  RemoveRegisteredServiceWorkerInfo(extension_id);
  std::optional<base::UnguessableToken> activation_token =
      GetCurrentActivationToken(extension_id);

  // Extension was never activated, this happens in tests.
  if (!activation_token) {
    return;
  }

  activation_tokens_.erase(extension_id);
  const SequencedContextId context_id = {
      extension_id, browser_context_->UniqueId(), *activation_token};
  ServiceWorkerState* worker_state = GetWorkerState(context_id);
  DCHECK(worker_state);
  // At this point `ExtensionRegistrar` has already triggered a worker stop
  // synchronously via `ServiceWorkerManager::OnExtensionUnloaded`, as part of
  // the deactivation process.
  DCHECK(!worker_state->IsReady());

  RunAndClearPendingTasksWithNullContext(context_id);
  worker_state_observations_.RemoveObservation(worker_state);
  worker_state_map_.erase(context_id);
  pending_tasks_map_.erase(context_id);
  bool worker_previously_registered = worker_registered_.erase(context_id);
  // If an extension/worker is unloaded/disabled before the registration
  // callback then we might still have this record to delete.
  worker_registration_retries_.erase(context_id.token);
  // Same for worker start attempts.
  worker_start_retries_.erase(context_id.token);

  // Erase any registrations that might still have been pending being fully
  // stored.
  pending_storage_registrations_.erase(extension_id);

  content::ServiceWorkerContext* service_worker_context =
      GetServiceWorkerContext(extension->id());

  // Note: It's important that the unregistration happen immediately (rather
  // waiting for any controllees to be closed). Otherwise, we can get into a
  // state where the old registration is not cleared by the time we re-register
  // the worker if the extension is being reloaded, e.g. for an update.
  // See https://crbug.com/1501930.
  service_worker_context->UnregisterServiceWorkerImmediately(
      extension->url(),
      blink::StorageKey::CreateFirstParty(extension->origin()),
      base::BindOnce(&ServiceWorkerTaskQueue::DidUnregisterServiceWorker,
                     weak_factory_.GetWeakPtr(), extension_id,
                     *activation_token, worker_previously_registered));

  StopObserving(service_worker_context);
}

std::vector<ServiceWorkerTaskQueue::PendingTask>*
ServiceWorkerTaskQueue::pending_tasks(const SequencedContextId& context_id) {
  return base::FindOrNull(pending_tasks_map_, context_id);
}

std::vector<ServiceWorkerTaskQueue::PendingTask>&
ServiceWorkerTaskQueue::GetOrAddPendingTasks(
    const SequencedContextId& context_id) {
  return pending_tasks_map_[context_id];
}

void ServiceWorkerTaskQueue::AddPendingTaskForContext(
    PendingTask&& pending_task,
    const SequencedContextId& context_id) {
  GetOrAddPendingTasks(context_id).push_back(std::move(pending_task));
}

void ServiceWorkerTaskQueue::RunAndClearPendingTasksWithNullContext(
    const SequencedContextId& context_id) {
  if (!HasPendingTasks(context_id)) {
    return;
  }
  std::vector<PendingTask> tasks;
  std::swap(GetOrAddPendingTasks(context_id), tasks);

  for (auto& task : tasks) {
    // Run the task with nullptr ContextInfo to indicate failure.
    std::move(task).Run(nullptr);
  }
}

bool ServiceWorkerTaskQueue::HasPendingTasks(
    const SequencedContextId& context_id) {
  std::vector<PendingTask>* tasks = pending_tasks(context_id);
  return tasks && !tasks->empty();
}

void ServiceWorkerTaskQueue::MaybeStartWorker(
    ServiceWorkerState* worker_state,
    const SequencedContextId& context_id) {
  if (worker_state->IsStarting()) {
    return;
  }

  EmitWorkerWillBeStartedHistograms(context_id.extension_id);
  worker_state->StartWorker(context_id);
  if (g_test_observer) {
    g_test_observer->RequestedWorkerStart(context_id.extension_id);
  }
}

void ServiceWorkerTaskQueue::RetryRegisterServiceWorker(
    const SequencedContextId& context_id,
    RegistrationReason reason) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Ensure the activation is still current.
  if (!IsCurrentActivation(context_id.extension_id, context_id.token)) {
    // NOTE: retry state has been cleared when `DeactivateExtension` was called.
    DCHECK(!base::Contains(worker_registration_retries_, context_id.token));
    return;
  }

  ExtensionRegistry* registry = ExtensionRegistry::Get(browser_context_);
  DCHECK(registry);
  const Extension* extension =
      registry->enabled_extensions().GetByID(context_id.extension_id);
  if (!extension) {
    // Extension unloaded during retry delay. Clean up retry state.
    worker_registration_retries_.erase(context_id.token);
    return;
  }

  RegisterServiceWorker(reason, context_id, *extension);
}

void ServiceWorkerTaskQueue::RetryStartWorker(
    const SequencedContextId& context_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Ensure the activation is still current.
  if (!IsCurrentActivation(context_id.extension_id, context_id.token)) {
    // NOTE: retry state has been cleared when `DeactivateExtension` was called.
    DCHECK(!base::Contains(worker_start_retries_, context_id.token));
    return;
  }
  ServiceWorkerState* worker_state = GetWorkerState(context_id);
  CHECK(worker_state);

  // The worker might have successfully started or is starting due to another
  // task being added between the failure and this retry attempt.
  if (worker_state->IsReady() || worker_state->IsStarting()) {
    // NOTE: The retry count will be cleared when `OnWorkerStart` is called.
    return;
  }

  // If there are no pending tasks, there is no reason to start the worker.
  // This is unlikely as we got here from a failure while trying to run tasks,
  // but it can conceivably happen in at least two scenarios:
  // - Another task wakes up the worker successfully in the span between
  //   posting the retry task and it running.
  // - Another start attempt has failed with a non-transient error, causing
  //   `OnWorkerStartFail` to call `RunAndClearPendingTasksWithNullContext`.
  if (!HasPendingTasks(context_id)) {
    worker_start_retries_.erase(context_id.token);
    return;
  }

  MaybeStartWorker(worker_state, context_id);
}

bool ServiceWorkerTaskQueue::IsStartFailureRetryable(
    blink::ServiceWorkerStatusCode status_code) const {
  switch (status_code) {
    // --- TRANSIENT FAILURES ---

    // This could be transient if caused by I/O contention or temporary
    // unavailability.
    case blink::ServiceWorkerStatusCode::kErrorDiskCache:
      return true;

    // Generic failure. This can be transient if caused by the worker process
    // being unexpectedly terminated during startup.
    case blink::ServiceWorkerStatusCode::kErrorFailed:
      return true;

    // Registration or version was not found.
    // TODO(https://crbug.com/444255717): investigate and clean up
    // if it's not actually transient.
    case blink::ServiceWorkerStatusCode::kErrorNotFound:
      return true;

    // Failed to spin up the worker. This includes failing to allocate a
    // process, which can happen due to high system load. Retrying can help.
    case blink::ServiceWorkerStatusCode::kErrorStartWorkerFailed:
      return true;

    // The worker took too long to initialize or handle startup events.
    // Often caused by high system load. Retrying can help.
    case blink::ServiceWorkerStatusCode::kErrorTimeout:
      return true;

    // --- NON-TRANSIENT FAILURES ---
    case blink::ServiceWorkerStatusCode::kOk:
    case blink::ServiceWorkerStatusCode::kErrorAbort:
    case blink::ServiceWorkerStatusCode::kErrorProcessNotFound:
    case blink::ServiceWorkerStatusCode::kErrorExists:
    case blink::ServiceWorkerStatusCode::kErrorInstallWorkerFailed:
    case blink::ServiceWorkerStatusCode::kErrorActivateWorkerFailed:
    case blink::ServiceWorkerStatusCode::kErrorIpcFailed:
    case blink::ServiceWorkerStatusCode::kErrorNetwork:
    case blink::ServiceWorkerStatusCode::kErrorSecurity:
    case blink::ServiceWorkerStatusCode::kErrorEventWaitUntilRejected:
    case blink::ServiceWorkerStatusCode::kErrorState:
    case blink::ServiceWorkerStatusCode::kErrorScriptEvaluateFailed:
    case blink::ServiceWorkerStatusCode::kErrorRedundant:
    case blink::ServiceWorkerStatusCode::kErrorDisallowed:
    case blink::ServiceWorkerStatusCode::kErrorInvalidArguments:
    case blink::ServiceWorkerStatusCode::kErrorStorageDisconnected:
    case blink::ServiceWorkerStatusCode::kErrorStorageDataCorrupted:
      return false;  // Do not retry.
  }
}

bool ServiceWorkerTaskQueue::ScheduleRetry(const base::UnguessableToken& token,
                                           RetryMap& retry_map,
                                           base::OnceClosure retry_callback) {
  auto& retry_state = retry_map[token];
  if (!retry_state) {
    retry_state = std::make_unique<RetryState>(&kRetryBackoffPolicy);
  }
  retry_state->backoff_entry.InformOfRequest(false);

  if (retry_state->backoff_entry.failure_count() <= kMaxRetries) {
    base::TimeDelta delay = retry_state->backoff_entry.GetTimeUntilRelease();
    retry_state->timer.Start(FROM_HERE, delay, std::move(retry_callback));
    return true;  // Retry scheduled.
  }

  return false;  // Retries exhausted.
}

void ServiceWorkerTaskQueue::ClearRetryState(
    const base::UnguessableToken& token,
    RetryMap& retry_map,
    const char* histogram_name,
    bool success) {
  auto it = retry_map.find(token);
  if (it == retry_map.end()) {
    return;
  }

  if (it->second->backoff_entry.failure_count() > 0) {
    base::UmaHistogramBoolean(histogram_name, success);
  }
  retry_map.erase(it);
}

void ServiceWorkerTaskQueue::DidRegisterServiceWorker(
    const SequencedContextId& context_id,
    RegistrationReason reason,
    base::Time start_time,
    blink::ServiceWorkerStatusCode status_code) {
  const bool success = IsWorkerRegistrationSuccess(status_code);
  base::UmaHistogramBoolean(
      "Extensions.ServiceWorkerBackground.WorkerRegistrationState2", success);
  if (!success && g_test_observer) {
    g_test_observer->OnWorkerRegistrationFailed(context_id.extension_id,
                                                status_code);
  }

  ExtensionRegistry* registry = ExtensionRegistry::Get(browser_context_);
  const ExtensionId& extension_id = context_id.extension_id;
  DCHECK(registry);
  const Extension* extension =
      registry->enabled_extensions().GetByID(extension_id);
  if (!extension) {
    // No extension and failed registration can expectedly happen if an
    // extension is deactivated when worker activation/registration request is
    // in-flight. But if registration was successful then that could interfere
    // with future worker registrations for the extension.
    base::UmaHistogramBoolean(
        "Extensions.ServiceWorkerBackground.WorkerRegistrationState", !success);
    if (g_test_observer) {
      g_test_observer->OnWorkerRegistered(context_id.extension_id);
    }
    return;
  }
  if (!IsCurrentActivation(extension_id, context_id.token)) {
    // TODO(crbug.com/346732739): This shouldn't be happening since we seem to
    // always remove extension from enabled extension before we delete the
    // extension activation token, but lets confirm that.
    base::UmaHistogramBoolean(
        "Extensions.ServiceWorkerBackground.WorkerRegistrationState", false);
    if (g_test_observer) {
      g_test_observer->OnWorkerRegistered(context_id.extension_id);
    }
    return;
  }

  ServiceWorkerState* worker_state = GetWorkerState(context_id);
  DCHECK(worker_state);

  if (reason == RegistrationReason::RE_REGISTER_ON_STATE_MISMATCH) {
    base::UmaHistogramBoolean(
        "Extensions.ServiceWorkerBackground.RegistrationMismatchMitigated2",
        success);
    if (!success) {
      // TODO(crbug.com/346732739): Create a test for this if it is feasible.
      base::UmaHistogramEnumeration(
          "Extensions.ServiceWorkerBackground.RegistrationMismatchMitigated_"
          "FailStatus",
          status_code);
    }
    if (g_test_observer) {
      g_test_observer->RegistrationMismatchMitigated(extension_id, success);
    }
  }

  // If the registration failed due to timeout then retry registration.
  if (status_code == blink::ServiceWorkerStatusCode::kErrorTimeout &&
      ScheduleRetry(
          context_id.token, worker_registration_retries_,
          base::BindOnce(&ServiceWorkerTaskQueue::RetryRegisterServiceWorker,
                         weak_factory_.GetWeakPtr(), context_id,
                         RegistrationReason::RE_REGISTER_ON_TIMEOUT))) {
    return;
  }

  // Retries exhausted or non-transient error (or success).
  // Clean up the retries entry for this context. If there were retries
  // attempted, emit metrics about the ultimate result.
  ClearRetryState(context_id.token, worker_registration_retries_,
                  "Extensions.ServiceWorkerBackground."
                  "WorkerRegistrationRetryAttemptsResult",
                  success);

  // After retries are exhausted, emit the ultimate end result.
  base::UmaHistogramBoolean(
      "Extensions.ServiceWorkerBackground.WorkerRegistrationState", success);

  if (!success) {
    base::UmaHistogramEnumeration(
        "Extensions.ServiceWorkerBackground.Registration_FailStatus",
        status_code);
  }

  if (!success ||
      // Still show script evaluate error to developer so that it can be fixed,
      // despite it not being considered an internal failure.
      status_code ==
          blink::ServiceWorkerStatusCode::kErrorScriptEvaluateFailed) {
    std::string msg = base::StringPrintf(
        "Service worker registration failed. Status code: %d",
        static_cast<int>(status_code));
    auto error = std::make_unique<ManifestError>(
        extension_id, base::UTF8ToUTF16(msg), manifest_keys::kBackground,
        file_util::ExtensionURLToRelativeFilePath(
            BackgroundInfo::GetBackgroundServiceWorkerScriptURL(extension))
            .AsUTF16Unsafe());

    ExtensionsBrowserClient::Get()->ReportError(browser_context_,
                                                std::move(error));
    if (g_test_observer) {
      g_test_observer->OnWorkerRegistered(context_id.extension_id);
    }
    return;
  }
  base::UmaHistogramTimes("Extensions.ServiceWorkerBackground.RegistrationTime",
                          base::Time::Now() - start_time);

  worker_registered_.insert(context_id);
  pending_storage_registrations_.emplace(
      extension->id(), *GetCurrentActivationToken(extension->id()));

  if (HasPendingTasks(context_id)) {
    MaybeStartWorker(worker_state, context_id);
  }

  if (g_test_observer) {
    g_test_observer->OnWorkerRegistered(context_id.extension_id);
  }
}

void ServiceWorkerTaskQueue::DidUnregisterServiceWorker(
    const ExtensionId& extension_id,
    const base::UnguessableToken& activation_token,
    bool worker_previously_registered,
    blink::ServiceWorkerStatusCode status) {
  // When unregistering the worker we should've already deactivated the
  // extension.
  CHECK(!IsCurrentActivation(extension_id, activation_token));

  bool success =
      IsWorkerUnregistrationSuccess(status, worker_previously_registered);

  base::UmaHistogramBoolean(
      "Extensions.ServiceWorkerBackground.WorkerUnregistrationState", success);
  base::UmaHistogramBoolean(
      "Extensions.ServiceWorkerBackground.WorkerUnregistrationState_"
      "DeactivateExtension",
      success);

  // TODO(crbug.com/346732739): Handle this better than just logging an error
  // message.
  if (!success) {
    LOG(ERROR) << "Failed to unregister service worker for extension id: "
               << extension_id
               << " error status was: " << base::to_underlying(status);
    base::UmaHistogramEnumeration(
        "Extensions.ServiceWorkerBackground.WorkerUnregistrationFailureStatus",
        status);
    base::UmaHistogramEnumeration(
        "Extensions.ServiceWorkerBackground.WorkerUnregistrationFailureStatus_"
        "DeactivateExtension",
        status);
  }

  if (g_test_observer) {
    g_test_observer->WorkerUnregistered(extension_id);
  }
}

bool ServiceWorkerTaskQueue::IsWorkerRegistrationSuccess(
    blink::ServiceWorkerStatusCode status) {
  switch (status) {
    case blink::ServiceWorkerStatusCode::kOk:
      return true;
    case blink::ServiceWorkerStatusCode::kErrorAbort:
      return browser_context_shutting_down_;
    case blink::ServiceWorkerStatusCode::kErrorScriptEvaluateFailed:
      // Developer script syntax errors are considered user errors.
      return true;
    default:
      // All other registration failures are unexpected.
      return false;
  }
}

base::Version ServiceWorkerTaskQueue::RetrieveRegisteredServiceWorkerVersion(
    const ExtensionId& extension_id) {
  if (browser_context_->IsOffTheRecord()) {
    auto it = off_the_record_registrations_.find(extension_id);
    return it != off_the_record_registrations_.end() ? it->second
                                                     : base::Version();
  }
  const base::Value::Dict* info =
      ExtensionPrefs::Get(browser_context_)
          ->ReadPrefAsDict(extension_id, kPrefServiceWorkerRegistrationInfo);
  if (!info) {
    return base::Version();
  }

  if (const std::string* version_string =
          info->FindString(kServiceWorkerVersion)) {
    return base::Version(*version_string);
  }
  return base::Version();
}

void ServiceWorkerTaskQueue::SetRegisteredServiceWorkerInfo(
    const ExtensionId& extension_id,
    const base::Version& version) {
  DCHECK(version.IsValid());
  if (browser_context_->IsOffTheRecord()) {
    off_the_record_registrations_[extension_id] = version;
  } else {
    base::Value::Dict info;
    info.Set(kServiceWorkerVersion, version.GetString());
    ExtensionPrefs::Get(browser_context_)
        ->UpdateExtensionPref(extension_id, kPrefServiceWorkerRegistrationInfo,
                              base::Value(std::move(info)));
  }
}

void ServiceWorkerTaskQueue::RemoveRegisteredServiceWorkerInfo(
    const ExtensionId& extension_id) {
  if (browser_context_->IsOffTheRecord()) {
    off_the_record_registrations_.erase(extension_id);
  } else {
    ExtensionPrefs::Get(browser_context_)
        ->UpdateExtensionPref(extension_id, kPrefServiceWorkerRegistrationInfo,
                              std::nullopt);
  }
}

bool ServiceWorkerTaskQueue::IsCurrentActivation(
    const ExtensionId& extension_id,
    const base::UnguessableToken& activation_token) const {
  return activation_token == GetCurrentActivationToken(extension_id);
}

std::optional<base::UnguessableToken>
ServiceWorkerTaskQueue::GetCurrentActivationToken(
    const ExtensionId& extension_id) const {
  auto iter = activation_tokens_.find(extension_id);
  if (iter == activation_tokens_.end()) {
    return std::nullopt;
  }
  return iter->second;
}

void ServiceWorkerTaskQueue::OnRegistrationStoredSync(int64_t registration_id,
                                                      const GURL& scope) {
  const ExtensionId extension_id = scope.GetHost();
  auto iter = pending_storage_registrations_.find(extension_id);
  if (iter == pending_storage_registrations_.end()) {
    return;
  }

  // The only registrations we track are the ones for root-scope extension
  // service workers.
  DCHECK_EQ(kExtensionScheme, scope.GetScheme());
  DCHECK_EQ("/", scope.GetPath());

  base::UnguessableToken activation_token = iter->second;
  SequencedContextId context_id = {extension_id, browser_context_->UniqueId(),
                                   activation_token};
  pending_storage_registrations_.erase(iter);

  ExtensionRegistry* registry = ExtensionRegistry::Get(browser_context_);
  const Extension* extension =
      registry->enabled_extensions().GetByID(extension_id);

  // Check the extension's presence and current activation; this might be
  // different if the extension was [un|re]loaded.
  if (extension && IsCurrentActivation(extension_id, activation_token)) {
    SetRegisteredServiceWorkerInfo(extension->id(), extension->version());
  }
}

void ServiceWorkerTaskQueue::OnReportConsoleMessageSync(
    int render_process_id,
    int64_t version_id,
    const GURL& scope,
    const content::ConsoleMessage& message) {
  if (message.message_level != blink::mojom::ConsoleMessageLevel::kError) {
    // We don't report certain low-severity errors.
    return;
  }

  auto error_instance = std::make_unique<RuntimeError>(
      scope.GetHost(), browser_context_->IsOffTheRecord(),
      base::UTF8ToUTF16(content::MessageSourceToString(message.source)),
      message.message,
      StackTrace(1, StackFrame(message.line_number, 1,
                               base::UTF8ToUTF16(message.source_url.spec()),
                               u"")) /* Construct a trace to contain
                                        one frame with the error */
      ,
      message.source_url,
      content::ConsoleMessageLevelToLogSeverity(message.message_level),
      -1 /* a service worker does not have a render_view_id */,
      render_process_id,
      /*is_from_service_worker=*/true);

  ExtensionsBrowserClient::Get()->ReportError(browser_context_,
                                              std::move(error_instance));
}

void ServiceWorkerTaskQueue::OnDestructSync(
    content::ServiceWorkerContext* context) {
  StopObserving(context);
}

std::tuple<ServiceWorkerState*, SequencedContextId>
ServiceWorkerTaskQueue::GetWorkerStateForActivation(
    const ExtensionId& extension_id,
    const base::UnguessableToken& activation_token) {
  if (!IsCurrentActivation(extension_id, activation_token)) {
    return {};
  }

  const SequencedContextId context_id = {
      extension_id, browser_context_->UniqueId(), activation_token};
  ServiceWorkerState* worker_state = GetWorkerState(context_id);

  // If the extension is still activated, worker state should still exist.
  CHECK(worker_state);

  return {worker_state, context_id};
}

bool ServiceWorkerTaskQueue::IsWorkerUnregistrationSuccess(
    blink::ServiceWorkerStatusCode status,
    bool worker_previously_registered) {
  switch (status) {
    case blink::ServiceWorkerStatusCode::kOk:
      return true;
    case blink::ServiceWorkerStatusCode::kErrorNotFound:
      return !worker_previously_registered;
    case blink::ServiceWorkerStatusCode::kErrorAbort:
      return browser_context_shutting_down_;
    default:
      // All other unregistration failures are unexpected.
      return false;
  }
}

bool ServiceWorkerTaskQueue::IsWorkerRegistered(
    const ExtensionId extension_id) {
  // TODO(crbug.com/346732739): Key worker_registered_ by extension_id so that
  // this check isn't necessary anymore.
  std::optional<base::UnguessableToken> activation_token =
      GetCurrentActivationToken(extension_id);
  if (!activation_token) {
    // This implies that a request to register the worker hasn't been sent yet,
    // or a worker unregistration has, at least, been sent.
    return false;
  }
  const SequencedContextId context_id = {
      extension_id, browser_context_->UniqueId(), *activation_token};
  return base::Contains(worker_registered_, context_id);
}

size_t ServiceWorkerTaskQueue::GetNumPendingTasksForTest(
    const LazyContextId& lazy_context_id) {
  auto activation_token =
      GetCurrentActivationToken(lazy_context_id.extension_id());
  if (!activation_token) {
    return 0;
  }
  const SequencedContextId context_id = {
      lazy_context_id.extension_id(),
      lazy_context_id.browser_context()->UniqueId(), *activation_token};
  std::vector<PendingTask>* tasks = pending_tasks(context_id);
  return tasks ? tasks->size() : 0;
}

const ServiceWorkerState* ServiceWorkerTaskQueue::GetWorkerState(
    const SequencedContextId& context_id) const {
  const auto* worker_state = base::FindOrNull(worker_state_map_, context_id);
  return worker_state ? worker_state->get() : nullptr;
}

ServiceWorkerState* ServiceWorkerTaskQueue::GetWorkerState(
    const SequencedContextId& context_id) {
  return const_cast<ServiceWorkerState*>(
      std::as_const(*this).GetWorkerState(context_id));
}

content::ServiceWorkerContext* ServiceWorkerTaskQueue::GetServiceWorkerContext(
    const ExtensionId& extension_id) {
  return util::GetServiceWorkerContextForExtensionId(extension_id,
                                                     browser_context_);
}

void ServiceWorkerTaskQueue::StartObserving(
    content::ServiceWorkerContext* service_worker_context) {
  if (++observing_worker_contexts_[service_worker_context] == 1) {
    service_worker_context->AddSyncObserver(this);
  }
}

void ServiceWorkerTaskQueue::StopObserving(
    content::ServiceWorkerContext* service_worker_context) {
  auto iter = observing_worker_contexts_.find(service_worker_context);
  if (iter == observing_worker_contexts_.end()) {
    return;
  }
  DCHECK(iter->second > 0);
  if (--iter->second == 0) {
    service_worker_context->RemoveSyncObserver(this);
    observing_worker_contexts_.erase(iter);
  }
}

void ServiceWorkerTaskQueue::DidVerifyRegistration(
    const SequencedContextId& context_id,
    content::ServiceWorkerCapability capability) {
  const bool is_registered =
      capability != content::ServiceWorkerCapability::NO_SERVICE_WORKER;
  base::UmaHistogramBoolean(
      "Extensions.ServiceWorkerBackground.RegistrationWhenExpected",
      is_registered);

  if (is_registered) {
    return;
  }

  // We expected a SW registration (as ExtensionPrefs said so), but there isn't
  // one. Re-register SW script if the extension is still installed (it's
  // possible it was uninstalled while we were checking).
  const ExtensionId& extension_id = context_id.extension_id;
  ExtensionRegistry* registry = ExtensionRegistry::Get(browser_context_);
  DCHECK(registry);
  const Extension* extension =
      registry->enabled_extensions().GetByID(extension_id);
  if (!extension) {
    return;
  }

  // It is possible that the extension got reloaded while we were verifying the
  // registration. Ignore the request if it is not the current activation.
  // TODO(crbug.com/391414854): Add a test for this.
  if (!IsCurrentActivation(extension_id, context_id.token)) {
    return;
  }

  base::UmaHistogramEnumeration(
      "Extensions.ServiceWorkerBackground.RegistrationMismatchLocation",
      extension->location());

  RegisterServiceWorker(RegistrationReason::RE_REGISTER_ON_STATE_MISMATCH,
                        context_id, *extension);
}

void ServiceWorkerTaskQueue::EmitWorkerWillBeStartedHistograms(
    const ExtensionId& extension_id) {
  bool worker_is_ready_to_run_tasks = IsReadyToRunTasks(
      browser_context_, extensions::ExtensionRegistry::Get(browser_context_)
                            ->GetInstalledExtension(extension_id));
  base::UmaHistogramBoolean(
      "Extensions.ServiceWorkerBackground."
      "RequestedWorkerStartForStartedWorker3",
      worker_is_ready_to_run_tasks);
}

void ServiceWorkerTaskQueue::ActivateIncognitoSplitModeExtensions(
    ServiceWorkerTaskQueue* other) {
  DCHECK(browser_context_->IsOffTheRecord())
      << "Only need to activate split mode extensions for an OTR context";
  for (const auto& activated : other->activation_tokens_) {
    ExtensionRegistry* registry = ExtensionRegistry::Get(browser_context_);
    DCHECK(registry);
    const Extension* extension =
        registry->enabled_extensions().GetByID(activated.first);
    if (extension && IncognitoInfo::IsSplitMode(extension)) {
      ActivateExtension(extension);
    }
  }
}

}  // namespace extensions
