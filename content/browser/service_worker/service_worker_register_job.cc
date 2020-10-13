// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_register_job.h"

#include <stdint.h>

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "content/browser/service_worker/embedded_worker_instance.h"
#include "content/browser/service_worker/embedded_worker_status.h"
#include "content/browser/service_worker/service_worker_consts.h"
#include "content/browser/service_worker/service_worker_container_host.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_job_coordinator.h"
#include "content/browser/service_worker/service_worker_metrics.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_registry.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/browser/url_loader_factory_getter.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/net_errors.h"
#include "third_party/blink/public/common/service_worker/service_worker_scope_match.h"
#include "third_party/blink/public/common/service_worker/service_worker_type_converters.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_object.mojom.h"

namespace content {

typedef ServiceWorkerRegisterJobBase::RegistrationJobType RegistrationJobType;

ServiceWorkerRegisterJob::ServiceWorkerRegisterJob(
    ServiceWorkerContextCore* context,
    const GURL& script_url,
    const blink::mojom::ServiceWorkerRegistrationOptions& options,
    blink::mojom::FetchClientSettingsObjectPtr
        outside_fetch_client_settings_object)
    : context_(context),
      job_type_(REGISTRATION_JOB),
      scope_(options.scope),
      script_url_(script_url),
      worker_script_type_(options.type),
      update_via_cache_(options.update_via_cache),
      outside_fetch_client_settings_object_(
          std::move(outside_fetch_client_settings_object)),
      phase_(INITIAL),
      is_shutting_down_(false),
      is_promise_resolved_(false),
      should_uninstall_on_failure_(false),
      force_bypass_cache_(false),
      skip_script_comparison_(false),
      promise_resolved_status_(blink::ServiceWorkerStatusCode::kOk) {
  DCHECK(context_);
  DCHECK(outside_fetch_client_settings_object_);
}

ServiceWorkerRegisterJob::ServiceWorkerRegisterJob(
    ServiceWorkerContextCore* context,
    ServiceWorkerRegistration* registration,
    bool force_bypass_cache,
    bool skip_script_comparison,
    blink::mojom::FetchClientSettingsObjectPtr
        outside_fetch_client_settings_object)
    : context_(context),
      job_type_(UPDATE_JOB),
      scope_(registration->scope()),
      update_via_cache_(registration->update_via_cache()),
      outside_fetch_client_settings_object_(
          std::move(outside_fetch_client_settings_object)),
      phase_(INITIAL),
      is_shutting_down_(false),
      is_promise_resolved_(false),
      should_uninstall_on_failure_(false),
      force_bypass_cache_(force_bypass_cache),
      skip_script_comparison_(skip_script_comparison),
      promise_resolved_status_(blink::ServiceWorkerStatusCode::kOk) {
  DCHECK(context_);
  DCHECK(outside_fetch_client_settings_object_);
  internal_.registration = registration;
}

ServiceWorkerRegisterJob::~ServiceWorkerRegisterJob() {
  DCHECK(is_shutting_down_ || phase_ == INITIAL || phase_ == COMPLETE ||
         phase_ == ABORT)
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
  // Schedule the job based on job type. For registration, give it the
  // current task priority as it's an explicit JavaScript API and sites
  // may show a message like "offline enabled". For update, give it a lower
  // priority as (soft) update doesn't affect user interactions directly.
  // TODO(bashi): For explicit update() API, we may want to prioritize it too.
  auto traits = (job_type_ == REGISTRATION_JOB)
                    ? base::TaskTraits(ServiceWorkerContext::GetCoreThreadId())
                    : base::TaskTraits(ServiceWorkerContext::GetCoreThreadId(),
                                       base::TaskPriority::BEST_EFFORT);
  base::PostTask(FROM_HERE, std::move(traits),
                 base::BindOnce(&ServiceWorkerRegisterJob::StartImpl,
                                weak_factory_.GetWeakPtr()));
}

void ServiceWorkerRegisterJob::StartImpl() {
  SetPhase(START);
  ServiceWorkerRegistry::FindRegistrationCallback next_step;
  if (job_type_ == REGISTRATION_JOB) {
    next_step =
        base::BindOnce(&ServiceWorkerRegisterJob::ContinueWithRegistration,
                       weak_factory_.GetWeakPtr());
  } else {
    next_step = base::BindOnce(&ServiceWorkerRegisterJob::ContinueWithUpdate,
                               weak_factory_.GetWeakPtr());
  }

  scoped_refptr<ServiceWorkerRegistration> registration =
      context_->registry()->GetUninstallingRegistration(scope_);
  if (registration.get())
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(next_step),
                       blink::ServiceWorkerStatusCode::kOk, registration));
  else
    context_->registry()->FindRegistrationForScope(scope_,
                                                   std::move(next_step));
}

