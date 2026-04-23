// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/service_worker/service_worker_state.h"

#include "base/debug/alias.h"
#include "base/debug/dump_without_crashing.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/child_process_id.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/service_worker/worker_id.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_features.h"

namespace extensions {

namespace {

// Prevent check on multiple workers per extension for testing purposes.
bool g_allow_multiple_workers_per_extension = false;

// NOTE: These values are persisted to logs. Entries should not be renumbered
// and numeric values should never be reused.
enum class WorkerVersionIdState {
  kNewVersionIsOlder = 0,  // The newly received `version_id` is older.
  kNewVersionIsEqual = 1,  // The newly received `version_id` is equal.
  kNewVersionIsNewer = 2,  // The newly received `version_id` is newer.
  kMaxValue = kNewVersionIsNewer,
};

void RecordWorkerVersionIdStateHistogram(int64_t new_version_id,
                                         int64_t preexisting_version_id) {
  WorkerVersionIdState state = WorkerVersionIdState::kNewVersionIsEqual;
  if (new_version_id < preexisting_version_id) {
    state = WorkerVersionIdState::kNewVersionIsOlder;
  } else if (new_version_id > preexisting_version_id) {
    state = WorkerVersionIdState::kNewVersionIsNewer;
  }
  base::UmaHistogramEnumeration(
      "Extensions.ServiceWorkerBackground.WorkerVersionIdState_OnInitialized_"
      "NotActive",
      state);
}

}  // namespace

ServiceWorkerState::ServiceWorkerState(
    content::ServiceWorkerContext* service_worker_context,
    const ProcessManager* process_manager)
    : service_worker_context_(service_worker_context),
      process_manager_(process_manager) {
  service_worker_context_observation_.Observe(service_worker_context_);
}

ServiceWorkerState::~ServiceWorkerState() = default;

void ServiceWorkerState::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ServiceWorkerState::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ServiceWorkerState::SetBrowserState(BrowserState browser_state) {
  browser_state_ = browser_state;
}

void ServiceWorkerState::SetRendererState(RendererState renderer_state) {
  renderer_state_ = renderer_state;
}

void ServiceWorkerState::Reset() {
  worker_id_.reset();
  browser_state_ = BrowserState::kNotActive;
  renderer_state_ = RendererState::kNotActive;

  // NOTE: `worker_starting_` is intentionally NOT reset here.
  //
  // `Reset()` can be called when a worker stops, including when it stops in the
  // middle of a start attempt. In that case, `content::ServiceWorkerVersion`
  // may try to restart the worker to fulfill the pending start request(s)
  // (see `content::ServiceWorkerVersion::OnStoppedInternal`).
  //
  // `worker_starting_` must remain true to prevent the extensions layer from
  // issuing a new start request while the content layer's automatic restart
  // is in flight. Resetting it here can create a race condition where two
  // `DidStartWorkerForScope` callbacks are processed concurrently, leading
  // to a crash (see crbug.com/452178846).
  //
  // The flag is correctly cleared only upon success (in
  // `NotifyObserversIfReady`) or failure (in `DidStartWorkerFail`).
}

bool ServiceWorkerState::IsStarting() const {
  return worker_starting_;
}

bool ServiceWorkerState::IsReady() const {
  return browser_state_ == BrowserState::kActive &&
         renderer_state_ == RendererState::kActive && worker_id_.has_value();
}

void ServiceWorkerState::SetWorkerId(const WorkerId& worker_id) {
  if (worker_id_ && *worker_id_ != worker_id) {
    // Sanity check that the old worker is gone.
    // TODO(crbug.com/40936639): remove
    // `g_allow_multiple_workers_per_extension` once bug is fixed so that this
    // DCHECK() will be default behavior everywhere. Also upgrade to a CHECK
    // once the bug is completely fixed.
    DCHECK(!process_manager_->HasServiceWorker(*worker_id_) ||
           g_allow_multiple_workers_per_extension);

    // Clear stale renderer state if there's any.
    renderer_state_ = RendererState::kNotActive;
  }

  worker_id_ = worker_id;
  CHECK(worker_id_->start_token);
}

void ServiceWorkerState::StartWorker(const SequencedContextId& context_id) {
  CHECK(!IsReady());
  if (worker_starting_) {
    return;
  }
  worker_starting_ = true;

  const GURL scope =
      Extension::GetServiceWorkerScopeFromExtensionId(context_id.extension_id);

  service_worker_context_->StartWorkerForScope(
      scope, blink::StorageKey::CreateFirstParty(url::Origin::Create(scope)),
      base::BindOnce(&ServiceWorkerState::DidStartWorkerForScope,
                     weak_factory_.GetWeakPtr(), context_id, base::Time::Now()),
      base::BindOnce(&ServiceWorkerState::DidStartWorkerFail,
                     weak_factory_.GetWeakPtr(), context_id,
                     base::Time::Now()));
}

void ServiceWorkerState::DidStartWorkerForScope(
    const SequencedContextId& context_id,
    base::Time start_time,
    int64_t version_id,
    content::ChildProcessId process_id,
    int thread_id,
    const blink::ServiceWorkerToken& token) {
  UMA_HISTOGRAM_BOOLEAN("Extensions.ServiceWorkerBackground.StartWorkerStatus",
                        true);
  UMA_HISTOGRAM_TIMES("Extensions.ServiceWorkerBackground.StartWorkerTime",
                      base::Time::Now() - start_time);

  DCHECK_NE(BrowserState::kActive, browser_state())
      << "Worker was already loaded";

  const ExtensionId& extension_id = context_id.extension_id;
  const WorkerId worker_id = {extension_id, process_id, version_id, thread_id,
                              token};

  if (!service_worker_context_->IsLiveServiceWorkerWithToken(version_id,
                                                             token)) {
    // Drop the IPC message. It is from a stale worker instance.
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
  if (!content::RenderProcessHost::FromID(worker_id.render_process_id)) {
    // The IsLiveServiceWorkerWithToken() check above *should* have caught
    // this instance.
    base::debug::DumpWithoutCrashing();
    // TODO(crbug.com/40913640): Investigate and fix.
    LOG(ERROR) << "Received bad DidStartWorkerForScope() message. "
                  "No corresponding RenderProcessHost.";
    return;
  }

  SetWorkerId(worker_id);
  SetBrowserState(BrowserState::kActive);
  NotifyObserversIfReady(context_id);
}

void ServiceWorkerState::DidStartWorkerFail(
    const SequencedContextId& context_id,
    base::Time start_time,
    content::StatusCodeResponse status) {
  worker_starting_ = false;
  for (auto& observer : observers_) {
    observer.OnWorkerStartFail(context_id, start_time, status);
  }
}

bool ServiceWorkerState::RendererDidInitializeServiceWorkerContext(
    const SequencedContextId& context_id,
    const WorkerId& worker_id) {
  CHECK(worker_id.start_token);
  if (!service_worker_context_->IsLiveServiceWorkerWithToken(
          worker_id.version_id, *worker_id.start_token)) {
    // Drop the IPC message. It is from a stale worker instance.
    return false;
  }

  if (renderer_state() != RendererState::kNotActive) {
    // Must be set because the renderer state must have gone through
    // `kInitialized`, and set the `worker_id`.
    CHECK(worker_id_.has_value());

    // For a given service worker instance, we can only see one
    // `RendererDidInitializeServiceWorkerContext` and it will always come
    // before the associated `RendererDidStartServiceWorkerContext`. So we
    // can't see the same token twice here.
    auto preexisting_token = *worker_id_->start_token;
    auto new_token = *worker_id.start_token;
    CHECK_NE(preexisting_token, new_token);

    auto preexisting_version_id = worker_id_->version_id;
    auto new_version_id = worker_id.version_id;
    RecordWorkerVersionIdStateHistogram(new_version_id, preexisting_version_id);

    // We don't expect to see this method being called twice for the same
    // service worker version, and histograms confirm it, so let's assert it.
    CHECK_NE(preexisting_version_id, new_version_id);
    if (new_version_id < preexisting_version_id) {
      // Drop the IPC message. It is from a stale worker version.
      return false;
    }

    // We received a message from a newer worker than the one we're tracking.
    // This means a newer worker started up and we just haven't received the
    // stop notification for the old one yet. Proactively stop tracking the
    // old worker; it's now considered stale.
    GURL scope = Extension::GetServiceWorkerScopeFromExtensionId(
        context_id.extension_id);
    HandleStop(worker_id_->version_id, scope, *worker_id_->start_token);
  }

  SetWorkerId(worker_id);
  SetRendererState(RendererState::kInitialized);
  return true;
}

void ServiceWorkerState::RendererDidStartServiceWorkerContext(
    const SequencedContextId& context_id,
    const WorkerId& worker_id) {
  CHECK(worker_id.start_token);
  if (!service_worker_context_->IsLiveServiceWorkerWithToken(
          worker_id.version_id, *worker_id.start_token)) {
    // Drop the IPC message. It is from a stale worker instance.
    return;
  }

  if (renderer_state() != RendererState::kInitialized) {
    // We should always see `RendererDidInitializeServiceWorkerContext`
    // before `RendererDidStartServiceWorkerContext`, so if that's not the
    // case, we drop this IPC message, because it must be from a stale service
    // worker.
    return;
  }

  // Must be set because the renderer state is `kInitialized`.
  CHECK(worker_id_.has_value());
  if (worker_id.start_token != worker_id_->start_token) {
    // Drop the IPC message. It's from a different worker instance than the one
    // associated with the `RendererDidInitializeServiceWorkerContext`, so it
    // must be stale.
    return;
  }

  SetRendererState(RendererState::kActive);
  NotifyObserversIfReady(context_id);
}

void ServiceWorkerState::NotifyObserversIfReady(
    const SequencedContextId& context_id) {
  if (!IsReady()) {
    return;
  }
  worker_starting_ = false;
  for (auto& observer : observers_) {
    observer.OnWorkerStart(context_id, *worker_id_);
  }
}

void ServiceWorkerState::RendererDidStopServiceWorkerContext(
    const WorkerId& worker_id,
    const GURL& scope) {
  CHECK(worker_id.start_token);
  HandleStop(worker_id.version_id, scope, *worker_id.start_token);
}

void ServiceWorkerState::OnStoppingSync(
    int64_t version_id,
    const GURL& scope,
    const blink::ServiceWorkerToken& service_worker_token) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  HandleStop(version_id, scope, service_worker_token);
}

void ServiceWorkerState::OnStoppedSync(
    int64_t version_id,
    const GURL& scope,
    const blink::ServiceWorkerToken& service_worker_token) {
  HandleStop(version_id, scope, service_worker_token);
}

void ServiceWorkerState::HandleStop(
    int64_t version_id,
    const GURL& scope,
    const blink::ServiceWorkerToken& service_worker_token) {
  // NOTE: this method may be called multiple times for the same service worker,
  // or even for service workers whose token is not tracked by this class
  // anymore. It needs to handle those cases gracefully.

  // Service workers registered for subscopes via
  // `navigation.serviceWorker.register()` rather than being declared in the
  // manifest's background section are not allowed to use extensions API, and
  // should be ignored here.
  if (scope.GetPath() != "/") {
    return;
  }

  // Check that the worker that is stopping refers to an extension service
  // worker that is tracked by this class.
  if (worker_id_ && worker_id_->start_token == service_worker_token) {
    // Untrack all the worker state because once a worker begin stopping or
    // stops, a new instance must start before the worker can be considered
    // ready to receive tasks/events again and the renderer stop notifications
    // are not 100% reliable.
    Reset();
  }

  // NOTE: we still signal to our observers that a service worker version is
  // stopping, even when we don't track the state of that version in this class
  // anymore. Observers may still care about those versions for tracking or
  // testing purposes. Importantly, ServiceWorkerTaskQueue needs this to untrack
  // old service worker versions from ProcessManager. See crbug.com/40936639.
  for (auto& observer : observers_) {
    observer.OnWorkerStop(version_id, service_worker_token, scope);
  }
}

// static
base::AutoReset<bool>
ServiceWorkerState::AllowMultipleWorkersPerExtensionForTesting() {
  return base::AutoReset<bool>(&g_allow_multiple_workers_per_extension, true);
}

void ServiceWorkerState::StopObservingContextForTest() {
  service_worker_context_observation_.Reset();
}

}  // namespace extensions
