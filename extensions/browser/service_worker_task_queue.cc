// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/service_worker_task_queue.h"

#include <memory>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/syslog_logging.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/console_message.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_error.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/renderer_startup_helper.h"
#include "extensions/browser/service_worker_task_queue_factory.h"
#include "extensions/common/constants.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/common/manifest_handlers/incognito_info.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
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

ServiceWorkerTaskQueue::TestObserver* g_test_observer = nullptr;

// ServiceWorkerRegistration state of an activated extension.
enum class RegistrationState {
  // Not registered.
  kNotRegistered,
  // Registration is inflight.
  kPending,
  // Registration is complete.
  kRegistered,
};

// Browser process worker state of an activated extension.
enum class BrowserState {
  // Initial state, not started.
  kInitial,
  // Worker is in the process of starting from the browser process.
  kStarting,
  // Worker has completed starting (i.e. has seen DidStartWorkerForScope).
  kStarted,
};

// Render process worker state of an activated extension.
enum class RendererState {
  // Initial state, neither started nor stopped.
  kInitial,
  // Worker thread has started.
  kStarted,
  // Worker thread has not started or has been stopped.
  kStopped,
};

}  // namespace

ServiceWorkerTaskQueue::ServiceWorkerTaskQueue(BrowserContext* browser_context)
    : browser_context_(browser_context) {}

ServiceWorkerTaskQueue::~ServiceWorkerTaskQueue() {
  for (auto* const service_worker_context : observing_worker_contexts_)
    service_worker_context->RemoveObserver(this);
}

ServiceWorkerTaskQueue::TestObserver::TestObserver() = default;

ServiceWorkerTaskQueue::TestObserver::~TestObserver() = default;

// static
ServiceWorkerTaskQueue* ServiceWorkerTaskQueue::Get(BrowserContext* context) {
  return ServiceWorkerTaskQueueFactory::GetForBrowserContext(context);
}

// The current worker related state of an activated extension.
class ServiceWorkerTaskQueue::WorkerState {
 public:
  WorkerState() = default;

  WorkerState(const WorkerState&) = delete;
  WorkerState& operator=(const WorkerState&) = delete;

  void SetWorkerId(const WorkerId& worker_id, ProcessManager* process_manager) {
    if (worker_id_ && *worker_id_ != worker_id) {
      // Sanity check that the old worker is gone.
      DCHECK(!process_manager->HasServiceWorker(*worker_id_));
      // Clear stale renderer state if there's any.
      renderer_state_ = RendererState::kInitial;
    }
    worker_id_ = worker_id;
  }

  bool ready() const {
    return registration_state_ == RegistrationState::kRegistered &&
           browser_state_ == BrowserState::kStarted &&
           renderer_state_ == RendererState::kStarted && worker_id_.has_value();
  }
  bool has_pending_tasks() const { return !pending_tasks_.empty(); }

 private:
  friend class ServiceWorkerTaskQueue;

  RegistrationState registration_state_ = RegistrationState::kNotRegistered;
  BrowserState browser_state_ = BrowserState::kInitial;
  RendererState renderer_state_ = RendererState::kInitial;

  // Pending tasks that will be run once the worker becomes ready.
  std::vector<PendingTask> pending_tasks_;

  // Contains the worker's WorkerId associated with this WorkerState, once we
  // have discovered info about the worker.
  std::optional<WorkerId> worker_id_;
};

