// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/service_worker_task_queue.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/task/post_task.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/service_worker_task_queue_factory.h"
#include "extensions/common/constants.h"
#include "extensions/common/manifest_handlers/background_info.h"

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

}  // namespace

ServiceWorkerTaskQueue::ServiceWorkerTaskQueue(BrowserContext* browser_context)
    : browser_context_(browser_context) {}

ServiceWorkerTaskQueue::~ServiceWorkerTaskQueue() {}

ServiceWorkerTaskQueue::TestObserver::TestObserver() {}

ServiceWorkerTaskQueue::TestObserver::~TestObserver() {}

// static
ServiceWorkerTaskQueue* ServiceWorkerTaskQueue::Get(BrowserContext* context) {
  return ServiceWorkerTaskQueueFactory::GetForBrowserContext(context);
}

// static
void ServiceWorkerTaskQueue::DidStartWorkerForScopeOnCoreThread(
    const SequencedContextId& context_id,
    base::WeakPtr<ServiceWorkerTaskQueue> task_queue,
    int64_t version_id,
    int process_id,
    int thread_id) {
  DCHECK_CURRENTLY_ON(content::ServiceWorkerContext::GetCoreThreadId());
  if (content::ServiceWorkerContext::IsServiceWorkerOnUIEnabled()) {
    if (task_queue) {
      task_queue->DidStartWorkerForScope(context_id, version_id, process_id,
                                         thread_id);
    }
  } else {
    base::PostTask(
        FROM_HERE, {content::BrowserThread::UI},
        base::BindOnce(&ServiceWorkerTaskQueue::DidStartWorkerForScope,
                       task_queue, context_id, version_id, process_id,
                       thread_id));
  }
}

// static
void ServiceWorkerTaskQueue::DidStartWorkerFailOnCoreThread(
    const SequencedContextId& context_id,
    base::WeakPtr<ServiceWorkerTaskQueue> task_queue) {
  DCHECK_CURRENTLY_ON(content::ServiceWorkerContext::GetCoreThreadId());
  if (content::ServiceWorkerContext::IsServiceWorkerOnUIEnabled()) {
    if (task_queue)
      task_queue->DidStartWorkerFail(context_id);
  } else {
    base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                   base::BindOnce(&ServiceWorkerTaskQueue::DidStartWorkerFail,
                                  task_queue, context_id));
  }
}

// static
void ServiceWorkerTaskQueue::StartServiceWorkerOnCoreThreadToRunTasks(
    base::WeakPtr<ServiceWorkerTaskQueue> task_queue_weak,
    const SequencedContextId& context_id,
    content::ServiceWorkerContext* service_worker_context) {
  DCHECK_CURRENTLY_ON(content::ServiceWorkerContext::GetCoreThreadId());
  service_worker_context->StartWorkerForScope(
      context_id.first.service_worker_scope(),
      base::BindOnce(
          &ServiceWorkerTaskQueue::DidStartWorkerForScopeOnCoreThread,
          context_id, task_queue_weak),
      base::BindOnce(&ServiceWorkerTaskQueue::DidStartWorkerFailOnCoreThread,
                     context_id, task_queue_weak));
}

// The current state of a worker.
struct ServiceWorkerTaskQueue::WorkerState {
  // Whether or not worker has completed starting (DidStartWorkerForScope).
  bool browser_ready = false;

  // Whether or not worker is ready in the renderer
  // (DidStartServiceWorkerContext).
  bool renderer_ready = false;

  // If |browser_ready| = true, this is the ActivationSequence of the worker.
  base::Optional<ActivationSequence> sequence;

  WorkerState() = default;
};

