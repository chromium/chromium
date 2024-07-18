// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_registration.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/observer_list.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "content/browser/service_worker/service_worker_client.h"
#include "content/browser/service_worker/service_worker_consts.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_info.h"
#include "content/browser/service_worker/service_worker_job_coordinator.h"
#include "content/browser/service_worker/service_worker_metrics.h"
#include "content/browser/service_worker/service_worker_register_job.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/common/content_navigation_policy.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/blink/public/common/service_worker/embedded_worker_status.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration_options.mojom.h"

namespace content {

namespace {

constexpr base::TimeDelta kSelfUpdateDelay = base::Seconds(30);
constexpr base::TimeDelta kMaxSelfUpdateDelay = base::Minutes(3);

// If an outgoing active worker has no controllees or the waiting worker called
// skipWaiting(), it is given |kMaxLameDuckTime| time to finish its requests
// before it is removed. If the waiting worker called skipWaiting() more than
// this time ago, or the outgoing worker has had no controllees for a continuous
// period of time exceeding this time, the outgoing worker will be removed even
// if it has ongoing requests.
constexpr base::TimeDelta kMaxLameDuckTime = base::Minutes(5);

ServiceWorkerVersionInfo GetVersionInfo(ServiceWorkerVersion* version) {
  if (!version)
    return ServiceWorkerVersionInfo();
  return version->GetInfo();
}

}  // namespace

// static
scoped_refptr<ServiceWorkerRegistration> ServiceWorkerRegistration::Create(
    const blink::mojom::ServiceWorkerRegistrationOptions& options,
    const blink::StorageKey& key,
    int64_t registration_id,
    base::WeakPtr<ServiceWorkerContextCore> context,
    blink::mojom::AncestorFrameType ancestor_frame_type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_NE(blink::mojom::kInvalidServiceWorkerRegistrationId, registration_id);
  DCHECK(context);

  // A scoped ref pointer of `ServiceWorkerRegistration` is explicitly created
  // here so that the instance won't be unexpectedly destroyed due to a
  // scoped_refptr operation on the registration inside `AddLiveRegistration()`.
  auto registration_ref =
      base::WrapRefCounted(std::move(new ServiceWorkerRegistration(
          options, key, registration_id, context, ancestor_frame_type)));

  registration_ref->context_->AddLiveRegistration(registration_ref.get());
  return registration_ref;
}

ServiceWorkerRegistration::ServiceWorkerRegistration(
    const blink::mojom::ServiceWorkerRegistrationOptions& options,
    const blink::StorageKey& key,
    int64_t registration_id,
    base::WeakPtr<ServiceWorkerContextCore> context,
    blink::mojom::AncestorFrameType ancestor_frame_type)
    : scope_(options.scope),
      key_(key),
      update_via_cache_(options.update_via_cache),
      registration_id_(registration_id),
      status_(Status::kIntact),
      store_state_(StoreState::kNotStored),
      should_activate_when_ready_(false),
      resources_total_size_bytes_(0),
      context_(context),
      task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()),
      ancestor_frame_type_(ancestor_frame_type) {
}

ServiceWorkerRegistration::~ServiceWorkerRegistration() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(listeners_.empty());

  // TODO(crbug.com/40737650): Remove once the bug is fixed.
  CHECK(!in_activate_waiting_version_)
      << "ServiceWorkerRegistration was destroyed while activating waiting "
         "version";
  if (context_)
    context_->RemoveLiveRegistration(registration_id_);
}

void ServiceWorkerRegistration::SetStatus(Status status) {
  if (status_ == status)
    return;
#if DCHECK_IS_ON()
  switch (status_) {
    case Status::kIntact:
      DCHECK_EQ(status, Status::kUninstalling);
      break;
    case Status::kUninstalling:
      // All transitions are allowed:
      // - To kIntact: resurrected.
      // - To kUninstalled: finished uninstalling.
      break;
    case Status::kUninstalled:
      NOTREACHED_IN_MIGRATION();
      break;
  }
#endif  // DCHECK_IS_ON()

  status_ = status;

  if (active_version_)
    active_version_->SetRegistrationStatus(status_);
  if (waiting_version_)
    waiting_version_->SetRegistrationStatus(status_);
  if (installing_version_)
    installing_version_->SetRegistrationStatus(status_);
}

