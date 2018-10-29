// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_register_job.h"

#include <stdint.h>

#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "content/browser/service_worker/embedded_worker_instance.h"
#include "content/browser/service_worker/embedded_worker_status.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_job_coordinator.h"
#include "content/browser/service_worker/service_worker_metrics.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_storage.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/browser/service_worker/service_worker_write_to_cache_job.h"
#include "content/browser/url_loader_factory_getter.h"
#include "content/common/service_worker/service_worker.mojom.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "net/base/net_errors.h"
#include "third_party/blink/public/common/service_worker/service_worker_type_converters.h"
#include "third_party/blink/public/common/service_worker/service_worker_utils.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_object.mojom.h"

namespace content {

typedef ServiceWorkerRegisterJobBase::RegistrationJobType RegistrationJobType;

ServiceWorkerRegisterJob::ServiceWorkerRegisterJob(
    base::WeakPtr<ServiceWorkerContextCore> context,
    const GURL& script_url,
    const blink::mojom::ServiceWorkerRegistrationOptions& options)
    : context_(context),
      job_type_(REGISTRATION_JOB),
      scope_(options.scope),
      script_url_(script_url),
      worker_script_type_(options.type),
      update_via_cache_(options.update_via_cache),
      phase_(INITIAL),
      doom_installing_worker_(false),
      is_promise_resolved_(false),
      should_uninstall_on_failure_(false),
      force_bypass_cache_(false),
      skip_script_comparison_(false),
      promise_resolved_status_(blink::ServiceWorkerStatusCode::kOk),
      weak_factory_(this) {}

ServiceWorkerRegisterJob::ServiceWorkerRegisterJob(
    base::WeakPtr<ServiceWorkerContextCore> context,
    ServiceWorkerRegistration* registration,
    bool force_bypass_cache,
    bool skip_script_comparison)
    : context_(context),
      job_type_(UPDATE_JOB),
      scope_(registration->scope()),
      update_via_cache_(registration->update_via_cache()),
      phase_(INITIAL),
      doom_installing_worker_(false),
      is_promise_resolved_(false),
      should_uninstall_on_failure_(false),
      force_bypass_cache_(force_bypass_cache),
      promise_resolved_status_(blink::ServiceWorkerStatusCode::kOk),
      weak_factory_(this) {
  // |skip_script_comparison_| should be true when
  // ServiceWorkerImportedScriptUpdateCheck is enabled, because then script
  // comparison happens before starting a worker and it doesn't need to happen
  // during the worker startup.
  skip_script_comparison_ =
      blink::ServiceWorkerUtils::IsImportedScriptUpdateCheckEnabled() ||
      skip_script_comparison;
  internal_.registration = registration;
}

ServiceWorkerRegisterJob::~ServiceWorkerRegisterJob() {
  DCHECK(!context_ ||
         phase_ == INITIAL || phase_ == COMPLETE || phase_ == ABORT)
      << "Jobs should only be interrupted during shutdown.";
}

void ServiceWorkerRegisterJob::AddCallback(RegistrationCallback callback) {
  if (!is_promise_resolved_) {
    callbacks_.emplace_back(std::move(callback));
    return;
  }
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), promise_resolved_status_,
                     promise_resolved_status_message_,
                     base::RetainedRef(promise_resolved_registration_)));
}

void ServiceWorkerRegisterJob::Start() {
  BrowserThread::PostAfterStartupTask(
      FROM_HERE, base::ThreadTaskRunnerHandle::Get(),
      base::BindOnce(&ServiceWorkerRegisterJob::StartImpl,
                     weak_factory_.GetWeakPtr()));
}