void ServiceWorkerRegisterJob::Abort() {
  SetPhase(ABORT);
  CompleteInternal(blink::ServiceWorkerStatusCode::kErrorAbort, std::string());
  // Don't have to call FinishJob() because the caller takes care of removing
  // the jobs from the queue.
}

void ServiceWorkerRegisterJob::WillShutDown() {
  is_shutting_down_ = true;
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

ServiceWorkerRegisterJob::Internal::Internal() {}

ServiceWorkerRegisterJob::Internal::~Internal() {}

void ServiceWorkerRegisterJob::set_registration(
    scoped_refptr<ServiceWorkerRegistration> registration) {
  DCHECK(phase_ == START || phase_ == REGISTER) << phase_;
  DCHECK(!internal_.registration.get());
  internal_.registration = std::move(registration);
}

ServiceWorkerRegistration* ServiceWorkerRegisterJob::registration() const {
  DCHECK(phase_ >= REGISTER || job_type_ == UPDATE_JOB) << phase_;
  return internal_.registration.get();
}

void ServiceWorkerRegisterJob::set_new_version(
    scoped_refptr<ServiceWorkerVersion> version) {
  DCHECK(phase_ == UPDATE) << phase_;
  DCHECK(!internal_.new_version.get());
  internal_.new_version = std::move(version);
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

  // If the outgoing referrer is not set, use |script_url_| as referrer.
  if (outside_fetch_client_settings_object_->outgoing_referrer.is_empty())
    outside_fetch_client_settings_object_->outgoing_referrer = script_url_;

  // TODO(michaeln): If the last update check was less than 24 hours
  // ago, depending on the freshness of the cached worker script we
  // may be able to complete the update job right here.

  UpdateAndContinue();
}

bool ServiceWorkerRegisterJob::IsUpdateCheckNeeded() const {
  ServiceWorkerVersion* newest_version = registration()->GetNewestVersion();

  // Skip the update check if there is no newest service worker, which means
  // that a new registration is created.
  if (!newest_version)
    return false;

  // Skip the byte-to-byte comparison when either of the script type or the
  // script url is updated.
  if (newest_version->script_url() != script_url_ ||
      newest_version->script_type() != worker_script_type_) {
    DCHECK_EQ(job_type_, REGISTRATION_JOB);
    return false;
  }
  // Need byte-to-byte comparison unless it should be forcefully skipped.
  return !skip_script_comparison_;
}

void ServiceWorkerRegisterJob::TriggerUpdateCheck(
    scoped_refptr<network::SharedURLLoaderFactory> loader_factory) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

  if (!loader_factory) {
    // We can't continue with update checking appropriately without
    // |loader_factory|. Null |loader_factory| means that the storage partition
    // was not available probably because it's shutting down.
    // This terminates the current job (|this|).
    Complete(blink::ServiceWorkerStatusCode::kErrorAbort,
             ServiceWorkerConsts::kShutdownErrorMessage);
    return;
  }

  ServiceWorkerVersion* version_to_update = registration()->GetNewestVersion();
  base::TimeDelta time_since_last_check =
      base::Time::Now() - registration()->last_update_check();
  std::vector<storage::mojom::ServiceWorkerResourceRecordPtr> resources;
  version_to_update->script_cache_map()->GetResources(&resources);
  int64_t script_resource_id =
      version_to_update->script_cache_map()->LookupResourceId(script_url_);
  DCHECK_NE(script_resource_id, blink::mojom::kInvalidServiceWorkerResourceId);

  update_checker_ = std::make_unique<ServiceWorkerUpdateChecker>(
      std::move(resources), script_url_, script_resource_id, version_to_update,
      std::move(loader_factory), force_bypass_cache_,
      registration()->update_via_cache(), time_since_last_check, context_,
      outside_fetch_client_settings_object_.Clone());
  update_checker_->Start(
      base::BindOnce(&ServiceWorkerRegisterJob::OnUpdateCheckFinished,
                     weak_factory_.GetWeakPtr()));
}