bool ServiceWorkerRegistration::IsStored() const {
  return context_ && store_state_ == StoreState::kStored;
}

void ServiceWorkerRegistration::SetStored() {
  store_state_ = StoreState::kStored;
}

void ServiceWorkerRegistration::UnsetStored() {
  store_state_ = StoreState::kNotStored;
}

ServiceWorkerVersion* ServiceWorkerRegistration::GetNewestVersion() const {
  if (installing_version())
    return installing_version();
  if (waiting_version())
    return waiting_version();
  return active_version();
}

void ServiceWorkerRegistration::AddListener(Listener* listener) {
  listeners_.AddObserver(listener);
}

void ServiceWorkerRegistration::RemoveListener(Listener* listener) {
  listeners_.RemoveObserver(listener);
}

void ServiceWorkerRegistration::NotifyRegistrationFailed() {
  for (auto& observer : listeners_)
    observer.OnRegistrationFailed(this);
  NotifyRegistrationFinished();
}

void ServiceWorkerRegistration::NotifyUpdateFound() {
  for (auto& observer : listeners_)
    observer.OnUpdateFound(this);
}

void ServiceWorkerRegistration::NotifyVersionAttributesChanged(
    blink::mojom::ChangedServiceWorkerObjectsMaskPtr mask) {
  for (auto& observer : listeners_)
    observer.OnVersionAttributesChanged(this, mask.Clone());
  if (mask->active || mask->waiting)
    NotifyRegistrationFinished();
}

ServiceWorkerRegistrationInfo ServiceWorkerRegistration::GetInfo() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return ServiceWorkerRegistrationInfo(
      scope(), key(), update_via_cache(), registration_id_,
      is_deleted() ? ServiceWorkerRegistrationInfo::IS_DELETED
                   : ServiceWorkerRegistrationInfo::IS_NOT_DELETED,
      GetVersionInfo(active_version_.get()),
      GetVersionInfo(waiting_version_.get()),
      GetVersionInfo(installing_version_.get()), resources_total_size_bytes_,
      navigation_preload_state_.enabled,
      navigation_preload_state_.header.length());
}

void ServiceWorkerRegistration::SetActiveVersion(
    const scoped_refptr<ServiceWorkerVersion>& version) {
  if (active_version_ == version)
    return;

  should_activate_when_ready_ = false;

  auto mask =
      blink::mojom::ChangedServiceWorkerObjectsMask::New(false, false, false);
  if (version) {
    UnsetVersionInternal(version.get(), mask.get());
    version->SetRegistrationStatus(status_);
  }
  active_version_ = version;
  if (active_version_)
    active_version_->SetNavigationPreloadState(navigation_preload_state_);
  mask->active = true;

  NotifyVersionAttributesChanged(std::move(mask));
}

void ServiceWorkerRegistration::SetWaitingVersion(
    const scoped_refptr<ServiceWorkerVersion>& version) {
  if (waiting_version_ == version)
    return;

  should_activate_when_ready_ = false;

  auto mask =
      blink::mojom::ChangedServiceWorkerObjectsMask::New(false, false, false);
  if (version) {
    UnsetVersionInternal(version.get(), mask.get());
    version->SetRegistrationStatus(status_);
  }
  waiting_version_ = version;
  mask->waiting = true;

  NotifyVersionAttributesChanged(std::move(mask));
}

void ServiceWorkerRegistration::SetInstallingVersion(
    const scoped_refptr<ServiceWorkerVersion>& version) {
  if (installing_version_ == version)
    return;
  auto mask =
      blink::mojom::ChangedServiceWorkerObjectsMask::New(false, false, false);
  if (version) {
    UnsetVersionInternal(version.get(), mask.get());
    version->SetRegistrationStatus(status_);
  }
  installing_version_ = version;
  mask->installing = true;
  NotifyVersionAttributesChanged(std::move(mask));
}