void ServiceWorkerRegisterJob::StartImpl() {
  SetPhase(START);
  ServiceWorkerStorage::FindRegistrationCallback next_step;
  if (job_type_ == REGISTRATION_JOB) {
    next_step =
        base::BindOnce(&ServiceWorkerRegisterJob::ContinueWithRegistration,
                       weak_factory_.GetWeakPtr());
  } else {
    next_step = base::BindOnce(&ServiceWorkerRegisterJob::ContinueWithUpdate,
                               weak_factory_.GetWeakPtr());
  }

  scoped_refptr<ServiceWorkerRegistration> registration =
      context_->storage()->GetUninstallingRegistration(scope_);
  if (registration.get())
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(next_step),
                       blink::ServiceWorkerStatusCode::kOk, registration));
  else
    context_->storage()->FindRegistrationForScope(scope_, std::move(next_step));
}

void ServiceWorkerRegisterJob::Abort() {
  SetPhase(ABORT);
  CompleteInternal(blink::ServiceWorkerStatusCode::kErrorAbort, std::string());
  // Don't have to call FinishJob() because the caller takes care of removing
  // the jobs from the queue.
}

bool ServiceWorkerRegisterJob::Equals(ServiceWorkerRegisterJobBase* job) const {
  if (job->GetType() != job_type_)
    return false;
  ServiceWorkerRegisterJob* register_job =
      static_cast<ServiceWorkerRegisterJob*>(job);
  if (job_type_ == UPDATE_JOB)
    return register_job->scope_ == scope_;
  DCHECK_EQ(REGISTRATION_JOB, job_type_);
  return register_job->scope_ == scope_ &&
         register_job->script_url_ == script_url_;
}

RegistrationJobType ServiceWorkerRegisterJob::GetType() const {
  return job_type_;
}

void ServiceWorkerRegisterJob::DoomInstallingWorker() {
  doom_installing_worker_ = true;
  if (phase_ == INSTALL)
    Complete(blink::ServiceWorkerStatusCode::kErrorInstallWorkerFailed,
             std::string());
}

ServiceWorkerRegisterJob::Internal::Internal() {}

ServiceWorkerRegisterJob::Internal::~Internal() {}

void ServiceWorkerRegisterJob::set_registration(
    scoped_refptr<ServiceWorkerRegistration> registration) {
  DCHECK(phase_ == START || phase_ == REGISTER) << phase_;
  DCHECK(!internal_.registration.get());
  internal_.registration = std::move(registration);
}

ServiceWorkerRegistration* ServiceWorkerRegisterJob::registration() {
  DCHECK(phase_ >= REGISTER || job_type_ == UPDATE_JOB) << phase_;
  return internal_.registration.get();
}

void ServiceWorkerRegisterJob::set_new_version(
    ServiceWorkerVersion* version) {
  DCHECK(phase_ == UPDATE) << phase_;
  DCHECK(!internal_.new_version.get());
  internal_.new_version = version;
}

ServiceWorkerVersion* ServiceWorkerRegisterJob::new_version() {
  DCHECK(phase_ >= UPDATE) << phase_;
  return internal_.new_version.get();
}

void ServiceWorkerRegisterJob::SetPhase(Phase phase) {
  switch (phase) {
    case INITIAL:
      NOTREACHED();
      break;
    case START:
      DCHECK(phase_ == INITIAL) << phase_;
      break;
    case REGISTER:
      DCHECK(phase_ == START) << phase_;
      break;
    case UPDATE:
      DCHECK(phase_ == START || phase_ == REGISTER) << phase_;
      break;
    case INSTALL:
      DCHECK(phase_ == UPDATE) << phase_;
      break;
    case STORE:
      DCHECK(phase_ == INSTALL) << phase_;
      break;
    case COMPLETE:
      DCHECK(phase_ != INITIAL && phase_ != COMPLETE) << phase_;
      break;
    case ABORT:
      break;
  }
  phase_ = phase;
}