void ServiceWorkerRegisterJob::OnUpdateCheckFinished(
    ServiceWorkerSingleScriptUpdateChecker::Result result,
    std::unique_ptr<ServiceWorkerSingleScriptUpdateChecker::FailureInfo>
        failure_info) {
  // Update check failed.
  if (result == ServiceWorkerSingleScriptUpdateChecker::Result::kFailed) {
    DCHECK(failure_info);
    ResolvePromise(failure_info->status, failure_info->error_message, nullptr);
    // This terminates the current job (|this|).
    Complete(failure_info->status, failure_info->error_message);
    return;
  }

  // Update the last update check time.
  BumpLastUpdateCheckTimeIfNeeded();

  // Update check succeeded.
  if (result == ServiceWorkerSingleScriptUpdateChecker::Result::kIdentical) {
    ResolvePromise(blink::ServiceWorkerStatusCode::kOk, std::string(),
                   registration());
    // This terminates the current job (|this|).
    Complete(blink::ServiceWorkerStatusCode::kErrorExists,
             "The updated worker is identical to the incumbent.");
    return;
  }

  CreateNewVersionForUpdate();
}

// Creates a new ServiceWorkerRegistration.
void ServiceWorkerRegisterJob::RegisterAndContinue() {
  SetPhase(REGISTER);

  blink::mojom::ServiceWorkerRegistrationOptions options(
      scope_, worker_script_type_, update_via_cache_);
  context_->registry()->CreateNewRegistration(
      options,
      base::BindOnce(&ServiceWorkerRegisterJob::ContinueWithNewRegistration,
                     weak_factory_.GetWeakPtr()));
}