void ServiceWorkerTaskQueue::DidStartWorkerForScope(
    const SequencedContextId& context_id,
    base::Time start_time,
    int64_t version_id,
    int process_id,
    int thread_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const ExtensionId& extension_id = context_id.first.extension_id();
  const base::UnguessableToken& activation_token = context_id.second;
  if (!IsCurrentActivation(extension_id, activation_token)) {
    // Extension run with |activation_token| was already deactivated.
    // TODO(lazyboy): Add a DCHECK that the worker in question is actually
    // shutting down soon.
    DCHECK(!GetWorkerState(context_id));
    return;
  }

  // HACK: The service worker layer might invoke this callback with an ID for a
  // RenderProcessHost that has already terminated. This isn't the right fix for
  // this, because it results in the internal state here stalling out - we'll
  // wait on the browser side to be ready, which will never happen. This should
  // be cleaned up on the next activation sequence, but this still isn't good.
  // The proper fix here is that the service worker layer shouldn't be invoking
  // this callback with stale processes.
  // https://crbug.com/1335821.
  if (!content::RenderProcessHost::FromID(process_id)) {
    // This is definitely hit, and often enough that we can't NOTREACHED(),
    // CHECK(), or DumpWithoutCrashing(). Instead, log an error and gracefully
    // return.
    // TODO(https://crbug.com/1447448): Investigate and fix.
    LOG(ERROR) << "Received bad DidStartWorkerForScope() message. "
                  "No corresponding RenderProcessHost.";
    return;
  }

  UMA_HISTOGRAM_BOOLEAN("Extensions.ServiceWorkerBackground.StartWorkerStatus",
                        true);
  UMA_HISTOGRAM_TIMES("Extensions.ServiceWorkerBackground.StartWorkerTime",
                      base::Time::Now() - start_time);

  WorkerState* worker_state = GetWorkerState(context_id);
  DCHECK(worker_state);
  const WorkerId worker_id = {extension_id, process_id, version_id, thread_id};

  // Note: If the worker has already stopped on worker thread
  // (DidStopServiceWorkerContext) before we got here (i.e. the browser has
  // finished starting the worker), then |worker_state_map_| will hold the
  // worker until deactivation.
  // TODO(lazyboy): We need to ensure that the worker is not stopped in the
  // renderer before we execute tasks in the browser process. This will also
  // avoid holding the worker in |worker_state_map_| until deactivation as noted
  // above.
  DCHECK_NE(BrowserState::kStarted, worker_state->browser_state_)
      << "Worker was already loaded";
  worker_state->SetWorkerId(worker_id, ProcessManager::Get(browser_context_));
  worker_state->browser_state_ = BrowserState::kStarted;

  RunPendingTasksIfWorkerReady(context_id);
}

void ServiceWorkerTaskQueue::DidStartWorkerFail(
    const SequencedContextId& context_id,
    base::Time start_time,
    blink::ServiceWorkerStatusCode status_code) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!IsCurrentActivation(context_id.first.extension_id(),
                           context_id.second)) {
    // This can happen is when the registration got unregistered right before we
    // tried to start it. See crbug.com/999027 for details.
    DCHECK(!GetWorkerState(context_id));
    return;
  }

  UMA_HISTOGRAM_BOOLEAN("Extensions.ServiceWorkerBackground.StartWorkerStatus",
                        false);
  UMA_HISTOGRAM_ENUMERATION(
      "Extensions.ServiceWorkerBackground.StartWorker_FailStatus", status_code);
  UMA_HISTOGRAM_TIMES("Extensions.ServiceWorkerBackground.StartWorkerTime_Fail",
                      base::Time::Now() - start_time);

  WorkerState* worker_state = GetWorkerState(context_id);
  DCHECK(worker_state);
  if (g_test_observer) {
    g_test_observer->DidStartWorkerFail(context_id.first.extension_id(),
                                        worker_state->pending_tasks_.size(),
                                        status_code);
  }
  worker_state->pending_tasks_.clear();
  // TODO(https://crbug/1062936): Needs more thought: extension would be in
  // perma-broken state after this as the registration wouldn't be stored if
  // this happens.
  LOG(ERROR)
      << "DidStartWorkerFail " << context_id.first.extension_id() << ": "
      << static_cast<std::underlying_type_t<blink::ServiceWorkerStatusCode>>(
             status_code);

  // If there was a pending registration for this scope, erase it.
  pending_registrations_.erase(context_id.first.service_worker_scope());
}

void ServiceWorkerTaskQueue::DidInitializeServiceWorkerContext(
    int render_process_id,
    const ExtensionId& extension_id,
    int64_t service_worker_version_id,
    int thread_id) {
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
  ProcessManager::Get(browser_context_)
      ->RegisterServiceWorker({extension_id, render_process_id,
                               service_worker_version_id, thread_id});
  RendererStartupHelperFactory::GetForBrowserContext(browser_context_)
      ->ActivateExtensionInProcess(*extension, process_host);

  if (g_test_observer) {
    g_test_observer->DidInitializeServiceWorkerContext(extension_id);
  }
}

