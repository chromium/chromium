// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/service_worker/service_worker_state.h"

#include "base/metrics/histogram_macros.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_features.h"

namespace extensions {

namespace {

// Prevent check on multiple workers per extension for testing purposes.
bool g_allow_multiple_workers_per_extension = false;

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
    int process_id,
    int thread_id) {
  UMA_HISTOGRAM_BOOLEAN("Extensions.ServiceWorkerBackground.StartWorkerStatus",
                        true);
  UMA_HISTOGRAM_TIMES("Extensions.ServiceWorkerBackground.StartWorkerTime",
                      base::Time::Now() - start_time);

  DCHECK_NE(BrowserState::kActive, browser_state())
      << "Worker was already loaded";

  const ExtensionId& extension_id = context_id.extension_id;
  const WorkerId worker_id = {extension_id, process_id, version_id, thread_id};

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

void ServiceWorkerState::RendererDidStartServiceWorkerContext(
    const SequencedContextId& context_id,
    const WorkerId& worker_id) {
  DCHECK_NE(RendererState::kActive, renderer_state())
      << "Worker already started";

  SetWorkerId(worker_id);
  SetRendererState(RendererState::kActive);
  NotifyObserversIfReady(context_id);
}

void ServiceWorkerState::NotifyObserversIfReady(
    const SequencedContextId& context_id) {
  if (IsReady()) {
    worker_starting_ = false;

    if (!base::FeatureList::IsEnabled(
            extensions_features::kOptimizeServiceWorkerStartRequests)) {
      SetBrowserState(ServiceWorkerState::BrowserState::kReady);
    }

    for (auto& observer : observers_) {
      observer.OnWorkerStart(context_id, *worker_id_);
    }
  }
}

void ServiceWorkerState::RendererDidStopServiceWorkerContext(
    const WorkerId& worker_id,
    const GURL& scope) {
  if (worker_id_ != worker_id) {
    // We can see `RendererDidStopServiceWorkerContext` right after
    // `RendererDidInitializeServiceWorkerContext` and without
    // `RendererDidStartServiceWorkerContext`.
    return;
  }

  if (renderer_state() != RendererState::kActive) {
    // We can see `RendererDidStopServiceWorkerContext` before or after
    // `OnStoppingSync`.
    return;
  }

  HandleStop(worker_id_->version_id, scope);
}

void ServiceWorkerState::OnStoppingSync(int64_t version_id, const GURL& scope) {
  // TODO(crbug.com/40936639): Confirming this is true in order to allow for
  // synchronous notification of this status change.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  HandleStop(version_id, scope);
}

void ServiceWorkerState::OnStoppedSync(int64_t version_id, const GURL& scope) {
  // If `OnStoppingSync` was not called for some reason, try again here.
  if (browser_state_ != BrowserState::kNotActive) {
    OnStoppingSync(version_id, scope);
  }
}

void ServiceWorkerState::HandleStop(int64_t version_id, const GURL& scope) {
  // Check that the version ID of the worker that is stopping refers to an
  // extension service worker that is tracked by this class. Service workers
  // registered for subscopes via `navigation.serviceWorker.register()` rather
  // than being declared in the manifest's background section are not allowed
  // to use extensions API, and should be ignored here. See crbug.com/395536907.
  if (worker_id_ && worker_id_->version_id == version_id) {
    // Untrack all the worker state because once a worker begin stopping or
    // stops, a new instance must start before the worker can be considered
    // ready to receive tasks/events again and the renderer stop notifications
    // are not 100% reliable.
    Reset();
  }

  for (auto& observer : observers_) {
    observer.OnWorkerStop(version_id, scope);
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