void ServiceWorkerTaskQueue::DidStartWorkerForScope(
    const SequencedContextId& context_id,
    int64_t version_id,
    int process_id,
    int thread_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const ExtensionId& extension_id = context_id.first.extension_id();
  const ActivationSequence& sequence = context_id.second;
  if (!IsCurrentSequence(extension_id, sequence)) {
    // Extension run with |sequence| was already deactivated.
    // TODO(lazyboy): Add a DCHECK that the worker in question is actually
    // shutting down soon.
    DCHECK(!base::Contains(pending_tasks_, context_id));
    return;
  }

  const LazyContextId& lazy_context_id = context_id.first;
  const WorkerId worker_id = {extension_id, process_id, version_id, thread_id};

  // Note: If the worker has already stopped on worker thread
  // (DidStopServiceWorkerContext) before we got here (i.e. the browser has
  // finished starting the worker), then |worker_state_map_| will hold the
  // worker until deactivation.
  // TODO(lazyboy): We need to ensure that the worker is not stopped in the
  // renderer before we execute tasks in the browser process. This will also
  // avoid holding the worker in |worker_state_map_| until deactivation as noted
  // above.
  WorkerState* worker_state =
      GetOrCreateWorkerState(WorkerKey(lazy_context_id, worker_id));
  DCHECK(worker_state);
  DCHECK(!worker_state->browser_ready) << "Worker was already loaded";
  worker_state->browser_ready = true;
  worker_state->sequence = sequence;

  RunPendingTasksIfWorkerReady(lazy_context_id, version_id, process_id,
                               thread_id);
}

void ServiceWorkerTaskQueue::DidStartWorkerFail(
    const SequencedContextId& context_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!IsCurrentSequence(context_id.first.extension_id(), context_id.second)) {
    // This can happen is when the registration got unregistered right before we
    // tried to start it. See crbug.com/999027 for details.
    DCHECK(!base::Contains(pending_tasks_, context_id));
    return;
  }
  // TODO(lazyboy): Handle failure cases.
  DCHECK(false) << "DidStartWorkerFail: " << context_id.first.extension_id();
}

void ServiceWorkerTaskQueue::DidInitializeServiceWorkerContext(
    int render_process_id,
    const ExtensionId& extension_id,
    int64_t service_worker_version_id,
    int thread_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  ProcessManager::Get(browser_context_)
      ->RegisterServiceWorker({extension_id, render_process_id,
                               service_worker_version_id, thread_id});
}

void ServiceWorkerTaskQueue::DidStartServiceWorkerContext(
    int render_process_id,
    const ExtensionId& extension_id,
    const GURL& service_worker_scope,
    int64_t service_worker_version_id,
    int thread_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  LazyContextId context_id(browser_context_, extension_id,
                           service_worker_scope);
  const WorkerId worker_id = {extension_id, render_process_id,
                              service_worker_version_id, thread_id};
  WorkerState* worker_state =
      GetOrCreateWorkerState(WorkerKey(context_id, worker_id));
  DCHECK(!worker_state->renderer_ready) << "Worker already started";
  worker_state->renderer_ready = true;

  RunPendingTasksIfWorkerReady(context_id, service_worker_version_id,
                               render_process_id, thread_id);
}