void ServiceWorkerRegistration::UnsetVersion(ServiceWorkerVersion* version) {
  if (!version)
    return;
  auto mask =
      blink::mojom::ChangedServiceWorkerObjectsMask::New(false, false, false);
  UnsetVersionInternal(version, mask.get());
  if (mask->installing || mask->waiting || mask->active)
    NotifyVersionAttributesChanged(std::move(mask));
}

void ServiceWorkerRegistration::UnsetVersionInternal(
    ServiceWorkerVersion* version,
    blink::mojom::ChangedServiceWorkerObjectsMask* mask) {
  DCHECK(version);

  if (installing_version_.get() == version) {
    installing_version_ = nullptr;
    mask->installing = true;
  } else if (waiting_version_.get() == version) {
    waiting_version_ = nullptr;
    should_activate_when_ready_ = false;
    mask->waiting = true;
  } else if (active_version_.get() == version) {
    active_version_ = nullptr;
    mask->active = true;
  }
}

void ServiceWorkerRegistration::SetUpdateViaCache(
    blink::mojom::ServiceWorkerUpdateViaCache update_via_cache) {
  if (update_via_cache_ == update_via_cache)
    return;
  update_via_cache_ = update_via_cache;
  for (auto& observer : listeners_)
    observer.OnUpdateViaCacheChanged(this);
}

void ServiceWorkerRegistration::ActivateWaitingVersionWhenReady() {
  DCHECK(waiting_version());
  should_activate_when_ready_ = true;
  if (IsReadyToActivate()) {
    ActivateWaitingVersion(false /* delay */);
    return;
  }

  if (IsLameDuckActiveVersion()) {
    if (active_version()->running_status() ==
        blink::EmbeddedWorkerStatus::kRunning) {
      // If the waiting worker is ready and the active worker needs to be
      // swapped out, ask the active worker to trigger idle timer as soon as
      // possible.
      active_version()->TriggerIdleTerminationAsap();
    }
    StartLameDuckTimer();
  }
}

void ServiceWorkerRegistration::ClaimClients() {
  DCHECK(context_);
  DCHECK(active_version());

  // https://w3c.github.io/ServiceWorker/#clients-claim
  //
  // "For each service worker client client whose origin is the same as the
  //  service worker's origin:
  const bool include_reserved_clients = false;
  // Include clients in BackForwardCache in order to evict them if needed.
  const bool include_back_forward_cached_clients = true;
  for (auto it =
           context_->service_worker_client_owner().GetServiceWorkerClients(
               key_, include_reserved_clients,
               include_back_forward_cached_clients);
       !it.IsAtEnd(); ++it) {
    // "1. If client’s execution ready flag is unset or client’s discarded flag
    //     is set, continue."
    // |include_reserved_clients| ensures only execution ready clients are
    // returned.
    DCHECK(it->is_execution_ready());

    // This is part of step 5 but performed here as an optimization. Do nothing
    // if this version is already the controller.
    if (it->controller() == active_version()) {
      continue;
    }

    // "2. If client is not a secure context, continue."
    if (!it->IsEligibleForServiceWorkerController()) {
      continue;
    }

    // "3. Let registration be the result of running Match Service Worker
    //     Registration algorithm passing client’s creation URL as the argument.
    //  4. If registration is not the service worker's containing service worker
    //     registration, continue."
    if (it->MatchRegistration() != this) {
      continue;
    }

    // Evict the client in BackForwardCache.
    if (it->IsInBackForwardCache()) {
      it->EvictFromBackForwardCache(
          BackForwardCacheMetrics::NotRestoredReason::kServiceWorkerClaim);
    }

    // The remaining steps are performed here:
    it->ClaimedByRegistration(this);
  }
}

