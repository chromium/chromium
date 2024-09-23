// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_register_job.h"

#include <stdint.h>

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/devtools/devtools_throttle_handle.h"
#include "content/browser/devtools/service_worker_devtools_manager.h"
#include "content/browser/renderer_host/private_network_access_util.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/service_worker/embedded_worker_instance.h"
#include "content/browser/service_worker/service_worker_client.h"
#include "content/browser/service_worker/service_worker_consts.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_job_coordinator.h"
#include "content/browser/service_worker/service_worker_metrics.h"
#include "content/browser/service_worker/service_worker_new_script_fetcher.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_registry.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/browser/worker_host/worker_script_fetcher.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_client.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/ip_address_space_util.h"
#include "services/network/public/mojom/client_security_state.mojom-forward.h"
#include "third_party/blink/public/common/service_worker/embedded_worker_status.h"
#include "third_party/blink/public/common/service_worker/service_worker_scope_match.h"
#include "third_party/blink/public/common/service_worker/service_worker_type_converters.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_object.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration_options.mojom.h"

namespace content {

typedef ServiceWorkerRegisterJobBase::RegistrationJobType RegistrationJobType;

ServiceWorkerRegisterJob::ServiceWorkerRegisterJob(
    ServiceWorkerContextCore* context,
    const GURL& script_url,
    const blink::mojom::ServiceWorkerRegistrationOptions& options,
    const blink::StorageKey& key,
    blink::mojom::FetchClientSettingsObjectPtr
        outside_fetch_client_settings_object,
    const GlobalRenderFrameHostId& requesting_frame_id,
    blink::mojom::AncestorFrameType ancestor_frame_type,
    PolicyContainerPolicies policy_container_policies)
    : context_(context),
      job_type_(REGISTRATION_JOB),
      scope_(options.scope),
      script_url_(script_url),
      key_(key),
      worker_script_type_(options.type),
      update_via_cache_(options.update_via_cache),
      outside_fetch_client_settings_object_(
          std::move(outside_fetch_client_settings_object)),
      phase_(INITIAL),
      is_promise_resolved_(false),
      should_uninstall_on_failure_(false),
      force_bypass_cache_(false),
      skip_script_comparison_(false),
      promise_resolved_status_(blink::ServiceWorkerStatusCode::kOk),
      requesting_frame_id_(requesting_frame_id),
      ancestor_frame_type_(ancestor_frame_type),
      creator_policy_container_policies_(std::move(policy_container_policies)) {
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
      key_(registration->key()),
      update_via_cache_(registration->update_via_cache()),
      outside_fetch_client_settings_object_(
          std::move(outside_fetch_client_settings_object)),
      phase_(INITIAL),
      is_promise_resolved_(false),
      should_uninstall_on_failure_(false),
      force_bypass_cache_(force_bypass_cache),
      skip_script_comparison_(skip_script_comparison),
      promise_resolved_status_(blink::ServiceWorkerStatusCode::kOk),
      ancestor_frame_type_(registration->ancestor_frame_type()) {
  DCHECK(context_);
  DCHECK(outside_fetch_client_settings_object_);
  internal_.registration = registration;

  ServiceWorkerVersion* version = registration->GetNewestVersion();
  if (version) {
    scoped_refptr<PolicyContainerHost> policy_container_host =
        version->policy_container_host();
    if (policy_container_host) {
      creator_policy_container_policies_ =
          mojo::Clone(policy_container_host->policies());
    }
  }
}

ServiceWorkerRegisterJob::~ServiceWorkerRegisterJob() {
  DCHECK(phase_ == INITIAL || phase_ == COMPLETE || phase_ == ABORT)
      << "Jobs should only be interrupted during shutdown.";
}

void ServiceWorkerRegisterJob::AddCallback(RegistrationCallback callback) {
  if (!is_promise_resolved_) {
    callbacks_.emplace_back(std::move(callback));
    return;
  }
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
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
  const auto traits = (job_type_ == REGISTRATION_JOB)
                          ? BrowserTaskTraits{}
                          : BrowserTaskTraits{base::TaskPriority::BEST_EFFORT};
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  GetUIThreadTaskRunner(traits)->PostTask(
      FROM_HERE, base::BindOnce(&ServiceWorkerRegisterJob::StartImpl,
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
      context_->registry()->GetUninstallingRegistration(scope_, key_);
  if (registration.get())
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(next_step),
                       blink::ServiceWorkerStatusCode::kOk, registration));
  else
    context_->registry()->FindRegistrationForScope(scope_, key_,
                                                   std::move(next_step));
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
  return register_job->scope_ == scope_ && register_job->key_ == key_ &&
         register_job->update_via_cache_ == update_via_cache_ &&
         register_job->script_url_ == script_url_ &&
         register_job->worker_script_type_ == worker_script_type_;
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
      NOTREACHED_IN_MIGRATION();
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

// This function corresponds to the steps in the [[Register]] algorithm.
// https://w3c.github.io/ServiceWorker/#register-algorithm
// "4. Let registration be the result of running the Get Registration algorithm
// passing job’s scope url as the argument."
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
  // "5.2. If newestWorker is not null, job’s script url equals newestWorker’s
  // script url, job’s worker type equals newestWorker’s type, and job’s update
  // via cache mode's value equals registration’s update via cache mode, then:"
  if (existing_registration->GetNewestVersion()->script_url() == script_url_ &&
      existing_registration->GetNewestVersion()->script_type() ==
          worker_script_type_ &&
      existing_registration->update_via_cache() == update_via_cache_) {
    // Subsequent spec steps (5.2.1-5.2.2) are implemented in
    // ContinueWithRegistrationWithSameRegistrationOptions().
    existing_registration->AbortPendingClear(
        base::BindOnce(&ServiceWorkerRegisterJob::
                           ContinueWithRegistrationWithSameRegistrationOptions,
                       weak_factory_.GetWeakPtr(), existing_registration));
    return;
  }

  if (existing_registration->is_uninstalling()) {
    existing_registration->AbortPendingClear(base::BindOnce(
        &ServiceWorkerRegisterJob::ContinueWithUninstallingRegistration,
        weak_factory_.GetWeakPtr(), existing_registration));
    return;
  }

  // "6.1. Invoke Set Registration algorithm with job’s scope url and job’s
  // update via cache mode."
  existing_registration->SetUpdateViaCache(update_via_cache_);
  set_registration(existing_registration);
  // "7. Invoke Update algorithm passing job as the argument."
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

void ServiceWorkerRegisterJob::OnUpdateCheckFinished(
    ServiceWorkerSingleScriptUpdateChecker::Result result,
    std::unique_ptr<ServiceWorkerSingleScriptUpdateChecker::FailureInfo>
        failure_info,
    const std::map<GURL, std::string>& updated_sha256_script_checksums) {
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
    if (updated_sha256_script_checksums.size() > 0) {
      // Update checksums on cached scripts and set it to the map.
      base::flat_map<int64_t, std::string> updated_checksum_map;
      ServiceWorkerScriptCacheMap& cache_map =
          *registration()->GetNewestVersion()->script_cache_map();
      for (const auto& item : updated_sha256_script_checksums) {
        cache_map.UpdateSha256Checksum(item.first, item.second);
        const int64_t resource_id = cache_map.LookupResourceId(item.first);
        updated_checksum_map.emplace(resource_id, item.second);
      }
      // Update resource list on the database. Pass a no-op callback as the
      // checksums are only used for an optimization and we don't need to wait
      // for the completion.
      context_->registry()->UpdateResourceSha256Checksums(
          registration()->id(), key_, updated_checksum_map,
          base::BindOnce([](blink::ServiceWorkerStatusCode status) {
            UMA_HISTOGRAM_ENUMERATION(
                "ServiceWorker.UpdateResourceSha256ChecksumsResult", status);
          }));
    }
    ResolvePromise(blink::ServiceWorkerStatusCode::kOk, std::string(),
                   registration());
    // This terminates the current job (|this|).
    Complete(blink::ServiceWorkerStatusCode::kErrorExists,
             "The updated worker is identical to the incumbent.");
    return;
  }

  context_->registry()->NotifyInstallingRegistration(registration());
  context_->registry()->CreateNewVersion(
      registration(), script_url_, worker_script_type_,
      base::BindOnce(&ServiceWorkerRegisterJob::StartWorkerForUpdate,
                     weak_factory_.GetWeakPtr()));
}

// Creates a new ServiceWorkerRegistration.
void ServiceWorkerRegisterJob::RegisterAndContinue() {
  SetPhase(REGISTER);

  blink::mojom::ServiceWorkerRegistrationOptions options(
      scope_, worker_script_type_, update_via_cache_);
  context_->registry()->CreateNewRegistration(
      options, key_, ancestor_frame_type_,
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

void ServiceWorkerRegisterJob::
    ContinueWithRegistrationWithSameRegistrationOptions(
        scoped_refptr<ServiceWorkerRegistration> existing_registration,
        blink::ServiceWorkerStatusCode status) {
  if (status != blink::ServiceWorkerStatusCode::kOk) {
    Complete(status);
    return;
  }
  set_registration(existing_registration);

  // We resolve only if there's an active version. If there's not,
  // then there is either no version or only a waiting version from
  // the last browser session; it makes sense to proceed with registration in
  // either case.
  DCHECK(!existing_registration->installing_version());
  if (existing_registration->active_version()) {
    // "5.2.1. Invoke Resolve Job Promise with job and registration."
    ResolvePromise(status, std::string(), existing_registration.get());
    // "5.2.2. Invoke Finish Job with job and abort these steps."
    Complete(blink::ServiceWorkerStatusCode::kOk);
    return;
  }

  // "7. Invoke Update algorithm passing job as the argument."
  UpdateAndContinue();
}

void ServiceWorkerRegisterJob::
    MaybeThrottleForDevToolsBeforeStartingScriptFetch(
        scoped_refptr<ServiceWorkerVersion> version) {
  int64_t version_id = version->version_id();
  const GURL& script_url = version->script_url();
  const GURL& scope = version->scope();
  auto devtools_throttle_handle = base::MakeRefCounted<DevToolsThrottleHandle>(
      base::BindOnce(&ServiceWorkerRegisterJob::StartScriptFetchForNewWorker,
                     weak_factory_.GetWeakPtr(), std::move(version)));

  // We are about to start fetching from the browser process and we want
  // devtools to be able to instrument the URLLoaderFactory. This call will
  // create a DevtoolsAgentHost.
  ServiceWorkerDevToolsManager::GetInstance()->WorkerMainScriptFetchingStarting(
      context_->wrapper(), version_id, script_url, scope, requesting_frame_id_,
      std::move(devtools_throttle_handle));
}

void ServiceWorkerRegisterJob::StartScriptFetchForNewWorker(
    scoped_refptr<ServiceWorkerVersion> version) {
  DCHECK(!new_script_fetcher_);

  scoped_refptr<network::SharedURLLoaderFactory> loader_factory =
      context_->wrapper()->GetLoaderFactoryForMainScriptFetch(
          version->scope(), version->version_id(),
          network::mojom::ClientSecurityState::New(
              creator_policy_container_policies_.cross_origin_embedder_policy,
              creator_policy_container_policies_.is_web_secure_context,
              creator_policy_container_policies_.ip_address_space,
              DerivePrivateNetworkRequestPolicy(
                  creator_policy_container_policies_,
                  PrivateNetworkRequestContext::kWorker),
              creator_policy_container_policies_.document_isolation_policy));

  new_script_fetcher_ = std::make_unique<ServiceWorkerNewScriptFetcher>(
      *context_, version, std::move(loader_factory),
      outside_fetch_client_settings_object_.Clone(), requesting_frame_id_);
  new_script_fetcher_->Start(
      base::BindOnce(&ServiceWorkerRegisterJob::OnScriptFetchCompleted,
                     weak_factory_.GetWeakPtr(), std::move(version)));
}

void ServiceWorkerRegisterJob::OnScriptFetchCompleted(
    scoped_refptr<ServiceWorkerVersion> version,
    blink::mojom::WorkerMainScriptLoadParamsPtr main_script_load_params) {
  if (!main_script_load_params) {
    // Null `main_script_load_params` means the main script failed to be loaded.
    ServiceWorkerDevToolsManager::GetInstance()->WorkerMainScriptFetchingFailed(
        context_->wrapper(), version->version_id());

    // Use DeduceStartWorkerFailureReason() because it returns an error code
    // based on the main script's net error.
    std::string message =
        version->script_cache_map()->main_script_status_message();
    if (message.empty())
      message = ServiceWorkerConsts::kServiceWorkerFetchScriptError;
    blink::ServiceWorkerStatusCode script_fetch_status_code =
        version->DeduceStartWorkerFailureReason(
            blink::ServiceWorkerStatusCode::kErrorFailed);
    Complete(script_fetch_status_code, message);
    if (script_fetch_status_code ==
            blink::ServiceWorkerStatusCode::kErrorNetwork &&
        version->scope().SchemeIs("chrome-extension")) {
      base::UmaHistogramSparse(
          "Extensions.ServiceWorkerBackground.WorkerScriptFetchNetError",
          (int)version->GetMainScriptNetError());
    }
    return;
  }

  GURL final_response_url = WorkerScriptFetcher::DetermineFinalResponseUrl(
      version->script_url(), main_script_load_params.get());

  network::mojom::IPAddressSpace response_address_space =
      network::CalculateResourceAddressSpace(
          final_response_url,
          main_script_load_params->response_head->remote_endpoint);

  auto* requesting_render_frame_host =
      RenderFrameHostImpl::FromID(requesting_frame_id_);
  // requesting_render_frame_host can be null in many payment tests
  if (requesting_render_frame_host &&
      network::IsLessPublicAddressSpace(
          response_address_space,
          creator_policy_container_policies_.ip_address_space)) {
    GetContentClient()->browser()->LogWebFeatureForCurrentPage(
        requesting_render_frame_host,
        blink::mojom::WebFeature::kPrivateNetworkAccessFetchedWorkerScript);
  }

  version->set_main_script_load_params(std::move(main_script_load_params));
  StartWorkerForUpdate(std::move(version));
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

  if (GetContentClient()
          ->browser()
          ->ShouldServiceWorkerInheritPolicyContainerFromCreator(script_url_)) {
    new_version()->set_policy_container_host(
        base::MakeRefCounted<PolicyContainerHost>(
            std::move(creator_policy_container_policies_)));
  }

  if (update_checker_) {
    new_version()->PrepareForUpdate(update_checker_->TakeComparedResults(),
                                    update_checker_->updated_script_url(),
                                    update_checker_->policy_container_host());
    update_checker_.reset();
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

  scoped_refptr<network::SharedURLLoaderFactory> loader_factory =
      context_->wrapper()->GetLoaderFactoryForUpdateCheck(
          scope_,
          network::mojom::ClientSecurityState::New(
              creator_policy_container_policies_.cross_origin_embedder_policy,
              creator_policy_container_policies_.is_web_secure_context,
              creator_policy_container_policies_.ip_address_space,
              DerivePrivateNetworkRequestPolicy(
                  creator_policy_container_policies_,
                  PrivateNetworkRequestContext::kWorker),
              creator_policy_container_policies_.document_isolation_policy));
  if (!loader_factory) {
    // We can't continue with update checking appropriately without
    // |loader_factory|. Null |loader_factory| means that the storage partition
    // was not available probably because it's shutting down.
    // This terminates the current job (|this|).
    Complete(blink::ServiceWorkerStatusCode::kErrorAbort,
             ServiceWorkerConsts::kShutdownErrorMessage);
    return;
  }

  if (!IsUpdateCheckNeeded()) {
    context_->registry()->NotifyInstallingRegistration(registration());
    base::OnceCallback<void(scoped_refptr<ServiceWorkerVersion>)> next_task =
        base::BindOnce(&ServiceWorkerRegisterJob::
                           MaybeThrottleForDevToolsBeforeStartingScriptFetch,
                       weak_factory_.GetWeakPtr());
    context_->registry()->CreateNewVersion(
        registration(), script_url_, worker_script_type_,
        std::move(next_task));
    return;
  }

  ServiceWorkerVersion* version_to_update = registration()->GetNewestVersion();
  base::TimeDelta time_since_last_check =
      base::Time::Now() - registration()->last_update_check();
  std::vector<storage::mojom::ServiceWorkerResourceRecordPtr> resources =
      version_to_update->script_cache_map()->GetResources();
  int64_t script_resource_id =
      version_to_update->script_cache_map()->LookupResourceId(script_url_);
  DCHECK_NE(script_resource_id, blink::mojom::kInvalidServiceWorkerResourceId);
  const std::optional<std::string> script_sha256_chekcsum =
      version_to_update->script_cache_map()->LookupSha256Checksum(script_url_);

  update_checker_ = std::make_unique<ServiceWorkerUpdateChecker>(
      std::move(resources), script_url_, script_resource_id,
      script_sha256_chekcsum, version_to_update, std::move(loader_factory),
      force_bypass_cache_, worker_script_type_,
      registration()->update_via_cache(), time_since_last_check, context_,
      outside_fetch_client_settings_object_.Clone());
  update_checker_->Start(
      base::BindOnce(&ServiceWorkerRegisterJob::OnUpdateCheckFinished,
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
  DCHECK_EQ(blink::EmbeddedWorkerStatus::kRunning,
            new_version()->running_status())
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
  context_->job_coordinator()->FinishJob(scope_, key_, this);
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
          const char* const scope = scope_.spec().c_str();
          const char* const script_url = script_url_.spec().c_str();
          const std::string error_prefix =
              job_type_ == REGISTRATION_JOB
                  ? base::StringPrintf(
                        ServiceWorkerConsts::kServiceWorkerRegisterErrorPrefix,
                        scope, script_url)
                  : base::StringPrintf(
                        ServiceWorkerConsts::kServiceWorkerUpdateErrorPrefix,
                        scope, script_url);
          new_version()->ReportError(status, error_prefix + status_message);
        }
        registration()->UnsetVersion(new_version());
        new_version()->Doom();
      }
      if (!registration()->newest_installed_version()) {
        registration()->NotifyRegistrationFailed();
        if (!registration()->is_deleted()) {
          context_->registry()->DeleteRegistration(registration(),
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
  for (auto it =
           context_->service_worker_client_owner().GetServiceWorkerClients(
               registration->key(), true /* include_reserved_clients */,
               true /* include_back_forward_cached_clients */);
       !it.IsAtEnd(); ++it) {
    if (!blink::ServiceWorkerScopeMatches(registration->scope(),
                                          it->GetUrlForScopeMatch())) {
      continue;
    }
    it->AddMatchingRegistration(registration);
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
          registration()->id(), registration()->key(),
          registration()->last_update_check(),
          base::BindOnce([](blink::ServiceWorkerStatusCode status) {
            // Ignore errors; bumping the update check time is just best-effort.
          }));
    }
  }
}

}  // namespace content