void ServiceWorkerTaskQueue::DidStartServiceWorkerContext(
    int render_process_id,
    const ExtensionId& extension_id,
    const base::UnguessableToken& activation_token,
    const GURL& service_worker_scope,
    int64_t service_worker_version_id,
    int thread_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!IsCurrentActivation(extension_id, activation_token)) {
    return;
  }

  SequencedContextId context_id(
      LazyContextId(browser_context_, extension_id, service_worker_scope),
      activation_token);

  const WorkerId worker_id = {extension_id, render_process_id,
                              service_worker_version_id, thread_id};
  WorkerState* worker_state = GetWorkerState(context_id);
  DCHECK(worker_state);
  // If |worker_state| had a worker running previously, for which we didn't
  // see DidStopServiceWorkerContext notification (typically happens on render
  // process shutdown), then we'd preserve stale state in |renderer_state_|.
  //
  // This isn't a problem because the next browser process readiness
  // (DidStartWorkerForScope) or the next renderer process readiness
  // (DidStartServiceWorkerContext) will clear the state, whichever happens
  // first.
  //
  // TODO(lazyboy): Update the renderer state in RenderProcessExited() and
  // uncomment the following DCHECK:
  // DCHECK_NE(RendererState::kStarted, worker_state->renderer_state_)
  //    << "Worker already started";
  worker_state->SetWorkerId(worker_id, ProcessManager::Get(browser_context_));
  worker_state->renderer_state_ = RendererState::kStarted;

  RunPendingTasksIfWorkerReady(context_id);
}

void ServiceWorkerTaskQueue::DidStopServiceWorkerContext(
    int render_process_id,
    const ExtensionId& extension_id,
    const base::UnguessableToken& activation_token,
    const GURL& service_worker_scope,
    int64_t service_worker_version_id,
    int thread_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!IsCurrentActivation(extension_id, activation_token)) {
    return;
  }

  const WorkerId worker_id = {extension_id, render_process_id,
                              service_worker_version_id, thread_id};
  ProcessManager::Get(browser_context_)->UnregisterServiceWorker(worker_id);
  SequencedContextId context_id(
      LazyContextId(browser_context_, extension_id, service_worker_scope),
      activation_token);

  WorkerState* worker_state = GetWorkerState(context_id);
  DCHECK(worker_state);

  if (worker_state->worker_id_ != worker_id) {
    // We can see DidStopServiceWorkerContext right after DidInitialize and
    // without DidStartServiceWorkerContext.
    return;
  }

  DCHECK_NE(RendererState::kStopped, worker_state->renderer_state_);
  worker_state->renderer_state_ = RendererState::kStopped;
  worker_state->worker_id_ = std::nullopt;
}

// static
void ServiceWorkerTaskQueue::SetObserverForTest(TestObserver* observer) {
  g_test_observer = observer;
}

bool ServiceWorkerTaskQueue::ShouldEnqueueTask(BrowserContext* context,
                                               const Extension* extension) {
  // We call StartWorker every time we want to dispatch an event to an extension
  // Service worker.
  // TODO(lazyboy): Is that a problem?
  return true;
}

void ServiceWorkerTaskQueue::AddPendingTask(
    const LazyContextId& lazy_context_id,
    PendingTask task) {
  DCHECK(lazy_context_id.is_for_service_worker());

  // TODO(lazyboy): Do we need to handle incognito context?

  auto activation_token =
      GetCurrentActivationToken(lazy_context_id.extension_id());
  DCHECK(activation_token)
      << "Trying to add pending task to an inactive extension: "
      << lazy_context_id.extension_id();
  const SequencedContextId context_id(lazy_context_id, *activation_token);
  WorkerState* worker_state = GetWorkerState(context_id);
  DCHECK(worker_state);
  auto& tasks = worker_state->pending_tasks_;
  bool needs_start_worker = tasks.empty();
  tasks.push_back(std::move(task));

  if (worker_state->registration_state_ != RegistrationState::kRegistered) {
    // If the worker hasn't finished registration, wait for it to complete.
    // DidRegisterServiceWorker will Start worker to run |task| later.
    return;
  }

  // Start worker if there isn't any request to start worker with |context_id|
  // is in progress.
  if (needs_start_worker)
    RunTasksAfterStartWorker(context_id);
}