void ServiceWorkerRegisterJob::ContinueWithNewRegistration(
    scoped_refptr<ServiceWorkerRegistration> new_registration) {
  if (!new_registration) {
    Complete(blink::ServiceWorkerStatusCode::kErrorAbort);
    return;
  }

  set_registration(std::move(new_registration));
  AddRegistrationToMatchingContainerHosts(registration());
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

void ServiceWorkerRegisterJob::CreateNewVersionForUpdate() {
  context_->registry()->NotifyInstallingRegistration(registration());
  context_->registry()->CreateNewVersion(
      registration(), script_url_, worker_script_type_,
      base::BindOnce(&ServiceWorkerRegisterJob::StartWorkerForUpdate,
                     weak_factory_.GetWeakPtr()));
}

void ServiceWorkerRegisterJob::StartWorkerForUpdate(
    scoped_refptr<ServiceWorkerVersion> version) {
  if (!version) {
    Complete(blink::ServiceWorkerStatusCode::kErrorAbort);
    return;
  }
  DCHECK_NE(version->version_id(),
            blink::mojom::kInvalidServiceWorkerVersionId);

  // "Let worker be a new ServiceWorker object..." and start the worker.
  set_new_version(std::move(version));
  new_version()->set_force_bypass_cache_for_scripts(force_bypass_cache_);

  if (update_checker_) {
    new_version()->PrepareForUpdate(
        update_checker_->TakeComparedResults(),
        update_checker_->updated_script_url(),
        update_checker_->cross_origin_embedder_policy());
    update_checker_.reset();
  } else {
    // When the update checker is not used, subresource loader factories needs
    // to be updated after the main script is loaded because COEP header is not
    // available until then. This flag lets the script evaluation wait until the
    // browser sends a message with a new subresoruce loader factories.
    // This happens when this is (1) a new registration, or (2) an old
    // registration where the script URL is changed.
    new_version()->set_initialize_global_scope_after_main_script_loaded();
  }

  new_version()->set_outside_fetch_client_settings_object(
      std::move(outside_fetch_client_settings_object_));

  new_version()->StartWorker(
      ServiceWorkerMetrics::EventType::INSTALL,
      base::BindOnce(&ServiceWorkerRegisterJob::OnStartWorkerFinished,
                     weak_factory_.GetWeakPtr()));
}

// This function corresponds to the spec's [[Update]] algorithm.
void ServiceWorkerRegisterJob::UpdateAndContinue() {
  SetPhase(UPDATE);

  if (!IsUpdateCheckNeeded()) {
    CreateNewVersionForUpdate();
    return;
  }

  // This will start the update check after loader factory is retrieved.
  context_->wrapper()->GetLoaderFactoryForUpdateCheck(
      scope_, base::BindOnce(&ServiceWorkerRegisterJob::TriggerUpdateCheck,
                             weak_factory_.GetWeakPtr()));
}

void ServiceWorkerRegisterJob::OnStartWorkerFinished(
    blink::ServiceWorkerStatusCode status) {
  // Bail out early if the job has already completed.
  // See https://crbug.com/1031764 for details.
  if (phase_ == COMPLETE)
    return;

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

  int main_script_net_error =
      new_version()->script_cache_map()->main_script_net_error();
  std::string message;
  if (main_script_net_error != net::OK) {
    message = new_version()->script_cache_map()->main_script_status_message();
    if (message.empty())
      message = ServiceWorkerConsts::kServiceWorkerFetchScriptError;
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
}

void ServiceWorkerRegisterJob::DispatchInstallEvent(
    blink::ServiceWorkerStatusCode start_worker_status) {
  if (start_worker_status != blink::ServiceWorkerStatusCode::kOk) {
    OnInstallFailed(/*fetch_count=*/0, start_worker_status);
    return;
  }

  DCHECK_EQ(ServiceWorkerVersion::INSTALLING, new_version()->status())
      << new_version()->status();
  DCHECK_EQ(EmbeddedWorkerStatus::RUNNING, new_version()->running_status())
      << "Worker stopped too soon after it was started.";
  int request_id = new_version()->StartRequest(
      ServiceWorkerMetrics::EventType::INSTALL,
      base::BindOnce(&ServiceWorkerRegisterJob::OnInstallFailed,
                     weak_factory_.GetWeakPtr(), /*fetch_count=*/0));

  new_version()->endpoint()->DispatchInstallEvent(
      base::BindOnce(&ServiceWorkerRegisterJob::OnInstallFinished,
                     weak_factory_.GetWeakPtr(), request_id));
}

void ServiceWorkerRegisterJob::OnInstallFinished(
    int request_id,
    blink::mojom::ServiceWorkerEventStatus event_status,
    uint32_t fetch_count) {
  bool succeeded =
      event_status == blink::mojom::ServiceWorkerEventStatus::COMPLETED;
  new_version()->FinishRequestWithFetchCount(request_id, succeeded,
                                             fetch_count);

  if (!succeeded) {
    OnInstallFailed(
        fetch_count,
        mojo::ConvertTo<blink::ServiceWorkerStatusCode>(event_status));
    return;
  }

  ServiceWorkerMetrics::RecordInstallEventStatus(
      blink::ServiceWorkerStatusCode::kOk, fetch_count);

  SetPhase(STORE);
  DCHECK(!registration()->last_update_check().is_null());
  context_->registry()->StoreRegistration(
      registration(), new_version(),
      base::BindOnce(&ServiceWorkerRegisterJob::OnStoreRegistrationComplete,
                     weak_factory_.GetWeakPtr()));
}

void ServiceWorkerRegisterJob::OnInstallFailed(
    uint32_t fetch_count,
    blink::ServiceWorkerStatusCode status) {
  ServiceWorkerMetrics::RecordInstallEventStatus(status, fetch_count);
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

  if (status != blink::ServiceWorkerStatusCode::kOk) {
    if (registration()) {
      if (should_uninstall_on_failure_) {
        registration()->DeleteAndClearWhenReady();
      }
      if (new_version()) {
        if (status == blink::ServiceWorkerStatusCode::kErrorExists) {
          new_version()->SetStartWorkerStatusCode(
              blink::ServiceWorkerStatusCode::kErrorExists);
        } else {
          const char* error_prefix =
              job_type_ == REGISTRATION_JOB
                  ? ServiceWorkerConsts::kServiceWorkerRegisterErrorPrefix
                  : ServiceWorkerConsts::kServiceWorkerUpdateErrorPrefix;
          new_version()->ReportError(
              status, base::StringPrintf(error_prefix, scope_.spec().c_str(),
                                         script_url_.spec().c_str()) +
                          status_message);
        }
        registration()->UnsetVersion(new_version());
        new_version()->Doom();
      }
      if (!registration()->newest_installed_version()) {
        registration()->NotifyRegistrationFailed();
        if (!registration()->is_deleted()) {
          context_->registry()->DeleteRegistration(
              registration(), registration()->scope().GetOrigin(),
              base::DoNothing());
          context_->registry()->NotifyDoneUninstallingRegistration(
              registration(), ServiceWorkerRegistration::Status::kUninstalled);
        }
      }
    }
    if (!is_promise_resolved_)
      ResolvePromise(status, status_message, nullptr);
  }
  DCHECK(callbacks_.empty());
  if (registration()) {
    context_->registry()->NotifyDoneInstallingRegistration(
        registration(), new_version(), status);
#if DCHECK_IS_ON()
    switch (registration()->status()) {
      case ServiceWorkerRegistration::Status::kIntact:
        // The registration must have a version installed, but this job may or
        // may not have succeeded (i.e., may have failed to update).
        DCHECK(registration()->newest_installed_version());
        break;
      case ServiceWorkerRegistration::Status::kUninstalling:
        // This job must have failed. One case this happens is when the
        // registration was already uninstalling when the job started, so it
        // aborted.
        DCHECK_NE(status, blink::ServiceWorkerStatusCode::kOk);
        break;
      case ServiceWorkerRegistration::Status::kUninstalled:
        // This job must have failed.
        DCHECK(!registration()->newest_installed_version());
        DCHECK_NE(status, blink::ServiceWorkerStatusCode::kOk);
        break;
    }
#endif  // DCHECK_IS_ON()
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

void ServiceWorkerRegisterJob::AddRegistrationToMatchingContainerHosts(
    ServiceWorkerRegistration* registration) {
  DCHECK(registration);
  // Include bfcached clients because they need to have the correct
  // information about the matching registrations if, e.g., claim() is called
  // while they are in bfcache or after they are restored from bfcache.
  for (std::unique_ptr<ServiceWorkerContextCore::ContainerHostIterator> it =
           context_->GetClientContainerHostIterator(
               registration->scope().GetOrigin(),
               true /* include_reserved_clients */,
               true /* include_back_forward_cached_clients */);
       !it->IsAtEnd(); it->Advance()) {
    ServiceWorkerContainerHost* container_host = it->GetContainerHost();
    DCHECK(container_host->IsContainerForClient());
    if (!blink::ServiceWorkerScopeMatches(registration->scope(),
                                          container_host->url())) {
      continue;
    }
    container_host->AddMatchingRegistration(registration);
  }
}

void ServiceWorkerRegisterJob::BumpLastUpdateCheckTimeIfNeeded() {
  bool network_accessed = false;
  bool force_bypass_cache = false;

  // Get |network_accessed| from |update_checker_| and |force_bypass_cache|
  // from the current job when the update checker tried to fetch the worker
  // script.
  // |update_checker_| is not available when installing a new
  // service worker without update checking (e.g. a new registration). In this
  // case, get |network_accessed| and |force_bypass_cache| from the new version.
  if (update_checker_) {
    network_accessed = update_checker_->network_accessed();
    force_bypass_cache = force_bypass_cache_;
  } else {
    network_accessed =
        new_version()->embedded_worker()->network_accessed_for_script();
    force_bypass_cache = new_version()->force_bypass_cache_for_scripts();
  }

  // Bump the last update check time only when the register/update job fetched
  // the version having bypassed the network cache. We assume that the
  // BYPASS_CACHE flag evicts an existing cache entry, so even if the install
  // ultimately failed for whatever reason, we know the version in the HTTP
  // cache is not stale, so it's OK to bump the update check time.
  if (network_accessed || force_bypass_cache ||
      registration()->last_update_check().is_null()) {
    registration()->set_last_update_check(base::Time::Now());

    if (registration()->newest_installed_version()) {
      context_->registry()->UpdateLastUpdateCheckTime(
          registration()->id(), registration()->scope().GetOrigin(),
          registration()->last_update_check(),
          base::BindOnce([](blink::ServiceWorkerStatusCode status) {
            // Ignore errors; bumping the update check time is just best-effort.
          }));
    }
  }
}

}  // namespace content