void ServiceWorkerRegistration::DeleteAndClearWhenReady() {
  DCHECK(context_);
  if (is_deleted()) {
    // We already deleted and are waiting to clear, or the registration is
    // already cleared.
    return;
  }

  context_->registry()->DeleteRegistration(
      this, base::BindOnce(&ServiceWorkerRegistration::OnDeleteFinished, this));

  if (!active_version() || !active_version()->HasControllee())
    Clear();
}

void ServiceWorkerRegistration::DeleteAndClearImmediately() {
  DCHECK(context_);
  if (!is_deleted()) {
    context_->registry()->DeleteRegistration(
        this,
        base::BindOnce(&ServiceWorkerRegistration::OnDeleteFinished, this));
  }

  if (is_uninstalling())
    Clear();
}

void ServiceWorkerRegistration::AbortPendingClear(StatusCallback callback) {
  DCHECK(context_);

  switch (status_) {
    case Status::kIntact:
      std::move(callback).Run(blink::ServiceWorkerStatusCode::kOk);
      return;
    case Status::kUninstalling:
      break;
    case Status::kUninstalled:
      NOTREACHED_IN_MIGRATION()
          << "attempt to resurrect a completely uninstalled registration";
      break;
  }

  context_->registry()->NotifyDoneUninstallingRegistration(this,
                                                           Status::kIntact);

  scoped_refptr<ServiceWorkerVersion> most_recent_version =
      waiting_version() ? waiting_version() : active_version();
  DCHECK(most_recent_version.get());
  context_->registry()->NotifyInstallingRegistration(this);
  context_->registry()->StoreRegistration(
      this, most_recent_version.get(),
      base::BindOnce(&ServiceWorkerRegistration::OnRestoreFinished, this,
                     std::move(callback), most_recent_version));
}

void ServiceWorkerRegistration::OnNoControllees(ServiceWorkerVersion* version) {
  DCHECK(context_);
  if (version != active_version())
    return;

  if (is_uninstalling()) {
    // TODO(falken): This can destroy the caller (ServiceWorkerVersion). Try to
    // make this async.
    Clear();
    return;
  }

  if (IsReadyToActivate()) {
    ActivateWaitingVersion(true /* delay */);
    return;
  }

  if (IsLameDuckActiveVersion()) {
    if (should_activate_when_ready_ &&
        active_version()->running_status() ==
            blink::EmbeddedWorkerStatus::kRunning) {
      // If the waiting worker is ready and the active worker needs to be
      // swapped out, ask the active worker to trigger idle timer as soon as
      // possible.
      active_version()->TriggerIdleTerminationAsap();
    }
    StartLameDuckTimer();
  }
}

void ServiceWorkerRegistration::OnNoWork(ServiceWorkerVersion* version) {
  DCHECK(context_);

  if (version == active_version() && IsReadyToActivate())
    ActivateWaitingVersion(true /* delay */);
}

bool ServiceWorkerRegistration::IsReadyToActivate() const {
  if (!should_activate_when_ready_)
    return false;

  DCHECK(waiting_version());
  const ServiceWorkerVersion* waiting = waiting_version();
  const ServiceWorkerVersion* active = active_version();
  if (!active) {
    return true;
  }
  if (IsLameDuckActiveVersion()) {
    return active->HasNoWork() ||
           waiting->TimeSinceSkipWaiting() > kMaxLameDuckTime ||
           active->TimeSinceNoControllees() > kMaxLameDuckTime;
  }
  return false;
}

bool ServiceWorkerRegistration::IsLameDuckActiveVersion() const {
  if (!waiting_version() || !active_version())
    return false;
  return waiting_version()->skip_waiting() ||
         !active_version()->HasControllee();
}

void ServiceWorkerRegistration::StartLameDuckTimer() {
  DCHECK(IsLameDuckActiveVersion());
  if (lame_duck_timer_.IsRunning())
    return;

  lame_duck_timer_.Start(
      FROM_HERE, kMaxLameDuckTime,
      base::BindRepeating(
          &ServiceWorkerRegistration::RemoveLameDuckIfNeeded,
          Unretained(this) /* OK because |this| owns the timer */));
}