void ServiceWorkerTaskQueue::ActivateExtension(const Extension* extension) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  const ExtensionId extension_id = extension->id();
  base::UnguessableToken activation_token = base::UnguessableToken::Create();
  activation_tokens_[extension_id] = activation_token;
  SequencedContextId context_id(
      LazyContextId(browser_context_, extension_id, extension->url()),
      activation_token);
  DCHECK(!base::Contains(worker_state_map_, context_id));
  WorkerState& worker_state = worker_state_map_[context_id];

  content::ServiceWorkerContext* service_worker_context =
      GetServiceWorkerContext(extension->id());
  StartObserving(service_worker_context);

  // Note: version.IsValid() = false implies we didn't have any prefs stored.
  base::Version version = RetrieveRegisteredServiceWorkerVersion(extension_id);
  const bool service_worker_already_registered =
      version.IsValid() && version == extension->version();
  if (g_test_observer) {
    g_test_observer->OnActivateExtension(extension_id,
                                         !service_worker_already_registered);
  }

  if (service_worker_already_registered) {
    worker_state.registration_state_ = RegistrationState::kRegistered;
    VerifyRegistration(service_worker_context, context_id, extension->url());
    return;
  }

  worker_state.registration_state_ = RegistrationState::kPending;

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

void ServiceWorkerTaskQueue::RegisterServiceWorker(
    RegistrationReason reason,
    const SequencedContextId& context_id,
    const Extension& extension) {
  GURL script_url = extension.GetResourceURL(
      BackgroundInfo::GetBackgroundServiceWorkerScript(&extension));
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
  SequencedContextId context_id(
      LazyContextId(browser_context_, extension_id, extension->url()),
      *activation_token);
  WorkerState* worker_state = GetWorkerState(context_id);
  DCHECK(worker_state);
  // TODO(lazyboy): Run orphaned tasks with nullptr ContextInfo.
  worker_state->pending_tasks_.clear();
  worker_state_map_.erase(context_id);

  // Erase any registrations that might still have been pending being fully
  // stored.
  pending_registrations_.erase(extension->url());

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
                     *activation_token));

  StopObserving(service_worker_context);
}

void ServiceWorkerTaskQueue::RunTasksAfterStartWorker(
    const SequencedContextId& context_id) {
  DCHECK(context_id.first.is_for_service_worker());

  const LazyContextId& lazy_context_id = context_id.first;
  if (lazy_context_id.browser_context() != browser_context_)
    return;

  WorkerState* worker_state = GetWorkerState(context_id);
  DCHECK_NE(BrowserState::kStarted, worker_state->browser_state_);

  content::ServiceWorkerContext* service_worker_context =
      GetServiceWorkerContext(lazy_context_id.extension_id());

  const GURL& scope = context_id.first.service_worker_scope();
  service_worker_context->StartWorkerForScope(
      scope, blink::StorageKey::CreateFirstParty(url::Origin::Create(scope)),
      base::BindOnce(&ServiceWorkerTaskQueue::DidStartWorkerForScope,
                     weak_factory_.GetWeakPtr(), context_id, base::Time::Now()),
      base::BindOnce(&ServiceWorkerTaskQueue::DidStartWorkerFail,
                     weak_factory_.GetWeakPtr(), context_id,
                     base::Time::Now()));
}