// This function corresponds to the steps in [[Register]] following
// "Let registration be the result of running the [[GetRegistration]] algorithm.
// Throughout this file, comments in quotes are excerpts from the spec.
void ServiceWorkerRegisterJob::ContinueWithRegistration(
    blink::ServiceWorkerStatusCode status,
    scoped_refptr<ServiceWorkerRegistration> existing_registration) {
  DCHECK_EQ(REGISTRATION_JOB, job_type_);
  if (status != blink::ServiceWorkerStatusCode::kErrorNotFound &&
      status != blink::ServiceWorkerStatusCode::kOk) {
    Complete(status);
    return;
  }

  if (!existing_registration.get() || existing_registration->is_uninstalled()) {
    RegisterAndContinue();
    return;
  }

  DCHECK(existing_registration->GetNewestVersion());
  // We also compare |script_type| here to proceed with registration when the
  // script type is changed.
  // TODO(asamidoi): Update the spec comment once
  // https://github.com/w3c/ServiceWorker/issues/1359 is resolved.
  // "If scriptURL is equal to registration.[[ScriptURL]] and "update_via_cache
  // is equal to registration.[[update_via_cache]], then:"
  if (existing_registration->GetNewestVersion()->script_url() == script_url_ &&
      existing_registration->update_via_cache() == update_via_cache_ &&
      existing_registration->GetNewestVersion()->script_type() ==
          worker_script_type_) {
    // "Set registration.[[Uninstalling]] to false."
    existing_registration->AbortPendingClear(base::BindOnce(
        &ServiceWorkerRegisterJob::ContinueWithRegistrationForSameScriptUrl,
        weak_factory_.GetWeakPtr(), existing_registration));
    return;
  }

  if (existing_registration->is_uninstalling()) {
    existing_registration->AbortPendingClear(base::BindOnce(
        &ServiceWorkerRegisterJob::ContinueWithUninstallingRegistration,
        weak_factory_.GetWeakPtr(), existing_registration));
    return;
  }

  // "Invoke Set Registration algorithm with job’s scope url and
  // job’s update via cache mode."
  existing_registration->SetUpdateViaCache(update_via_cache_);
  set_registration(existing_registration);
  // "Return the result of running the [[Update]] algorithm, or its equivalent,
  // passing registration as the argument."
  UpdateAndContinue();
}

void ServiceWorkerRegisterJob::ContinueWithUpdate(
    blink::ServiceWorkerStatusCode status,
    scoped_refptr<ServiceWorkerRegistration> existing_registration) {
  DCHECK_EQ(UPDATE_JOB, job_type_);
  if (status != blink::ServiceWorkerStatusCode::kOk) {
    Complete(status);
    return;
  }

  if (existing_registration.get() != registration()) {
    Complete(blink::ServiceWorkerStatusCode::kErrorNotFound);
    return;
  }

  // A previous job may have unregistered this registration.
  if (registration()->is_uninstalling() ||
      !registration()->GetNewestVersion()) {
    Complete(blink::ServiceWorkerStatusCode::kErrorNotFound);
    return;
  }

  DCHECK(script_url_.is_empty());
  script_url_ = registration()->GetNewestVersion()->script_url();
  worker_script_type_ = registration()->GetNewestVersion()->script_type();

  // TODO(michaeln): If the last update check was less than 24 hours
  // ago, depending on the freshness of the cached worker script we
  // may be able to complete the update job right here.

  if (blink::ServiceWorkerUtils::IsImportedScriptUpdateCheckEnabled()) {
    ServiceWorkerVersion* version_to_update =
        registration()->GetNewestVersion();
    std::vector<ServiceWorkerDatabase::ResourceRecord> resources;
    version_to_update->script_cache_map()->GetResources(&resources);
    update_checker_ = std::make_unique<ServiceWorkerUpdateChecker>(
        resources, version_to_update,
        context_->loader_factory_getter()->GetNetworkFactory());
    update_checker_->Start(
        base::BindOnce(&ServiceWorkerRegisterJob::OnUpdateCheckFinished,
                       weak_factory_.GetWeakPtr()));
    return;
  }

  UpdateAndContinue();
}