void ServiceWorkerTaskQueue::DidStopServiceWorkerContext(
    int render_process_id,
    const ExtensionId& extension_id,
    const GURL& service_worker_scope,
    int64_t service_worker_version_id,
    int thread_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const WorkerId worker_id = {extension_id, render_process_id,
                              service_worker_version_id, thread_id};
  ProcessManager::Get(browser_context_)->UnregisterServiceWorker(worker_id);
  LazyContextId context_id(browser_context_, extension_id,
                           service_worker_scope);

  WorkerKey worker_key(context_id, worker_id);
  WorkerState* worker_state = GetWorkerState(worker_key);
  if (!worker_state) {
    // We can see DidStopServiceWorkerContext right after DidInitialize and
    // without DidStartServiceWorkerContext.
    return;
  }

  // Clean up both the renderer and browser readiness states.
  // One caveat is that although this is renderer notification, we also clear
  // the browser readiness state, this is because a worker can be
  // |browser_ready| and was waiting for DidStartServiceWorkerContext, but
  // instead received DidStopServiceWorkerContext.
  worker_state_map_.erase(worker_key);
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

  auto sequence = GetCurrentSequence(lazy_context_id.extension_id());
  DCHECK(sequence) << "Trying to add pending task to an inactive extension: "
                   << lazy_context_id.extension_id();
  const SequencedContextId context_id(lazy_context_id, *sequence);
  auto& tasks = pending_tasks_[context_id];
  bool needs_start_worker = tasks.empty();
  tasks.push_back(std::move(task));

  if (pending_registrations_.count(context_id) > 0) {
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
  ActivationSequence current_sequence = ++next_activation_sequence_;
  activation_sequences_[extension_id] = current_sequence;

  // Note: version.IsValid() = false implies we didn't have any prefs stored.
  base::Version version = RetrieveRegisteredServiceWorkerVersion(extension_id);
  const bool service_worker_already_registered =
      version.IsValid() && version == extension->version();
  if (g_test_observer) {
    g_test_observer->OnActivateExtension(extension_id,
                                         !service_worker_already_registered);
  }

  if (service_worker_already_registered) {
    // TODO(https://crbug.com/901101): We should kick off an async check to see
    // if the registration is *actually* there and re-register if necessary.
    return;
  }

  SequencedContextId context_id(
      LazyContextId(browser_context_, extension_id, extension->url()),
      current_sequence);
  pending_registrations_.insert(context_id);
  GURL script_url = extension->GetResourceURL(
      BackgroundInfo::GetBackgroundServiceWorkerScript(extension));
  blink::mojom::ServiceWorkerRegistrationOptions option;
  option.scope = extension->url();
  content::BrowserContext::GetStoragePartitionForSite(browser_context_,
                                                      extension->url())
      ->GetServiceWorkerContext()
      ->RegisterServiceWorker(
          script_url, option,
          base::BindOnce(&ServiceWorkerTaskQueue::DidRegisterServiceWorker,
                         weak_factory_.GetWeakPtr(), context_id));
}

void ServiceWorkerTaskQueue::DeactivateExtension(const Extension* extension) {
  GURL script_url = extension->GetResourceURL(
      BackgroundInfo::GetBackgroundServiceWorkerScript(extension));
  const ExtensionId extension_id = extension->id();
  RemoveRegisteredServiceWorkerInfo(extension_id);
  base::Optional<ActivationSequence> sequence =
      GetCurrentSequence(extension_id);

  // Extension was never activated, this happens in tests.
  if (!sequence)
    return;

  SequencedContextId context_id(
      LazyContextId(browser_context_, extension_id, extension->url()),
      *sequence);
  ClearPendingTasks(context_id);

  // Clear loaded worker if it was waiting for start.
  // Note that we don't clear the entire state here as we expect the renderer to
  // stop shortly after this and its notification will clear the state.
  ClearBrowserReadyForWorkers(
      LazyContextId(browser_context_, extension_id, extension->url()),
      *sequence);

  content::BrowserContext::GetStoragePartitionForSite(browser_context_,
                                                      extension->url())
      ->GetServiceWorkerContext()
      ->UnregisterServiceWorker(
          extension->url(),
          base::BindOnce(&ServiceWorkerTaskQueue::DidUnregisterServiceWorker,
                         weak_factory_.GetWeakPtr(), extension_id));
}

void ServiceWorkerTaskQueue::RunTasksAfterStartWorker(
    const SequencedContextId& context_id) {
  DCHECK(context_id.first.is_for_service_worker());

  const LazyContextId& lazy_context_id = context_id.first;
  if (lazy_context_id.browser_context() != browser_context_)
    return;

  content::StoragePartition* partition =
      BrowserContext::GetStoragePartitionForSite(
          lazy_context_id.browser_context(),
          lazy_context_id.service_worker_scope());
  content::ServiceWorkerContext* service_worker_context =
      partition->GetServiceWorkerContext();

  if (content::ServiceWorkerContext::IsServiceWorkerOnUIEnabled()) {
    StartServiceWorkerOnCoreThreadToRunTasks(
        weak_factory_.GetWeakPtr(), context_id, service_worker_context);
  } else {
    content::ServiceWorkerContext::RunTask(
        base::CreateSingleThreadTaskRunner({content::BrowserThread::IO}),
        FROM_HERE, service_worker_context,
        base::BindOnce(
            &ServiceWorkerTaskQueue::StartServiceWorkerOnCoreThreadToRunTasks,
            weak_factory_.GetWeakPtr(), context_id, service_worker_context));
  }
}

void ServiceWorkerTaskQueue::DidRegisterServiceWorker(
    const SequencedContextId& context_id,
    bool success) {
  pending_registrations_.erase(context_id);
  ExtensionRegistry* registry = ExtensionRegistry::Get(browser_context_);
  const ExtensionId& extension_id = context_id.first.extension_id();
  DCHECK(registry);
  const Extension* extension =
      registry->enabled_extensions().GetByID(extension_id);
  if (!extension) {
    // DeactivateExtension must have cleared |pending_tasks_| already.
    DCHECK(!base::Contains(pending_tasks_, context_id));
    return;
  }

  if (!success) {
    if (!IsCurrentSequence(extension_id, context_id.second)) {
      // DeactivateExtension must have cleared |pending_tasks_| already.
      DCHECK(!base::Contains(pending_tasks_, context_id));
      return;
    }
    // TODO(lazyboy): Handle failure case thoroughly.
    DCHECK(false) << "Failed to register Service Worker";
    return;
  }

  SetRegisteredServiceWorkerInfo(extension->id(), extension->version());

  auto pending_tasks_iter = pending_tasks_.find(context_id);
  const bool has_pending_tasks = pending_tasks_iter != pending_tasks_.end() &&
                                 pending_tasks_iter->second.size() > 0u;
  if (has_pending_tasks) {
    // TODO(lazyboy): If worker for |context_id| is already running, consider
    // not calling StartWorker. This isn't straightforward as service worker's
    // internal state is mostly on the core thread.
    RunTasksAfterStartWorker(context_id);
  }
}

void ServiceWorkerTaskQueue::DidUnregisterServiceWorker(
    const ExtensionId& extension_id,
    bool success) {
  // TODO(lazyboy): Handle success = false case.
  if (!success)
    LOG(ERROR) << "Failed to unregister service worker!";
}

base::Version ServiceWorkerTaskQueue::RetrieveRegisteredServiceWorkerVersion(
    const ExtensionId& extension_id) {
  std::string version_string;
  if (browser_context_->IsOffTheRecord()) {
    auto it = off_the_record_registrations_.find(extension_id);
    return it != off_the_record_registrations_.end() ? it->second
                                                     : base::Version();
  }
  const base::DictionaryValue* info = nullptr;
  ExtensionPrefs::Get(browser_context_)
      ->ReadPrefAsDictionary(extension_id, kPrefServiceWorkerRegistrationInfo,
                             &info);
  if (info != nullptr) {
    info->GetString(kServiceWorkerVersion, &version_string);
  }

  return base::Version(version_string);
}

void ServiceWorkerTaskQueue::SetRegisteredServiceWorkerInfo(
    const ExtensionId& extension_id,
    const base::Version& version) {
  DCHECK(version.IsValid());
  if (browser_context_->IsOffTheRecord()) {
    off_the_record_registrations_[extension_id] = version;
  } else {
    auto info = std::make_unique<base::DictionaryValue>();
    info->SetString(kServiceWorkerVersion, version.GetString());
    ExtensionPrefs::Get(browser_context_)
        ->UpdateExtensionPref(extension_id, kPrefServiceWorkerRegistrationInfo,
                              std::move(info));
  }
}

void ServiceWorkerTaskQueue::RemoveRegisteredServiceWorkerInfo(
    const ExtensionId& extension_id) {
  if (browser_context_->IsOffTheRecord()) {
    off_the_record_registrations_.erase(extension_id);
  } else {
    ExtensionPrefs::Get(browser_context_)
        ->UpdateExtensionPref(extension_id, kPrefServiceWorkerRegistrationInfo,
                              nullptr);
  }
}

void ServiceWorkerTaskQueue::RunPendingTasksIfWorkerReady(
    const LazyContextId& context_id,
    int64_t version_id,
    int process_id,
    int thread_id) {
  WorkerState* worker_state = GetWorkerState(WorkerKey(
      context_id,
      {context_id.extension_id(), process_id, version_id, thread_id}));
  DCHECK(worker_state);
  if (!worker_state->browser_ready || !worker_state->renderer_ready) {
    // Worker isn't ready yet, wait for next event and run the tasks then.
    return;
  }
  base::Optional<int> sequence = worker_state->sequence;
  DCHECK(sequence.has_value());

  // Running |pending_tasks_[context_id]| marks the completion of
  // DidStartWorkerForScope, clean up |browser_ready| state of the worker so
  // that new tasks can be queued up.
  worker_state->browser_ready = false;

  auto iter = pending_tasks_.find(SequencedContextId(context_id, *sequence));
  DCHECK(iter != pending_tasks_.end()) << "Worker ready, but no tasks to run!";
  std::vector<PendingTask> tasks = std::move(iter->second);
  pending_tasks_.erase(iter);
  for (auto& task : tasks) {
    auto context_info = std::make_unique<LazyContextTaskQueue::ContextInfo>(
        context_id.extension_id(),
        content::RenderProcessHost::FromID(process_id), version_id, thread_id,
        context_id.service_worker_scope());
    std::move(task).Run(std::move(context_info));
  }
}

void ServiceWorkerTaskQueue::ClearPendingTasks(
    const SequencedContextId& context_id) {
  // TODO(lazyboy): Run orphaned tasks with nullptr ContextInfo.
  pending_tasks_.erase(context_id);
}

bool ServiceWorkerTaskQueue::IsCurrentSequence(
    const ExtensionId& extension_id,
    ActivationSequence sequence) const {
  auto current_sequence = GetCurrentSequence(extension_id);
  return current_sequence == sequence;
}

base::Optional<ServiceWorkerTaskQueue::ActivationSequence>
ServiceWorkerTaskQueue::GetCurrentSequence(
    const ExtensionId& extension_id) const {
  auto iter = activation_sequences_.find(extension_id);
  if (iter == activation_sequences_.end())
    return base::nullopt;
  return iter->second;
}

ServiceWorkerTaskQueue::WorkerState*
ServiceWorkerTaskQueue::GetOrCreateWorkerState(const WorkerKey& worker_key) {
  auto iter = worker_state_map_.find(worker_key);
  if (iter == worker_state_map_.end())
    iter = worker_state_map_.emplace(worker_key, WorkerState()).first;
  return &(iter->second);
}

ServiceWorkerTaskQueue::WorkerState* ServiceWorkerTaskQueue::GetWorkerState(
    const WorkerKey& worker_key) {
  auto iter = worker_state_map_.find(worker_key);
  if (iter == worker_state_map_.end())
    return nullptr;
  return &(iter->second);
}

void ServiceWorkerTaskQueue::ClearBrowserReadyForWorkers(
    const LazyContextId& context_id,
    ActivationSequence sequence) {
  // TODO(lazyboy): We could use |worker_state_map_|.lower_bound() to avoid
  // iterating over all workers. Note that it would require creating artificial
  // WorkerKey with |context_id|.
  for (auto iter = worker_state_map_.begin();
       iter != worker_state_map_.end();) {
    if (iter->first.first != context_id || iter->second.sequence != sequence) {
      ++iter;
      continue;
    }

    iter->second.browser_ready = false;
    iter->second.sequence = base::nullopt;

    // Clean up stray entries if renderer readiness was also gone.
    if (!iter->second.renderer_ready)
      iter = worker_state_map_.erase(iter);
    else
      ++iter;
  }
}

}  // namespace extensions