void ServiceWorkerTaskQueue::DidRegisterServiceWorker(
    const SequencedContextId& context_id,
    RegistrationReason reason,
    base::Time start_time,
    blink::ServiceWorkerStatusCode status_code) {
  ExtensionRegistry* registry = ExtensionRegistry::Get(browser_context_);
  const ExtensionId& extension_id = context_id.first.extension_id();
  DCHECK(registry);
  const Extension* extension =
      registry->enabled_extensions().GetByID(extension_id);
  if (!extension) {
    return;
  }
  if (!IsCurrentActivation(extension_id, context_id.second)) {
    return;
  }

  WorkerState* worker_state = GetWorkerState(context_id);
  DCHECK(worker_state);
  const bool success = status_code == blink::ServiceWorkerStatusCode::kOk;
  UMA_HISTOGRAM_BOOLEAN("Extensions.ServiceWorkerBackground.RegistrationStatus",
                        success);

  if (reason == RegistrationReason::RE_REGISTER_ON_STATE_MISMATCH) {
    UMA_HISTOGRAM_BOOLEAN(
        "Extensions.ServiceWorkerBackground.RegistrationMismatchMitigated",
        success);
    if (g_test_observer)
      g_test_observer->RegistrationMismatchMitigated(success);
  }

  if (!success) {
    std::string msg = base::StringPrintf(
        "Service worker registration failed. Status code: %d",
        static_cast<int>(status_code));
    auto error = std::make_unique<ManifestError>(
        extension_id, base::UTF8ToUTF16(msg), manifest_keys::kBackground,
        base::UTF8ToUTF16(
            BackgroundInfo::GetBackgroundServiceWorkerScript(extension)));

    ExtensionsBrowserClient::Get()->ReportError(browser_context_,
                                                std::move(error));
    return;
  }
  UMA_HISTOGRAM_TIMES("Extensions.ServiceWorkerBackground.RegistrationTime",
                      base::Time::Now() - start_time);

  worker_state->registration_state_ = RegistrationState::kRegistered;
  pending_registrations_.emplace(extension->url(),
                                 *GetCurrentActivationToken(extension->id()));

  if (worker_state->has_pending_tasks()) {
    // TODO(lazyboy): If worker for |context_id| is already running, consider
    // not calling StartWorker. This should be straightforward now that service
    // worker's internal state is on the UI thread rather than the IO thread.
    RunTasksAfterStartWorker(context_id);
  }
}