void ServiceWorkerRegisterJob::OnUpdateCheckFinished(bool script_changed) {
  DCHECK(blink::ServiceWorkerUtils::IsImportedScriptUpdateCheckEnabled());
  if (!script_changed) {
    // TODO(momohatt): Set phase correctly.
    // TODO(momohatt): Update the last update check time correctly.
    ServiceWorkerVersion* newest_version = registration()->GetNewestVersion();
    if (newest_version->force_bypass_cache_for_scripts()) {
      registration()->set_last_update_check(base::Time::Now());
    }
    context_->storage()->UpdateLastUpdateCheckTime(registration());
    ResolvePromise(blink::ServiceWorkerStatusCode::kOk, std::string(),
                   registration());
    // This terminates the current job (|this|).
    Complete(blink::ServiceWorkerStatusCode::kErrorExists,
             "The updated worker is identical to the incumbent.");
    return;
  }

  UpdateAndContinue();
}

// Creates a new ServiceWorkerRegistration.
void ServiceWorkerRegisterJob::RegisterAndContinue() {
  SetPhase(REGISTER);

  int64_t registration_id = context_->storage()->NewRegistrationId();
  if (registration_id == blink::mojom::kInvalidServiceWorkerRegistrationId) {
    Complete(blink::ServiceWorkerStatusCode::kErrorAbort);
    return;
  }

  blink::mojom::ServiceWorkerRegistrationOptions options(
      scope_, worker_script_type_, update_via_cache_);
  set_registration(
      new ServiceWorkerRegistration(options, registration_id, context_));
  AddRegistrationToMatchingProviderHosts(registration());
  UpdateAndContinue();
}

void ServiceWorkerRegisterJob::ContinueWithUninstallingRegistration(
    scoped_refptr<ServiceWorkerRegistration> existing_registration,
    blink::ServiceWorkerStatusCode status) {
  if (status != blink::ServiceWorkerStatusCode::kOk) {
    Complete(status);
    return;
  }
  should_uninstall_on_failure_ = true;
  set_registration(existing_registration);
  UpdateAndContinue();
}

void ServiceWorkerRegisterJob::ContinueWithRegistrationForSameScriptUrl(
    scoped_refptr<ServiceWorkerRegistration> existing_registration,
    blink::ServiceWorkerStatusCode status) {
  if (status != blink::ServiceWorkerStatusCode::kOk) {
    Complete(status);
    return;
  }
  set_registration(existing_registration);

  // "If newestWorker is not null, scriptURL is equal to newestWorker.scriptURL,
  // and job’s update via cache mode's value equals registration’s
  // update via cache mode then:
  // Return a promise resolved with registration."
  // We resolve only if there's an active version. If there's not,
  // then there is either no version or only a waiting version from
  // the last browser session; it makes sense to proceed with registration in
  // either case.
  DCHECK(!existing_registration->installing_version());
  if (existing_registration->active_version()) {
    ResolvePromise(status, std::string(), existing_registration.get());
    Complete(blink::ServiceWorkerStatusCode::kOk);
    return;
  }

  // "Return the result of running the [[Update]] algorithm, or its equivalent,
  // passing registration as the argument."
  UpdateAndContinue();
}

// This function corresponds to the spec's [[Update]] algorithm.
void ServiceWorkerRegisterJob::UpdateAndContinue() {
  SetPhase(UPDATE);
  context_->storage()->NotifyInstallingRegistration(registration());

  int64_t version_id = context_->storage()->NewVersionId();
  if (version_id == blink::mojom::kInvalidServiceWorkerVersionId) {
    Complete(blink::ServiceWorkerStatusCode::kErrorAbort);
    return;
  }

  // "Let worker be a new ServiceWorker object..." and start
  // the worker.
  set_new_version(new ServiceWorkerVersion(
      registration(), script_url_, worker_script_type_, version_id, context_));
  new_version()->set_force_bypass_cache_for_scripts(force_bypass_cache_);

  // Module service workers don't support pause after download so we can't
  // perform script comparison.
  // TODO(asamidoi): Support pause after download in module workers.
  if (worker_script_type_ == blink::mojom::ScriptType::kModule) {
    skip_script_comparison_ = true;
  }

  if (registration()->has_installed_version() && !skip_script_comparison_) {
    new_version()->SetToPauseAfterDownload(
        base::BindOnce(&ServiceWorkerRegisterJob::OnPausedAfterDownload,
                       weak_factory_.GetWeakPtr()));
  }
  new_version()->StartWorker(
      ServiceWorkerMetrics::EventType::INSTALL,
      base::BindOnce(&ServiceWorkerRegisterJob::OnStartWorkerFinished,
                     weak_factory_.GetWeakPtr()));
}