void ServiceWorkerRegistration::RemoveLameDuckIfNeeded() {
  if (!should_activate_when_ready_) {
    lame_duck_timer_.Stop();
    return;
  }

  if (IsReadyToActivate()) {
    ActivateWaitingVersion(false /* delay */);
    return;
  }

  if (!IsLameDuckActiveVersion()) {
    lame_duck_timer_.Stop();
  }
}

void ServiceWorkerRegistration::ActivateWaitingVersion(bool delay) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(context_);
  DCHECK(IsReadyToActivate());
  should_activate_when_ready_ = false;
  lame_duck_timer_.Stop();

  scoped_refptr<ServiceWorkerVersion> activating_version = waiting_version();
  scoped_refptr<ServiceWorkerVersion> exiting_version = active_version();

  if (activating_version->is_redundant())
    return;  // Activation is no longer relevant.

  in_activate_waiting_version_ = true;

  // "5. If exitingWorker is not null,
  if (exiting_version.get()) {
    // Whenever activation happens, evict bfcached controllees.
    if (IsBackForwardCacheEnabled()) {
      exiting_version->EvictBackForwardCachedControllees(
          BackForwardCacheMetrics::NotRestoredReason::
              kServiceWorkerVersionActivation);
    }

    // TODO(falken): Update the quoted spec comments once
    // https://github.com/slightlyoff/ServiceWorker/issues/916 is codified in
    // the spec.
    // "1. Wait for exitingWorker to finish handling any in-progress requests."
    // This is already handled by IsReadyToActivate().
    // "2. Terminate exitingWorker."
    exiting_version->StopWorker(base::DoNothing());
    // "3. Run the [[UpdateState]] algorithm passing exitingWorker and
    // "redundant" as the arguments."
    exiting_version->SetStatus(ServiceWorkerVersion::REDUNDANT);
  }

  // "6. Set serviceWorkerRegistration.activeWorker to activatingWorker."
  // "7. Set serviceWorkerRegistration.waitingWorker to null."
  SetActiveVersion(activating_version);

  // "8. Run the [[UpdateState]] algorithm passing registration.activeWorker and
  // "activating" as arguments."
  activating_version->SetStatus(ServiceWorkerVersion::ACTIVATING);
  // "9. Fire a simple event named controllerchange..."
  if (activating_version->skip_waiting()) {
    for (auto& observer : listeners_)
      observer.OnSkippedWaiting(this);
  }

  // "10. Queue a task to fire an event named activate..."
  // The browser could be shutting down. To avoid spurious start worker
  // failures, wait a bit before continuing.
  in_activate_waiting_version_ = false;
  if (delay) {
    task_runner_->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&ServiceWorkerRegistration::ContinueActivation, this,
                       activating_version),
        base::Seconds(1));
  } else {
    ContinueActivation(std::move(activating_version));
  }
}

void ServiceWorkerRegistration::ContinueActivation(
    scoped_refptr<ServiceWorkerVersion> activating_version) {
  if (!context_)
    return;
  if (active_version() != activating_version.get())
    return;
  DCHECK_EQ(ServiceWorkerVersion::ACTIVATING, activating_version->status());
  activating_version->RunAfterStartWorker(
      ServiceWorkerMetrics::EventType::ACTIVATE,
      base::BindOnce(&ServiceWorkerRegistration::DispatchActivateEvent, this,
                     activating_version));
}