void ServiceWorkerTaskQueue::DidUnregisterServiceWorker(
    const ExtensionId& extension_id,
    const base::UnguessableToken& activation_token,
    bool success) {
  // Extension run with |activation_token| was already deactivated.
  if (!IsCurrentActivation(extension_id, activation_token)) {
    return;
  }

  // TODO(lazyboy): Handle success = false case.
  if (!success)
    LOG(ERROR) << "Failed to unregister service worker!";
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

void ServiceWorkerTaskQueue::RunPendingTasksIfWorkerReady(
    const SequencedContextId& context_id) {
  WorkerState* worker_state = GetWorkerState(context_id);
  DCHECK(worker_state);
  if (!worker_state->ready()) {
    // Worker isn't ready yet, wait for next event and run the tasks then.
    return;
  }

  // Running |pending_tasks_[context_id]| marks the completion of
  // DidStartWorkerForScope, clean up |browser_ready| state of the worker so
  // that new tasks can be queued up.
  worker_state->browser_state_ = BrowserState::kInitial;

  DCHECK(worker_state->has_pending_tasks())
      << "Worker ready, but no tasks to run!";
  std::vector<PendingTask> tasks;
  std::swap(worker_state->pending_tasks_, tasks);
  DCHECK(worker_state->worker_id_);
  const auto& worker_id = *worker_state->worker_id_;
  for (auto& task : tasks) {
    auto context_info = std::make_unique<LazyContextTaskQueue::ContextInfo>(
        context_id.first.extension_id(),
        content::RenderProcessHost::FromID(worker_id.render_process_id),
        worker_id.version_id, worker_id.thread_id,
        context_id.first.service_worker_scope());
    std::move(task).Run(std::move(context_info));
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

void ServiceWorkerTaskQueue::OnRegistrationStored(int64_t registration_id,
                                                  const GURL& scope) {
  auto iter = pending_registrations_.find(scope);
  if (iter == pending_registrations_.end()) {
    return;
  }

  // The only registrations we track are the ones for root-scope extension
  // service workers.
  DCHECK_EQ(kExtensionScheme, scope.scheme());
  DCHECK_EQ("/", scope.path());

  base::UnguessableToken activation_token = iter->second;
  pending_registrations_.erase(iter);

  std::string extension_id = scope.host();
  ExtensionRegistry* registry = ExtensionRegistry::Get(browser_context_);
  const Extension* extension =
      registry->enabled_extensions().GetByID(extension_id);

  // Check the extension's presence and current activation; this might be
  // different if the extension was [un|re]loaded.
  if (extension && IsCurrentActivation(extension_id, activation_token)) {
    SetRegisteredServiceWorkerInfo(extension->id(), extension->version());
  }
}

void ServiceWorkerTaskQueue::OnReportConsoleMessage(
    int64_t version_id,
    const GURL& scope,
    const content::ConsoleMessage& message) {
  if (message.message_level != blink::mojom::ConsoleMessageLevel::kError) {
    // We don't report certain low-severity errors.
    return;
  }

  auto error_instance = std::make_unique<RuntimeError>(
      scope.host(), browser_context_->IsOffTheRecord(),
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
      -1 /* TODO(crbug.com/1218812): Retrieve render_process_id */);

  ExtensionsBrowserClient::Get()->ReportError(browser_context_,
                                              std::move(error_instance));
}

void ServiceWorkerTaskQueue::OnDestruct(
    content::ServiceWorkerContext* context) {
  StopObserving(context);
}

size_t ServiceWorkerTaskQueue::GetNumPendingTasksForTest(
    const LazyContextId& lazy_context_id) {
  auto activation_token =
      GetCurrentActivationToken(lazy_context_id.extension_id());
  if (!activation_token) {
    return 0u;
  }
  const SequencedContextId context_id(lazy_context_id, *activation_token);
  WorkerState* worker_state = GetWorkerState(context_id);
  return worker_state ? worker_state->pending_tasks_.size() : 0u;
}

ServiceWorkerTaskQueue::WorkerState* ServiceWorkerTaskQueue::GetWorkerState(
    const SequencedContextId& context_id) {
  auto worker_iter = worker_state_map_.find(context_id);
  return worker_iter == worker_state_map_.end() ? nullptr
                                                : &worker_iter->second;
}

content::ServiceWorkerContext* ServiceWorkerTaskQueue::GetServiceWorkerContext(
    const ExtensionId& extension_id) {
  return util::GetServiceWorkerContextForExtensionId(extension_id,
                                                     browser_context_);
}

void ServiceWorkerTaskQueue::StartObserving(
    content::ServiceWorkerContext* service_worker_context) {
  if (observing_worker_contexts_.count(service_worker_context) == 0u)
    service_worker_context->AddObserver(this);
  observing_worker_contexts_.insert(service_worker_context);
}

void ServiceWorkerTaskQueue::StopObserving(
    content::ServiceWorkerContext* service_worker_context) {
  auto iter_pair =
      observing_worker_contexts_.equal_range(service_worker_context);
  // ServiceWorkerContext not found if the iterators are equal.
  if (iter_pair.first == iter_pair.second) {
    return;
  }
  // If the distance is 1, it means there is just one instance of the observing
  // ServiceWorkerContext remaining so we also remove the
  // ServiceWorkerContextObserver (this) from the observing
  // ServiceWorkerContext.
  if (std::distance(iter_pair.first, iter_pair.second) == 1) {
    service_worker_context->RemoveObserver(this);
  }
  observing_worker_contexts_.erase(iter_pair.first);
}

void ServiceWorkerTaskQueue::DidVerifyRegistration(
    const SequencedContextId& context_id,
    content::ServiceWorkerCapability capability) {
  const bool is_registered =
      capability != content::ServiceWorkerCapability::NO_SERVICE_WORKER;
  UMA_HISTOGRAM_BOOLEAN(
      "Extensions.ServiceWorkerBackground.RegistrationWhenExpected",
      is_registered);

  if (is_registered)
    return;

  // We expected a SW registration (as ExtensionPrefs said so), but there isn't
  // one. Re-register SW script if the extension is still installed (it's
  // possible it was uninstalled while we were checking).
  const ExtensionId& extension_id = context_id.first.extension_id();
  ExtensionRegistry* registry = ExtensionRegistry::Get(browser_context_);
  DCHECK(registry);
  const Extension* extension =
      registry->enabled_extensions().GetByID(extension_id);
  if (!extension)
    return;

  UMA_HISTOGRAM_ENUMERATION(
      "Extensions.ServiceWorkerBackground.RegistrationMismatchLocation",
      extension->location());

  RegisterServiceWorker(RegistrationReason::RE_REGISTER_ON_STATE_MISMATCH,
                        context_id, *extension);
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
    if (extension && IncognitoInfo::IsSplitMode(extension))
      ActivateExtension(extension);
  }
}

}  // namespace extensions