void ServiceWorkerRegisterJob::OnStartWorkerFinished(
    blink::ServiceWorkerStatusCode status) {
  BumpLastUpdateCheckTimeIfNeeded();

  if (status == blink::ServiceWorkerStatusCode::kOk) {
    InstallAndContinue();
    return;
  }

  // "If serviceWorker fails to start up..." then reject the promise with an
  // error and abort.
  if (status == blink::ServiceWorkerStatusCode::kErrorTimeout) {
    Complete(status, "Timed out while trying to start the Service Worker.");
    return;
  }

  const net::URLRequestStatus& main_script_status =
      new_version()->script_cache_map()->main_script_status();
  std::string message;
  if (main_script_status.status() != net::URLRequestStatus::SUCCESS) {
    message = new_version()->script_cache_map()->main_script_status_message();
    if (message.empty())
      message = kServiceWorkerFetchScriptError;
  }
  Complete(status, message);
}

// This function corresponds to the spec's [[Install]] algorithm.
void ServiceWorkerRegisterJob::InstallAndContinue() {
  SetPhase(INSTALL);

  // "Set registration.installingWorker to worker."
  DCHECK(!registration()->installing_version());
  registration()->SetInstallingVersion(new_version());

  // "Run the Update State algorithm passing registration's installing worker
  // and installing as the arguments."
  new_version()->SetStatus(ServiceWorkerVersion::INSTALLING);

  // "Resolve registrationPromise with registration."
  ResolvePromise(blink::ServiceWorkerStatusCode::kOk, std::string(),
                 registration());

  // "Fire a simple event named updatefound..."
  registration()->NotifyUpdateFound();

  // "Fire an event named install..."
  new_version()->RunAfterStartWorker(
      ServiceWorkerMetrics::EventType::INSTALL,
      base::BindOnce(&ServiceWorkerRegisterJob::DispatchInstallEvent,
                     weak_factory_.GetWeakPtr()));

  // A subsequent registration job may terminate our installing worker. It can
  // only do so after we've started the worker and dispatched the install
  // event, as those are atomic substeps in the [[Install]] algorithm.
  if (doom_installing_worker_)
    Complete(blink::ServiceWorkerStatusCode::kErrorInstallWorkerFailed);
}

void ServiceWorkerRegisterJob::DispatchInstallEvent(
    blink::ServiceWorkerStatusCode start_worker_status) {
  if (start_worker_status != blink::ServiceWorkerStatusCode::kOk) {
    OnInstallFailed(start_worker_status);
    return;
  }

  DCHECK_EQ(ServiceWorkerVersion::INSTALLING, new_version()->status())
      << new_version()->status();
  DCHECK_EQ(EmbeddedWorkerStatus::RUNNING, new_version()->running_status())
      << "Worker stopped too soon after it was started.";
  int request_id = new_version()->StartRequest(
      ServiceWorkerMetrics::EventType::INSTALL,
      base::BindOnce(&ServiceWorkerRegisterJob::OnInstallFailed,
                     weak_factory_.GetWeakPtr()));

  new_version()->endpoint()->DispatchInstallEvent(
      base::BindOnce(&ServiceWorkerRegisterJob::OnInstallFinished,
                     weak_factory_.GetWeakPtr(), request_id));
}