void ServiceWorkerRegistration::ForceDelete() {
  DCHECK(context_);
  DCHECK(!is_uninstalled()) << "attempt to delete registration twice";

  // Protect the registration since version->Doom() can stop |version|, which
  // destroys start worker callbacks, which might be the only things holding a
  // reference to |this|.
  scoped_refptr<ServiceWorkerRegistration> protect(this);

  // Abort any queued or running jobs for this registration.
  context_->job_coordinator()->Abort(scope(), key());

  // The rest of this function is similar to Clear() but is slightly different
  // because this emergency deletion isn't part of the spec and happens
  // outside of the normal job coordinator.
  // TODO(falken): Consider merging the two.
  should_activate_when_ready_ = false;

  // Doom versions. This sets the versions to redundant and tells the
  // controllees that they are gone.
  //
  // There can't be an installing version since we aborted any register job.
  DCHECK(!installing_version_);
  auto mask =
      blink::mojom::ChangedServiceWorkerObjectsMask::New(false, false, false);
  // Unset the version first so we stop listening to the version as it might
  // invoke listener methods during Doom().
  if (scoped_refptr<ServiceWorkerVersion> waiting_version = waiting_version_) {
    UnsetVersionInternal(waiting_version.get(), mask.get());
    waiting_version->Doom();
  }
  if (scoped_refptr<ServiceWorkerVersion> active_version = active_version_) {
    UnsetVersionInternal(active_version.get(), mask.get());
    active_version->Doom();
  }

  // Delete the registration and its state from storage.
  if (status() == Status::kIntact) {
    context_->registry()->DeleteRegistration(
        this,
        base::BindOnce(&ServiceWorkerRegistration::OnDeleteFinished, protect));
  }
  DCHECK(is_uninstalling());
  context_->registry()->NotifyDoneUninstallingRegistration(
      this, Status::kUninstalled);

  // Tell observers that this registration is gone.
  NotifyRegistrationFailed();
}

void ServiceWorkerRegistration::NotifyRegistrationFinished() {
  std::vector<base::OnceClosure> callbacks;
  callbacks.swap(registration_finished_callbacks_);
  for (auto& callback : callbacks)
    std::move(callback).Run();
}

void ServiceWorkerRegistration::SetTaskRunnerForTest(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  task_runner_ = task_runner;
}

void ServiceWorkerRegistration::EnableNavigationPreload(bool enable) {
  navigation_preload_state_.enabled = enable;
  if (active_version_)
    active_version_->SetNavigationPreloadState(navigation_preload_state_);
}

void ServiceWorkerRegistration::SetNavigationPreloadHeader(
    const std::string& header) {
  navigation_preload_state_.header = header;
  if (active_version_)
    active_version_->SetNavigationPreloadState(navigation_preload_state_);
}

void ServiceWorkerRegistration::RegisterRegistrationFinishedCallback(
    base::OnceClosure callback) {
  // This should only be called if the registration is in progress.
  DCHECK(!active_version() && !waiting_version() && !is_uninstalled() &&
         !is_uninstalling());
  registration_finished_callbacks_.push_back(std::move(callback));
}

void ServiceWorkerRegistration::DispatchActivateEvent(
    scoped_refptr<ServiceWorkerVersion> activating_version,
    blink::ServiceWorkerStatusCode start_worker_status) {
  if (start_worker_status != blink::ServiceWorkerStatusCode::kOk) {
    OnActivateEventFinished(activating_version, start_worker_status);
    return;
  }
  if (activating_version != active_version()) {
    OnActivateEventFinished(activating_version,
                            blink::ServiceWorkerStatusCode::kErrorFailed);
    return;
  }

  DCHECK_EQ(ServiceWorkerVersion::ACTIVATING, activating_version->status());
  DCHECK_EQ(blink::EmbeddedWorkerStatus::kRunning,
            activating_version->running_status())
      << "Worker stopped too soon after it was started.";
  int request_id = activating_version->StartRequest(
      ServiceWorkerMetrics::EventType::ACTIVATE,
      base::BindOnce(&ServiceWorkerRegistration::OnActivateEventFinished, this,
                     activating_version));
  activating_version->endpoint()->DispatchActivateEvent(
      activating_version->CreateSimpleEventCallback(request_id));
}