void ServiceWorkerRegisterJob::OnInstallFinished(
    int request_id,
    blink::mojom::ServiceWorkerEventStatus event_status,
    bool has_fetch_handler,
    base::TimeTicks dispatch_event_time) {
  bool succeeded =
      event_status == blink::mojom::ServiceWorkerEventStatus::COMPLETED;
  new_version()->FinishRequest(request_id, succeeded, dispatch_event_time);

  if (!succeeded) {
    OnInstallFailed(
        mojo::ConvertTo<blink::ServiceWorkerStatusCode>(event_status));
    return;
  }

  ServiceWorkerMetrics::RecordInstallEventStatus(
      blink::ServiceWorkerStatusCode::kOk);

  SetPhase(STORE);
  DCHECK(!registration()->last_update_check().is_null());
  new_version()->set_fetch_handler_existence(
      has_fetch_handler
          ? ServiceWorkerVersion::FetchHandlerExistence::EXISTS
          : ServiceWorkerVersion::FetchHandlerExistence::DOES_NOT_EXIST);
  context_->storage()->StoreRegistration(
      registration(), new_version(),
      base::BindOnce(&ServiceWorkerRegisterJob::OnStoreRegistrationComplete,
                     weak_factory_.GetWeakPtr()));
}

void ServiceWorkerRegisterJob::OnInstallFailed(
    blink::ServiceWorkerStatusCode status) {
  ServiceWorkerMetrics::RecordInstallEventStatus(status);
  DCHECK_NE(status, blink::ServiceWorkerStatusCode::kOk)
      << "OnInstallFailed should not handle "
         "blink::ServiceWorkerStatusCode::kOk";
  Complete(status, std::string("ServiceWorker failed to install: ") +
                       blink::ServiceWorkerStatusToString(status));
}

void ServiceWorkerRegisterJob::OnStoreRegistrationComplete(
    blink::ServiceWorkerStatusCode status) {
  if (status != blink::ServiceWorkerStatusCode::kOk) {
    Complete(status);
    return;
  }

  // "9. If registration.waitingWorker is not null, then:..."
  if (registration()->waiting_version()) {
    // 1. Set redundantWorker to registration’s waiting worker.
    // 2. Terminate redundantWorker.
    registration()->waiting_version()->StopWorker(base::DoNothing());
    // TODO(falken): Move this further down. The spec says to set status to
    // 'redundant' after promoting the new version to .waiting attribute and
    // 'installed' status.
    registration()->waiting_version()->SetStatus(
        ServiceWorkerVersion::REDUNDANT);
  }

  // "10. Set registration.waitingWorker to registration.installingWorker."
  // "11. Set registration.installingWorker to null."
  registration()->SetWaitingVersion(new_version());

  // "12. Run the [[UpdateState]] algorithm passing registration.waitingWorker
  // and "installed" as the arguments."
  new_version()->SetStatus(ServiceWorkerVersion::INSTALLED);

  // "If registration's waiting worker's skip waiting flag is set:" then
  // activate the worker immediately otherwise "wait until no service worker
  // client is using registration as their service worker registration."
  registration()->ActivateWaitingVersionWhenReady();

  Complete(blink::ServiceWorkerStatusCode::kOk);
}

void ServiceWorkerRegisterJob::Complete(blink::ServiceWorkerStatusCode status) {
  Complete(status, std::string());
}

void ServiceWorkerRegisterJob::Complete(blink::ServiceWorkerStatusCode status,
                                        const std::string& status_message) {
  CompleteInternal(status, status_message);
  context_->job_coordinator()->FinishJob(scope_, this);
}

void ServiceWorkerRegisterJob::CompleteInternal(
    blink::ServiceWorkerStatusCode status,
    const std::string& status_message) {
  SetPhase(COMPLETE);

  if (new_version())
    new_version()->SetToNotPauseAfterDownload();

  if (status != blink::ServiceWorkerStatusCode::kOk) {
    if (registration()) {
      if (should_uninstall_on_failure_)
        registration()->ClearWhenReady();
      if (new_version()) {
        if (status == blink::ServiceWorkerStatusCode::kErrorExists)
          new_version()->SetStartWorkerStatusCode(
              blink::ServiceWorkerStatusCode::kErrorExists);
        else
          new_version()->ReportError(status, status_message);
        registration()->UnsetVersion(new_version());
        new_version()->Doom();
      }
      if (!registration()->waiting_version() &&
          !registration()->active_version()) {
        registration()->NotifyRegistrationFailed();
        context_->storage()->DeleteRegistration(
            registration()->id(), registration()->scope().GetOrigin(),
            base::DoNothing());
      }
    }
    if (!is_promise_resolved_)
      ResolvePromise(status, status_message, nullptr);
  }
  DCHECK(callbacks_.empty());
  if (registration()) {
    context_->storage()->NotifyDoneInstallingRegistration(
        registration(), new_version(), status);
    if (registration()->has_installed_version())
      registration()->set_is_uninstalled(false);
  }
}