void ServiceWorkerRegistration::OnActivateEventFinished(
    scoped_refptr<ServiceWorkerVersion> activating_version,
    blink::ServiceWorkerStatusCode status) {
  // Activate is prone to failing due to shutdown, because it's triggered when
  // tabs close.
  bool is_shutdown =
      !context_ || context_->wrapper()->process_manager()->IsShutdown();
  ServiceWorkerMetrics::RecordActivateEventStatus(status, is_shutdown);

  if (!context_ || activating_version != active_version() ||
      activating_version->status() != ServiceWorkerVersion::ACTIVATING) {
    return;
  }

  // Normally, the worker is committed to become activated once we get here, per
  // spec. E.g., if the script rejected waitUntil or had an unhandled exception,
  // it should still be activated. However, if the failure occurred during
  // shutdown, ignore it to give the worker another chance the next time the
  // browser starts up.
  if (is_shutdown && status != blink::ServiceWorkerStatusCode::kOk)
    return;

  // "Run the Update State algorithm passing registration's active worker and
  // 'activated' as the arguments."
  activating_version->SetStatus(ServiceWorkerVersion::ACTIVATED);

  // If router rules are registered, record the information on rules.
  if (activating_version->router_evaluator()) {
    activating_version->router_evaluator()->RecordRouterRuleInfo();
  }

  context_->registry()->UpdateToActiveState(id(), key_, base::DoNothing());
}

void ServiceWorkerRegistration::OnDeleteFinished(
    blink::ServiceWorkerStatusCode status) {
  for (auto& listener : listeners_)
    listener.OnRegistrationDeleted(this);
}

void ServiceWorkerRegistration::Clear() {
  DCHECK(is_uninstalling());
  SetStatus(Status::kUninstalled);
  should_activate_when_ready_ = false;

  // Some callbacks, at least OnRegistrationFinishedUninstalling and
  // NotifyDoneUninstallingRegistration, may drop their references to
  // |this|, so protect it first.
  // TODO(falken): Clean this up, can we call the observers from a task
  // or make the observers more polite?
  auto protect = base::WrapRefCounted(this);

  if (context_) {
    context_->registry()->NotifyDoneUninstallingRegistration(
        this, Status::kUninstalled);
  }

  std::vector<scoped_refptr<ServiceWorkerVersion>> versions_to_doom;
  auto mask =
      blink::mojom::ChangedServiceWorkerObjectsMask::New(false, false, false);
  if (installing_version_.get()) {
    versions_to_doom.push_back(installing_version_);
    installing_version_ = nullptr;
    mask->installing = true;
  }
  if (waiting_version_.get()) {
    versions_to_doom.push_back(waiting_version_);
    waiting_version_ = nullptr;
    mask->waiting = true;
  }
  if (active_version_.get()) {
    versions_to_doom.push_back(active_version_);
    active_version_ = nullptr;
    mask->active = true;
  }

  if (mask->installing || mask->waiting || mask->active) {
    NotifyVersionAttributesChanged(std::move(mask));

    // Doom only after notifying attributes changed, because the spec requires
    // the attributes to be cleared by the time the statechange event is
    // dispatched.
    for (const auto& version : versions_to_doom)
      version->Doom();
  }

  for (auto& observer : listeners_)
    observer.OnRegistrationFinishedUninstalling(this);
}

void ServiceWorkerRegistration::OnRestoreFinished(
    StatusCallback callback,
    scoped_refptr<ServiceWorkerVersion> version,
    blink::ServiceWorkerStatusCode status) {
  if (!context_) {
    std::move(callback).Run(blink::ServiceWorkerStatusCode::kErrorAbort);
    return;
  }
  context_->registry()->NotifyDoneInstallingRegistration(this, version.get(),
                                                         status);
  std::move(callback).Run(status);
}

void ServiceWorkerRegistration::DelayUpdate(
    ServiceWorkerVersion& version,
    blink::mojom::FetchClientSettingsObjectPtr
        outside_fetch_client_settings_object,
    blink::mojom::ServiceWorkerRegistrationObjectHost::UpdateCallback
        callback) {
  if (ServiceWorkerVersion::Status::INSTALLING == version.status()) {
    // This can happen if update() is called during execution of the
    // install-event-handler.
    std::move(callback).Run(blink::mojom::ServiceWorkerErrorType::kState,
                            ComposeUpdateErrorMessagePrefix(&version) +
                                ServiceWorkerConsts::kInvalidStateErrorMessage);
    return;
  }

  if (version.HasControllee()) {
    // Don't delay update() if called by ServiceWorkers with controllees.
    ExecuteUpdate(std::move(outside_fetch_client_settings_object),
                  std::move(callback));
    return;
  }

  base::TimeDelta delay = self_update_delay();
  if (delay > kMaxSelfUpdateDelay) {
    // The delay was already very long and update() is rejected immediately.
    std::move(callback).Run(
        blink::mojom::ServiceWorkerErrorType::kTimeout,
        ComposeUpdateErrorMessagePrefix(GetNewestVersion()) +
            ServiceWorkerConsts::kUpdateTimeoutErrorMesage);
    return;
  }

  if (delay < kSelfUpdateDelay) {
    set_self_update_delay(kSelfUpdateDelay);
  } else {
    set_self_update_delay(delay * 2);
  }

  if (delay < base::TimeDelta::Min()) {
    // Only enforce the delay of update() iff |delay| exists.
    ExecuteUpdate(std::move(outside_fetch_client_settings_object),
                  std::move(callback));
    return;
  }

  // Delays an update if it is called by a worker without controllee, to prevent
  // workers from running forever (see https://crbug.com/805496).
  GetUIThreadTaskRunner({})->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          &ServiceWorkerRegistration::ExecuteUpdate, base::WrapRefCounted(this),
          std::move(outside_fetch_client_settings_object), std::move(callback)),
      delay);
}

void ServiceWorkerRegistration::ExecuteUpdate(
    blink::mojom::FetchClientSettingsObjectPtr
        outside_fetch_client_settings_object,
    blink::mojom::ServiceWorkerRegistrationObjectHost::UpdateCallback
        callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!context_) {
    std::move(callback).Run(
        blink::mojom::ServiceWorkerErrorType::kAbort,
        ComposeUpdateErrorMessagePrefix(GetNewestVersion()) +
            ServiceWorkerConsts::kShutdownErrorMessage);
    return;
  }

  scoped_refptr<ServiceWorkerRegistration> registration =
      context_->GetLiveRegistration(id());
  if (!registration) {
    // The service worker is no longer running, so update() won't be rejected.
    // We still run the callback so the caller knows.
    std::move(callback).Run(
        blink::mojom::ServiceWorkerErrorType::kTimeout,
        ComposeUpdateErrorMessagePrefix(GetNewestVersion()) +
            ServiceWorkerConsts::kUpdateTimeoutErrorMesage);
    return;
  }

  context_->UpdateServiceWorker(
      registration.get(),
      /*force_bypass_cache=*/false, /*skip_script_comparison=*/false,
      std::move(outside_fetch_client_settings_object),
      base::BindOnce(&ServiceWorkerRegistration::UpdateComplete,
                     base::WrapRefCounted(this), std::move(callback)));
}

void ServiceWorkerRegistration::UpdateComplete(
    blink::mojom::ServiceWorkerRegistrationObjectHost::UpdateCallback callback,
    blink::ServiceWorkerStatusCode status,
    const std::string& status_message,
    int64_t registration_id) {
  if (status != blink::ServiceWorkerStatusCode::kOk) {
    std::string error_message;
    blink::mojom::ServiceWorkerErrorType error_type;
    GetServiceWorkerErrorTypeForRegistration(status, status_message,
                                             &error_type, &error_message);
    std::move(callback).Run(
        error_type,
        ComposeUpdateErrorMessagePrefix(GetNewestVersion()) + error_message);
    return;
  }

  std::move(callback).Run(blink::mojom::ServiceWorkerErrorType::kNone,
                          std::nullopt);
}

std::string ServiceWorkerRegistration::ComposeUpdateErrorMessagePrefix(
    const ServiceWorkerVersion* version_to_update) const {
  const char* script_url = version_to_update
                               ? version_to_update->script_url().spec().c_str()
                               : "Unknown";
  return base::StringPrintf(
      ServiceWorkerConsts::kServiceWorkerUpdateErrorPrefix,
      scope().spec().c_str(), script_url);
}

}  // namespace content