void ServiceWorkerRegisterJob::ResolvePromise(
    blink::ServiceWorkerStatusCode status,
    const std::string& status_message,
    ServiceWorkerRegistration* registration) {
  DCHECK(!is_promise_resolved_);

  is_promise_resolved_ = true;
  promise_resolved_status_ = status;
  promise_resolved_status_message_ = status_message,
  promise_resolved_registration_ = registration;
  for (RegistrationCallback& callback : callbacks_)
    std::move(callback).Run(status, status_message, registration);
  callbacks_.clear();
}

void ServiceWorkerRegisterJob::AddRegistrationToMatchingProviderHosts(
    ServiceWorkerRegistration* registration) {
  DCHECK(registration);
  for (std::unique_ptr<ServiceWorkerContextCore::ProviderHostIterator> it =
           context_->GetClientProviderHostIterator(
               registration->scope().GetOrigin(),
               true /* include_reserved_clients */);
       !it->IsAtEnd(); it->Advance()) {
    ServiceWorkerProviderHost* host = it->GetProviderHost();
    if (!ServiceWorkerUtils::ScopeMatches(registration->scope(),
                                          host->document_url())) {
      continue;
    }
    host->AddMatchingRegistration(registration);
  }
}

void ServiceWorkerRegisterJob::OnPausedAfterDownload() {
  DCHECK(!blink::ServiceWorkerUtils::IsImportedScriptUpdateCheckEnabled());
  net::URLRequestStatus status =
      new_version()->script_cache_map()->main_script_status();
  if (!status.is_success()) {
    // OnPausedAfterDownload() signifies a successful network load, which
    // translates into a script cache error only in the byte-for-byte identical
    // case.
    DCHECK_EQ(status.error(),
              ServiceWorkerWriteToCacheJob::kIdenticalScriptError);

    BumpLastUpdateCheckTimeIfNeeded();
    ResolvePromise(blink::ServiceWorkerStatusCode::kOk, std::string(),
                   registration());
    // Note: Complete() destroys this job and hence risks destroying
    // |new_version()| inside this callback, which is impolite and dangerous.
    // But most of this class operates this way, and introducing asynchronicity
    // here can potentially create bad interactions with other callbacks into
    // this class, so continue the pattern here. This code path should be
    // removed anyway when the byte-to-byte update check is moved outside of
    // worker startup.
    Complete(blink::ServiceWorkerStatusCode::kErrorExists,
             "The updated worker is identical to the incumbent.");
    return;
  }

  new_version()->embedded_worker()->ResumeAfterDownload();
}

void ServiceWorkerRegisterJob::BumpLastUpdateCheckTimeIfNeeded() {
  // Bump the last update check time only when the register/update job fetched
  // the version having bypassed the network cache. We assume that the
  // BYPASS_CACHE flag evicts an existing cache entry, so even if the install
  // ultimately failed for whatever reason, we know the version in the HTTP
  // cache is not stale, so it's OK to bump the update check time.
  if (new_version()->embedded_worker()->network_accessed_for_script() ||
      new_version()->force_bypass_cache_for_scripts() ||
      registration()->last_update_check().is_null()) {
    registration()->set_last_update_check(base::Time::Now());

    if (registration()->has_installed_version())
      context_->storage()->UpdateLastUpdateCheckTime(registration());
  }
}

}  // namespace content
