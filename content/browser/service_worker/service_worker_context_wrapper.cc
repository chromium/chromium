// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_context_wrapper.h"

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/observer_list.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "components/services/storage/service_worker/service_worker_storage_control_impl.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/loader/navigation_url_loader_impl.h"
#include "content/browser/loader/url_loader_factory_utils.h"
#include "content/browser/service_worker/service_worker_client.h"
#include "content/browser/service_worker/service_worker_container_host.h"
#include "content/browser/service_worker/service_worker_host.h"
#include "content/browser/service_worker/service_worker_object_host.h"
#include "content/browser/service_worker/service_worker_process_manager.h"
#include "content/browser/service_worker/service_worker_quota_client.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/webui/web_ui_controller_factory_registry.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/service_worker_context_observer.h"
#include "content/public/browser/service_worker_running_info.h"
#include "content/public/browser/storage_usage_info.h"
#include "content/public/browser/web_ui_url_loader_factory.h"
#include "content/public/browser/webui_config.h"
#include "content/public/browser/webui_config_map.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/origin_util.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/base/url_util.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/network/public/cpp/url_loader_factory_builder.h"
#include "services/network/public/mojom/client_security_state.mojom.h"
#include "storage/browser/quota/quota_client_type.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/quota/special_storage_policy.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_study_settings.h"
#include "third_party/blink/public/common/service_worker/embedded_worker_status.h"
#include "third_party/blink/public/common/service_worker/service_worker_scope_match.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"

namespace content {

namespace {

// Translate a ServiceWorkerVersion::Status to a
// ServiceWorkerRunningInfo::ServiceWorkerVersionStatus.
ServiceWorkerRunningInfo::ServiceWorkerVersionStatus
GetRunningInfoVersionStatusForStatus(
    ServiceWorkerVersion::Status version_status) {
  switch (version_status) {
    case ServiceWorkerVersion::Status::NEW:
      return ServiceWorkerRunningInfo::ServiceWorkerVersionStatus::kNew;
    case ServiceWorkerVersion::Status::INSTALLING:
      return ServiceWorkerRunningInfo::ServiceWorkerVersionStatus::kInstalling;
    case ServiceWorkerVersion::Status::INSTALLED:
      return ServiceWorkerRunningInfo::ServiceWorkerVersionStatus::kInstalled;
    case ServiceWorkerVersion::Status::ACTIVATING:
      return ServiceWorkerRunningInfo::ServiceWorkerVersionStatus::kActivating;
    case ServiceWorkerVersion::Status::ACTIVATED:
      return ServiceWorkerRunningInfo::ServiceWorkerVersionStatus::kActivated;
    case ServiceWorkerVersion::Status::REDUNDANT:
      return ServiceWorkerRunningInfo::ServiceWorkerVersionStatus::kRedundant;
  }
}

const base::FeatureParam<int> kUpdateDelayParam{
    &blink::features::kServiceWorkerUpdateDelay, "update_delay_in_ms", 1000};

void DidFindRegistrationForStartActiveWorker(
    ServiceWorkerContextWrapper::StatusCallback callback,
    blink::ServiceWorkerStatusCode status,
    scoped_refptr<ServiceWorkerRegistration> registration) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (status != blink::ServiceWorkerStatusCode::kOk ||
      !registration->active_version()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       blink::ServiceWorkerStatusCode::kErrorNotFound));
    return;
  }

  registration->active_version()->StartWorker(
      ServiceWorkerMetrics::EventType::UNKNOWN,
      base::BindOnce(
          [](ServiceWorkerContextWrapper::StatusCallback callback,
             blink::ServiceWorkerStatusCode status) {
            base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
                FROM_HERE, base::BindOnce(std::move(callback), status));
          },
          std::move(callback)));
}

void DidStartWorker(scoped_refptr<ServiceWorkerVersion> version,
                    ServiceWorkerContext::StartWorkerCallback info_callback,
                    ServiceWorkerContext::StatusCodeCallback failure_callback,
                    blink::ServiceWorkerStatusCode start_worker_status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (start_worker_status != blink::ServiceWorkerStatusCode::kOk) {
    std::move(failure_callback).Run(start_worker_status);
    return;
  }
  EmbeddedWorkerInstance* instance = version->embedded_worker();
  std::move(info_callback)
      .Run(version->version_id(), instance->process_id(),
           instance->thread_id());
}

void FoundRegistrationForStartWorker(
    ServiceWorkerContext::StartWorkerCallback info_callback,
    ServiceWorkerContext::StatusCodeCallback failure_callback,
    blink::ServiceWorkerStatusCode service_worker_status,
    scoped_refptr<ServiceWorkerRegistration> registration) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (service_worker_status != blink::ServiceWorkerStatusCode::kOk) {
    std::move(failure_callback).Run(service_worker_status);
    return;
  }

  ServiceWorkerVersion* version_ptr = registration->active_version()
                                          ? registration->active_version()
                                          : registration->installing_version();
  // Since FindRegistrationForScope returned
  // blink::ServiceWorkerStatusCode::kOk, there must have been either:
  // - an active version, which optionally might have activated from a waiting
  //   version (as DidFindRegistrationForFindImpl will activate any waiting
  //   version).
  // - or an installing version.
  // However, if the installation is rejected, the installing version can go
  // away by the time we reach here from DidFindRegistrationForFindImpl.
  if (!version_ptr) {
    std::move(failure_callback).Run(service_worker_status);
    return;
  }

  // Note: There might be a remote possibility that |registration|'s |version|
  // might change between here and DidStartWorker, so bind |version| to
  // RunAfterStartWorker.
  scoped_refptr<ServiceWorkerVersion> version =
      base::WrapRefCounted(version_ptr);
  version->RunAfterStartWorker(
      ServiceWorkerMetrics::EventType::EXTERNAL_REQUEST,
      base::BindOnce(&DidStartWorker, version, std::move(info_callback),
                     std::move(failure_callback)));
}

void RunOnceClosure(scoped_refptr<ServiceWorkerContextWrapper> ref_holder,
                    base::OnceClosure task) {
  std::move(task).Run();
}

// Helper class to create a callback that takes blink::ServiceWorkerStatusCode
// as the first parameter and calls the original callback with a boolean of
// whether the status is blink::ServiceWorkerStatusCode::kOk or not.
class WrapResultCallbackToTakeStatusCode {
 public:
  explicit WrapResultCallbackToTakeStatusCode(
      ServiceWorkerContext::ResultCallback callback)
      : callback_(std::move(callback)) {}

  template <typename... Args>
  operator base::OnceCallback<void(blink::ServiceWorkerStatusCode, Args...)>() {
    return Take<Args...>();
  }

 private:
  template <typename... Args>
  base::OnceCallback<void(blink::ServiceWorkerStatusCode, Args...)> Take() {
    return base::BindOnce(
        [](ServiceWorkerContext::ResultCallback callback,
           blink::ServiceWorkerStatusCode status, Args...) {
          std::move(callback).Run(status ==
                                  blink::ServiceWorkerStatusCode::kOk);
        },
        std::move(callback_));
  }

  ServiceWorkerContext::ResultCallback callback_;
};

void RunOrPostTaskOnUIThread(const base::Location& location,
                             base::OnceClosure task) {
  if (BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    std::move(task).Run();
  } else {
    GetUIThreadTaskRunner({})->PostTask(location, std::move(task));
  }
}

}  // namespace

ServiceWorkerContextSynchronousObserverList::
    ServiceWorkerContextSynchronousObserverList() = default;
ServiceWorkerContextSynchronousObserverList::
    ~ServiceWorkerContextSynchronousObserverList() = default;

// static
bool ServiceWorkerContext::ScopeMatches(const GURL& scope, const GURL& url) {
  return blink::ServiceWorkerScopeMatches(scope, url);
}

// static
void ServiceWorkerContext::RunTask(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    const base::Location& from_here,
    ServiceWorkerContext* service_worker_context,
    base::OnceClosure task) {
  auto ref = base::WrapRefCounted(
      static_cast<ServiceWorkerContextWrapper*>(service_worker_context));
  task_runner->PostTask(
      from_here,
      base::BindOnce(&RunOnceClosure, std::move(ref), std::move(task)));
}

// static
base::TimeDelta ServiceWorkerContext::GetUpdateDelay() {
  return base::Milliseconds(kUpdateDelayParam.Get());
}

ServiceWorkerContextWrapper::ServiceWorkerContextWrapper(
    BrowserContext* browser_context)
    : core_observer_list_(
          base::MakeRefCounted<ServiceWorkerContextObserverList>()),
      core_sync_observer_list_(
          base::MakeRefCounted<ServiceWorkerContextSynchronousObserverList>()),
      browser_context_(browser_context),
      process_manager_(std::make_unique<ServiceWorkerProcessManager>()) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Add this object as an observer of the wrapped |context_core_|. This lets us
  // forward observer methods to observers outside of content.
  core_observer_list_->AddObserver(this);

  if (blink::IdentifiabilityStudySettings::Get()->IsActive()) {
    identifiability_metrics_ =
        std::make_unique<ServiceWorkerIdentifiabilityMetrics>();
    core_observer_list_->AddObserver(identifiability_metrics_.get());
  }
}

void ServiceWorkerContextWrapper::Init(
    const base::FilePath& user_data_directory,
    storage::QuotaManagerProxy* quota_manager_proxy,
    storage::SpecialStoragePolicy* special_storage_policy,
    ChromeBlobStorageContext* blob_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(storage_partition_);

  is_incognito_ = user_data_directory.empty();

  user_data_directory_ = user_data_directory;
  quota_manager_proxy_ = quota_manager_proxy;

  InitInternal(quota_manager_proxy, special_storage_policy, blob_context,
               storage_partition_->browser_context());
}

void ServiceWorkerContextWrapper::InitInternal(
    storage::QuotaManagerProxy* quota_manager_proxy,
    storage::SpecialStoragePolicy* special_storage_policy,
    ChromeBlobStorageContext* blob_context,
    BrowserContext* browser_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
      non_network_pending_loader_factory_bundle_for_update_check;
  non_network_pending_loader_factory_bundle_for_update_check =
      CreateNonNetworkPendingURLLoaderFactoryBundleForUpdateCheck(
          browser_context);

  context_core_ = std::make_unique<ServiceWorkerContextCore>(
      quota_manager_proxy, special_storage_policy,
      std::move(non_network_pending_loader_factory_bundle_for_update_check),
      core_observer_list_.get(), core_sync_observer_list_.get(), this);
}

void ServiceWorkerContextWrapper::Shutdown() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  ClearRunningServiceWorkers();
  NotifyRunningServiceWorkerStoppedToSynchronousObserver();
  storage_partition_ = nullptr;
  storage_control_.reset();
  context_core_.reset();
  // Shutdown the `process_manager_` at the end so that the steps above can have
  // a valid browser context pointer through `process_manager_`.
  process_manager_->Shutdown();
  browser_context_ = nullptr;
}

void ServiceWorkerContextWrapper::DeleteAndStartOver() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!context_core_) {
    // The context could be null due to system shutdown or restart failure. In
    // either case, we should not have to recover the system, so just return
    // here.
    return;
  }
  context_core_->DeleteAndStartOver(base::BindOnce(
      &ServiceWorkerContextWrapper::DidDeleteAndStartOver, this));
}

StoragePartitionImpl* ServiceWorkerContextWrapper::storage_partition() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return storage_partition_;
}

void ServiceWorkerContextWrapper::set_storage_partition(
    StoragePartitionImpl* storage_partition) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  storage_partition_ = storage_partition;
  process_manager_->set_storage_partition(storage_partition_);
}

BrowserContext* ServiceWorkerContextWrapper::browser_context() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return browser_context_;
}

void ServiceWorkerContextWrapper::OnRegistrationCompleted(
    int64_t registration_id,
    const GURL& scope,
    const blink::StorageKey& key) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  for (auto& observer : observer_list_)
    observer.OnRegistrationCompleted(scope);
}

void ServiceWorkerContextWrapper::OnRegistrationStored(
    int64_t registration_id,
    const GURL& scope,
    const blink::StorageKey& key) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  for (auto& observer : observer_list_)
    observer.OnRegistrationStored(registration_id, scope);
}

void ServiceWorkerContextWrapper::OnAllRegistrationsDeletedForStorageKey(
    const blink::StorageKey& key) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void ServiceWorkerContextWrapper::OnErrorReported(
    int64_t version_id,
    const GURL& scope,
    const blink::StorageKey& key,
    const ServiceWorkerContextObserver::ErrorInfo& info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  for (auto& observer : observer_list_)
    observer.OnErrorReported(version_id, scope, info);
}

void ServiceWorkerContextWrapper::OnReportConsoleMessage(
    int64_t version_id,
    const GURL& scope,
    const blink::StorageKey& key,
    const ConsoleMessage& message) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  for (auto& observer : observer_list_)
    observer.OnReportConsoleMessage(version_id, scope, message);
}

void ServiceWorkerContextWrapper::OnControlleeAdded(
    int64_t version_id,
    const std::string& client_uuid,
    const ServiceWorkerClientInfo& client_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  for (auto& observer : observer_list_)
    observer.OnControlleeAdded(version_id, client_uuid, client_info);
}

void ServiceWorkerContextWrapper::OnControlleeRemoved(
    int64_t version_id,
    const std::string& client_uuid) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  for (auto& observer : observer_list_)
    observer.OnControlleeRemoved(version_id, client_uuid);
}

void ServiceWorkerContextWrapper::OnNoControllees(
    int64_t version_id,
    const GURL& scope,
    const blink::StorageKey& key) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  for (auto& observer : observer_list_)
    observer.OnNoControllees(version_id, scope);
}

void ServiceWorkerContextWrapper::OnControlleeNavigationCommitted(
    int64_t version_id,
    const std::string& uuid,
    GlobalRenderFrameHostId render_frame_host_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  for (auto& observer : observer_list_)
    observer.OnControlleeNavigationCommitted(version_id, uuid,
                                             render_frame_host_id);
}

void ServiceWorkerContextWrapper::OnWindowOpened(const GURL& script_url,
                                                 const GURL& url) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  for (auto& observer : observer_list_) {
    observer.OnWindowOpened(script_url, url);
  }
}

void ServiceWorkerContextWrapper::OnClientNavigated(const GURL& script_url,
                                                    const GURL& url) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  for (auto& observer : observer_list_) {
    observer.OnClientNavigated(script_url, url);
  }
}

void ServiceWorkerContextWrapper::OnStarting(int64_t version_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  for (auto& observer : observer_list_) {
    observer.OnVersionStartingRunning(version_id);
  }
}

void ServiceWorkerContextWrapper::OnStarted(
    int64_t version_id,
    const GURL& scope,
    int process_id,
    const GURL& script_url,
    const blink::ServiceWorkerToken& token,
    const blink::StorageKey& key) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (is_deleting_and_starting_over_)
    return;

  ServiceWorkerVersion* version = GetLiveVersion(version_id);
  ServiceWorkerRunningInfo::ServiceWorkerVersionStatus version_status =
      version ? GetRunningInfoVersionStatusForStatus(version->status())
              : ServiceWorkerRunningInfo::ServiceWorkerVersionStatus::kUnknown;
  auto insertion_result = running_service_workers_.insert(std::make_pair(
      version_id, ServiceWorkerRunningInfo(script_url, scope, key, process_id,
                                           token, version_status)));
  DCHECK(insertion_result.second);

  const auto& running_info = insertion_result.first->second;
  for (auto& observer : observer_list_)
    observer.OnVersionStartedRunning(version_id, running_info);
}

void ServiceWorkerContextWrapper::OnStopping(int64_t version_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  for (auto& observer : observer_list_) {
    observer.OnVersionStoppingRunning(version_id);
  }
}

void ServiceWorkerContextWrapper::OnStopped(int64_t version_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto it = running_service_workers_.find(version_id);
  if (it != running_service_workers_.end()) {
    running_service_workers_.erase(it);
    for (auto& observer : observer_list_)
      observer.OnVersionStoppedRunning(version_id);
  }
}

void ServiceWorkerContextWrapper::OnDeleteAndStartOver() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  is_deleting_and_starting_over_ = true;
  ClearRunningServiceWorkers();
}

void ServiceWorkerContextWrapper::OnVersionStateChanged(
    int64_t version_id,
    const GURL& scope,
    const blink::StorageKey& key,
    ServiceWorkerVersion::Status status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto it = running_service_workers_.find(version_id);
  if (it != running_service_workers_.end()) {
    it->second.version_status = GetRunningInfoVersionStatusForStatus(status);
  }

  if (status == ServiceWorkerVersion::Status::ACTIVATED) {
    for (auto& observer : observer_list_)
      observer.OnVersionActivated(version_id, scope);
  } else if (status == ServiceWorkerVersion::Status::REDUNDANT) {
    for (auto& observer : observer_list_)
      observer.OnVersionRedundant(version_id, scope);
  }
}

void ServiceWorkerContextWrapper::AddObserver(
    ServiceWorkerContextObserver* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  observer_list_.AddObserver(observer);
}

void ServiceWorkerContextWrapper::RemoveObserver(
    ServiceWorkerContextObserver* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  observer_list_.RemoveObserver(observer);
}

void ServiceWorkerContextWrapper::AddSyncObserver(
    ServiceWorkerContextObserverSynchronous* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  core_sync_observer_list_->observers.AddObserver(observer);
}

void ServiceWorkerContextWrapper::RemoveSyncObserver(
    ServiceWorkerContextObserverSynchronous* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  core_sync_observer_list_->observers.RemoveObserver(observer);
}

void ServiceWorkerContextWrapper::
    NotifyRunningServiceWorkerStoppedToSynchronousObserver() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  for (const auto& kv : running_service_workers_) {
    for (auto& observer : core_sync_observer_list_->observers) {
      observer.OnStopped(/*version_id=*/kv.first, /*worker_info=*/kv.second);
    }
  }
}

void ServiceWorkerContextWrapper::RegisterServiceWorker(
    const GURL& script_url,
    const blink::StorageKey& key,
    const blink::mojom::ServiceWorkerRegistrationOptions& options,
    StatusCodeCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!context_core_) {
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  blink::ServiceWorkerStatusCode::kErrorAbort));
    return;
  }
  blink::mojom::ServiceWorkerRegistrationOptions options_to_pass(
      net::SimplifyUrlForRequest(options.scope), options.type,
      options.update_via_cache);

  // TODO(crbug.com/40056874): initialize remaining fields
  PolicyContainerPolicies policy_container_policies;
  policy_container_policies.is_web_secure_context =
      network::IsUrlPotentiallyTrustworthy(script_url);
  // TODO(bashi): Pass a valid outside fetch client settings object. Perhaps
  // changing this method to take a settings object.
  context()->RegisterServiceWorker(
      net::SimplifyUrlForRequest(script_url), key, options_to_pass,
      blink::mojom::FetchClientSettingsObject::New(
          network::mojom::ReferrerPolicy::kDefault,
          /*outgoing_referrer=*/script_url,
          blink::mojom::InsecureRequestsPolicy::kDoNotUpgrade),
      base::BindOnce(
          [](StatusCodeCallback callback, blink::ServiceWorkerStatusCode status,
             const std::string&, int64_t) { std::move(callback).Run(status); },
          std::move(callback)),
      /*requesting_frame_id=*/GlobalRenderFrameHostId(),
      policy_container_policies);
}

void ServiceWorkerContextWrapper::UnregisterServiceWorker(
    const GURL& scope,
    const blink::StorageKey& key,
    StatusCodeCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  UnregisterServiceWorkerImpl(scope, key, std::move(callback));
}

void ServiceWorkerContextWrapper::UnregisterServiceWorkerImmediately(
    const GURL& scope,
    const blink::StorageKey& key,
    StatusCodeCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  UnregisterServiceWorkerImmediatelyImpl(scope, key, std::move(callback));
}

void ServiceWorkerContextWrapper::UnregisterServiceWorkerImpl(
    const GURL& scope,
    const blink::StorageKey& key,
    StatusCodeCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!context_core_) {
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  blink::ServiceWorkerStatusCode::kErrorAbort));
    return;
  }
  context()->UnregisterServiceWorker(net::SimplifyUrlForRequest(scope), key,
                                     /*is_immediate=*/false,
                                     std::move(callback));
}

void ServiceWorkerContextWrapper::UnregisterServiceWorkerImmediatelyImpl(
    const GURL& scope,
    const blink::StorageKey& key,
    StatusCodeCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!context_core_) {
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  blink::ServiceWorkerStatusCode::kErrorAbort));
    return;
  }
  context()->UnregisterServiceWorker(net::SimplifyUrlForRequest(scope), key,
                                     /*is_immediate=*/true,
                                     std::move(callback));
}

ServiceWorkerExternalRequestResult
ServiceWorkerContextWrapper::StartingExternalRequest(
    int64_t service_worker_version_id,
    ServiceWorkerExternalRequestTimeoutType timeout_type,
    const base::Uuid& request_uuid) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!context())
    return ServiceWorkerExternalRequestResult::kNullContext;
  scoped_refptr<ServiceWorkerVersion> version =
      context()->GetLiveVersion(service_worker_version_id);
  if (!version)
    return ServiceWorkerExternalRequestResult::kWorkerNotFound;
  return version->StartExternalRequest(request_uuid, timeout_type);
}

bool ServiceWorkerContextWrapper::ExecuteScriptForTest(
    const std::string& script,
    int64_t service_worker_version_id,
    ServiceWorkerScriptExecutionCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!context())
    return false;
  scoped_refptr<ServiceWorkerVersion> version =
      context()->GetLiveVersion(service_worker_version_id);
  if (!version)
    return false;
  version->ExecuteScriptForTest(script, std::move(callback));  // IN-TEST
  return true;
}

ServiceWorkerExternalRequestResult
ServiceWorkerContextWrapper::FinishedExternalRequest(
    int64_t service_worker_version_id,
    const base::Uuid& request_uuid) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!context())
    return ServiceWorkerExternalRequestResult::kNullContext;
  scoped_refptr<ServiceWorkerVersion> version =
      context()->GetLiveVersion(service_worker_version_id);
  if (!version)
    return ServiceWorkerExternalRequestResult::kWorkerNotFound;
  return version->FinishExternalRequest(request_uuid);
}

size_t ServiceWorkerContextWrapper::CountExternalRequestsForTest(
    const blink::StorageKey& key) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  std::vector<ServiceWorkerVersionInfo> live_version_info =
      GetAllLiveVersionInfo();
  for (const ServiceWorkerVersionInfo& info : live_version_info) {
    ServiceWorkerVersion* version = GetLiveVersion(info.version_id);
    if (version && version->key() == key) {
      return version->GetExternalRequestCountForTest();  // IN-TEST
    }
  }

  return 0u;
}

bool ServiceWorkerContextWrapper::MaybeHasRegistrationForStorageKey(
    const blink::StorageKey& key) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return context() ? context()->MaybeHasRegistrationForStorageKey(key) : true;
}

void ServiceWorkerContextWrapper::GetAllStorageKeysInfo(
    GetUsageInfoCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!context_core_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), std::vector<StorageUsageInfo>()));
    return;
  }
  context()->registry()->GetAllRegistrationsInfos(base::BindOnce(
      &ServiceWorkerContextWrapper::DidGetAllRegistrationsForGetAllStorageKeys,
      this, std::move(callback)));
}

void ServiceWorkerContextWrapper::DeleteForStorageKey(
    const blink::StorageKey& key,
    ResultCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Ensure the callback is called asynchronously.
  scoped_refptr<base::TaskRunner> callback_runner = GetUIThreadTaskRunner({});
  if (!context_core_) {
    callback_runner->PostTask(FROM_HERE,
                              base::BindOnce(std::move(callback), false));
    return;
  }
  context()->DeleteForStorageKey(
      key,
      base::BindOnce(
          [](ResultCallback callback,
             scoped_refptr<base::TaskRunner> callback_runner,
             blink::ServiceWorkerStatusCode status) {
            callback_runner->PostTask(
                FROM_HERE,
                base::BindOnce(std::move(callback),
                               status == blink::ServiceWorkerStatusCode::kOk));
          },
          std::move(callback), std::move(callback_runner)));
}

void ServiceWorkerContextWrapper::CheckHasServiceWorker(
    const GURL& url,
    const blink::StorageKey& key,
    CheckHasServiceWorkerCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Checking OriginCanAccessServiceWorkers() is for performance optimization.
  // Without this check, following FindRegistrationForClientUrl() can detect if
  // the given URL has service worker registration or not. But
  // FindRegistrationForClientUrl() takes time to compute. Hence avoid calling
  // it when the given URL clearly doesn't register service workers.
  if (!context_core_ || !OriginCanAccessServiceWorkers(url)) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  ServiceWorkerCapability::NO_SERVICE_WORKER));
    return;
  }

  DCHECK(url.is_valid());
  TRACE_EVENT1("ServiceWorker",
               "ServiceWorkerContextWrapper::CheckHasServiceWorker", "url",
               url.spec());

  context()->CheckHasServiceWorker(net::SimplifyUrlForRequest(url), key,
                                   std::move(callback));
}

void ServiceWorkerContextWrapper::ClearAllServiceWorkersForTest(
    base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!context_core_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(callback));
    return;
  }
  context_core_->ClearAllServiceWorkersForTest(std::move(callback));
}

void ServiceWorkerContextWrapper::StartWorkerForScope(
    const GURL& scope,
    const blink::StorageKey& key,
    StartWorkerCallback info_callback,
    StatusCodeCallback failure_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  FindRegistrationForScopeImpl(
      scope, key,
      /*include_installing_version=*/true,
      base::BindOnce(&FoundRegistrationForStartWorker, std::move(info_callback),
                     std::move(failure_callback)));
}

void ServiceWorkerContextWrapper::StartServiceWorkerAndDispatchMessage(
    const GURL& scope,
    const blink::StorageKey& key,
    blink::TransferableMessage message,
    ResultCallback result_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Ensure the callback is called asynchronously.
  auto wrapped_callback = base::BindOnce(
      [](ResultCallback callback, bool success) {
        GetUIThreadTaskRunner({})->PostTask(
            FROM_HERE, base::BindOnce(std::move(callback), success));
      },
      std::move(result_callback));

  // As we don't track tasks between workers and renderers, we can nullify the
  // message's parent task ID.
  message.parent_task_id = std::nullopt;

  // TODO(crbug.com/40820909): Don't post task to the UI thread. Instead,
  // make all call sites run on the UI thread.
  RunOrPostTaskOnUIThread(
      FROM_HERE,
      base::BindOnce(&ServiceWorkerContextWrapper::
                         StartServiceWorkerAndDispatchMessageOnUIThread,
                     this, scope, key, std::move(message),
                     base::BindOnce(
                         [](ResultCallback callback,
                            scoped_refptr<base::TaskRunner> callback_runner,
                            bool success) {
                           callback_runner->PostTask(
                               FROM_HERE,
                               base::BindOnce(std::move(callback), success));
                         },
                         std::move(wrapped_callback),
                         base::SingleThreadTaskRunner::GetCurrentDefault())));
}

void ServiceWorkerContextWrapper::
    StartServiceWorkerAndDispatchMessageOnUIThread(
        const GURL& scope,
        const blink::StorageKey& key,
        blink::TransferableMessage message,
        ResultCallback result_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!context_core_) {
    std::move(result_callback).Run(/*success=*/false);
    return;
  }

  FindRegistrationForScopeImpl(
      net::SimplifyUrlForRequest(scope), key,
      /*include_installing_version=*/false,
      base::BindOnce(
          &ServiceWorkerContextWrapper::DidFindRegistrationForMessageDispatch,
          this, std::move(message), scope, std::move(result_callback)));
}

void ServiceWorkerContextWrapper::DidFindRegistrationForMessageDispatch(
    blink::TransferableMessage message,
    const GURL& source_origin,
    ResultCallback result_callback,
    blink::ServiceWorkerStatusCode service_worker_status,
    scoped_refptr<ServiceWorkerRegistration> registration) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (service_worker_status != blink::ServiceWorkerStatusCode::kOk) {
    LOG(WARNING) << "No registration available, status: "
                 << static_cast<int>(service_worker_status);
    std::move(result_callback).Run(/*success=*/false);
    return;
  }
  registration->active_version()->StartWorker(
      ServiceWorkerMetrics::EventType::MESSAGE,
      base::BindOnce(
          &ServiceWorkerContextWrapper::DidStartServiceWorkerForMessageDispatch,
          this, std::move(message), source_origin, registration,
          std::move(result_callback)));
}

void ServiceWorkerContextWrapper::DidStartServiceWorkerForMessageDispatch(
    blink::TransferableMessage message,
    const GURL& source_origin,
    scoped_refptr<ServiceWorkerRegistration> registration,
    ResultCallback result_callback,
    blink::ServiceWorkerStatusCode status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (status != blink::ServiceWorkerStatusCode::kOk) {
    std::move(result_callback).Run(/*success=*/false);
    return;
  }

  scoped_refptr<ServiceWorkerVersion> version = registration->active_version();

  blink::mojom::ExtendableMessageEventPtr event =
      blink::mojom::ExtendableMessageEvent::New();
  event->message = std::move(message);
  event->source_origin = url::Origin::Create(source_origin);
  event->source_info_for_service_worker =
      version->worker_host()
          ->container_host()
          ->version_object_manager()
          .GetOrCreateHost(version)
          ->CreateCompleteObjectInfoToSend();

  int request_id = version->StartRequest(
      ServiceWorkerMetrics::EventType::MESSAGE,
      WrapResultCallbackToTakeStatusCode(std::move(result_callback)));
  version->endpoint()->DispatchExtendableMessageEvent(
      std::move(event), version->CreateSimpleEventCallback(request_id));
}

void ServiceWorkerContextWrapper::StartServiceWorkerForNavigationHint(
    const GURL& document_url,
    const blink::StorageKey& key,
    StartServiceWorkerForNavigationHintCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!context_core_) {
    std::move(callback).Run(StartServiceWorkerForNavigationHintResult::FAILED);
    return;
  }

  // Checking this is for performance optimization. Without this check,
  // following FindRegistrationForClientUrl() can detect if the given URL has
  // service worker registration or not. But FindRegistrationForClientUrl()
  // takes time to compute. Hence avoid calling it when the given URL clearly
  // doesn't register service workers.
  if (!OriginCanAccessServiceWorkers(document_url)) {
    std::move(callback).Run(StartServiceWorkerForNavigationHintResult::
                                NO_SERVICE_WORKER_REGISTRATION);
    return;
  }

  DCHECK(document_url.is_valid());
  TRACE_EVENT1("ServiceWorker", "StartServiceWorkerForNavigationHint",
               "document_url", document_url.spec());

  context_core_->registry()->FindRegistrationForClientUrl(
      ServiceWorkerRegistry::Purpose::kNotForNavigation,
      net::SimplifyUrlForRequest(document_url), key,
      base::BindOnce(
          &ServiceWorkerContextWrapper::DidFindRegistrationForNavigationHint,
          this, std::move(callback)));
}

void ServiceWorkerContextWrapper::WarmUpServiceWorker(
    const GURL& document_url,
    const blink::StorageKey& key,
    WarmUpServiceWorkerCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!context_core_) {
    std::move(callback).Run();
    return;
  }

  // Checking this is for performance optimization. Without this check,
  // following FindRegistrationForClientUrl() can detect if the given URL has
  // service worker registration or not. But FindRegistrationForClientUrl()
  // takes time to compute. Hence avoid calling it when the given URL clearly
  // doesn't register service workers.
  if (!OriginCanAccessServiceWorkers(document_url)) {
    std::move(callback).Run();
    return;
  }

  // Only warm-up http or https URLs. Do not warm-up extensions and others.
  if (!document_url.SchemeIsHTTPOrHTTPS()) {
    std::move(callback).Run();
    return;
  }

  context_core_->AddWarmUpRequest(document_url, key, std::move(callback));

  // If a service worker warm-up process is already running, do not call
  // `MaybeProcessPendingWarmUpRequest()` here. Instead, expect that
  // `MaybeProcessPendingWarmUpRequest()` will be called at the end of the
  // current warm-up process.
  if (!context_core_->IsProcessingWarmingUp()) {
    MaybeProcessPendingWarmUpRequest();
  }
}

void ServiceWorkerContextWrapper::StopAllServiceWorkersForStorageKey(
    const blink::StorageKey& key) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!context_core_.get()) {
    return;
  }
  std::vector<ServiceWorkerVersionInfo> live_versions = GetAllLiveVersionInfo();
  for (const ServiceWorkerVersionInfo& info : live_versions) {
    ServiceWorkerVersion* version = GetLiveVersion(info.version_id);
    if (version && version->key() == key)
      version->StopWorker(base::DoNothing());
  }
}

void ServiceWorkerContextWrapper::StopAllServiceWorkers(
    base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!context_core_.get()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(callback));
    return;
  }

  std::vector<ServiceWorkerVersionInfo> live_versions = GetAllLiveVersionInfo();
  base::RepeatingClosure barrier =
      base::BarrierClosure(live_versions.size(), std::move(callback));
  for (const ServiceWorkerVersionInfo& info : live_versions) {
    ServiceWorkerVersion* version = GetLiveVersion(info.version_id);
    DCHECK(version);
    version->StopWorker(barrier);
  }
}

const base::flat_map<int64_t, ServiceWorkerRunningInfo>&
ServiceWorkerContextWrapper::GetRunningServiceWorkerInfos() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return running_service_workers_;
}

bool ServiceWorkerContextWrapper::IsLiveStartingServiceWorker(
    int64_t service_worker_version_id) {
  auto* version = GetLiveServiceWorker(service_worker_version_id);
  return (version) ? version->running_status() ==
                         blink::EmbeddedWorkerStatus::kStarting
                   : false;
}

bool ServiceWorkerContextWrapper::IsLiveRunningServiceWorker(
    int64_t service_worker_version_id) {
  auto* version = GetLiveServiceWorker(service_worker_version_id);
  return (version) ? version->running_status() ==
                         blink::EmbeddedWorkerStatus::kRunning
                   : false;
}

service_manager::InterfaceProvider&
ServiceWorkerContextWrapper::GetRemoteInterfaces(
    int64_t service_worker_version_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CHECK(IsLiveStartingServiceWorker(service_worker_version_id) ||
        IsLiveRunningServiceWorker(service_worker_version_id));

  // This function should only be called on live running service workers
  // so it should be safe to dereference the returned pointer without
  // checking it first.
  auto& version = *context()->GetLiveVersion(service_worker_version_id);
  return version.worker_host()->remote_interfaces();
}

blink::AssociatedInterfaceProvider&
ServiceWorkerContextWrapper::GetRemoteAssociatedInterfaces(
    int64_t service_worker_version_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CHECK(IsLiveRunningServiceWorker(service_worker_version_id));

  // This function should only be called on live running service workers
  // so it should be safe to dereference the returned pointer without
  // checking it first.
  auto& version = *context()->GetLiveVersion(service_worker_version_id);
  return *version.associated_interface_provider();
}

std::optional<ServiceWorkerRunningInfo>
ServiceWorkerContextWrapper::GetRunningServiceWorkerInfo(int64_t version_id) {
  const auto search = running_service_workers_.find(version_id);
  return search != running_service_workers_.end()
             ? std::make_optional<ServiceWorkerRunningInfo>(search->second)
             : std::nullopt;
}

scoped_refptr<ServiceWorkerRegistration>
ServiceWorkerContextWrapper::GetLiveRegistration(int64_t registration_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!context_core_)
    return nullptr;
  return context_core_->GetLiveRegistration(registration_id);
}

ServiceWorkerVersion* ServiceWorkerContextWrapper::GetLiveVersion(
    int64_t version_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!context_core_)
    return nullptr;
  return context_core_->GetLiveVersion(version_id);
}

std::vector<ServiceWorkerRegistrationInfo>
ServiceWorkerContextWrapper::GetAllLiveRegistrationInfo() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!context_core_)
    return std::vector<ServiceWorkerRegistrationInfo>();
  return context_core_->GetAllLiveRegistrationInfo();
}

std::vector<ServiceWorkerVersionInfo>
ServiceWorkerContextWrapper::GetAllLiveVersionInfo() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!context_core_)
    return std::vector<ServiceWorkerVersionInfo>();
  return context_core_->GetAllLiveVersionInfo();
}

void ServiceWorkerContextWrapper::HasMainFrameWindowClient(
    const blink::StorageKey& key,
    BoolCallback callback) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!context_core_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
    return;
  }
  context_core_->service_worker_client_owner().HasMainFrameWindowClient(
      key, std::move(callback));
}

std::unique_ptr<std::vector<GlobalRenderFrameHostId>>
ServiceWorkerContextWrapper::GetWindowClientFrameRoutingIds(
    const blink::StorageKey& key) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  std::unique_ptr<std::vector<GlobalRenderFrameHostId>> rfh_ids(
      new std::vector<GlobalRenderFrameHostId>());
  if (!context_core_)
    return rfh_ids;
  for (auto it = context_core_->service_worker_client_owner()
                     .GetWindowServiceWorkerClients(
                         key,
                         /*include_reserved_clients=*/false);
       !it.IsAtEnd(); ++it) {
    DCHECK(it->IsContainerForWindowClient());
    rfh_ids->push_back(it->GetRenderFrameHostId());
  }

  return rfh_ids;
}

void ServiceWorkerContextWrapper::FindReadyRegistrationForClientUrl(
    const GURL& client_url,
    const blink::StorageKey& key,
    FindRegistrationCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!context_core_) {
    std::move(callback).Run(blink::ServiceWorkerStatusCode::kErrorAbort,
                            nullptr);
    return;
  }
  context_core_->registry()->FindRegistrationForClientUrl(
      ServiceWorkerRegistry::Purpose::kNotForNavigation,
      net::SimplifyUrlForRequest(client_url), key,
      base::BindOnce(
          &ServiceWorkerContextWrapper::DidFindRegistrationForFindImpl, this,
          /*include_installing_version=*/false, std::move(callback)));
}

void ServiceWorkerContextWrapper::FindReadyRegistrationForScope(
    const GURL& scope,
    const blink::StorageKey& key,
    FindRegistrationCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!context_core_) {
    std::move(callback).Run(blink::ServiceWorkerStatusCode::kErrorAbort,
                            nullptr);
    return;
  }
  const bool include_installing_version = false;
  context_core_->registry()->FindRegistrationForScope(
      net::SimplifyUrlForRequest(scope), key,
      base::BindOnce(
          &ServiceWorkerContextWrapper::DidFindRegistrationForFindImpl, this,
          include_installing_version, std::move(callback)));
}

void ServiceWorkerContextWrapper::FindRegistrationForScope(
    const GURL& scope,
    const blink::StorageKey& key,
    FindRegistrationCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  const bool include_installing_version = true;
  FindRegistrationForScopeImpl(scope, key, include_installing_version,
                               std::move(callback));
}

void ServiceWorkerContextWrapper::FindReadyRegistrationForId(
    int64_t registration_id,
    const blink::StorageKey& key,
    FindRegistrationCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!context_core_) {
    std::move(callback).Run(blink::ServiceWorkerStatusCode::kErrorAbort,
                            nullptr);
    return;
  }
  context_core_->registry()->FindRegistrationForId(
      registration_id, key,
      base::BindOnce(
          &ServiceWorkerContextWrapper::DidFindRegistrationForFindImpl, this,
          /*include_installing_version=*/false, std::move(callback)));
}

void ServiceWorkerContextWrapper::FindReadyRegistrationForIdOnly(
    int64_t registration_id,
    FindRegistrationCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!context_core_) {
    std::move(callback).Run(blink::ServiceWorkerStatusCode::kErrorAbort,
                            nullptr);
    return;
  }
  context_core_->registry()->FindRegistrationForIdOnly(
      registration_id,
      base::BindOnce(
          &ServiceWorkerContextWrapper::DidFindRegistrationForFindImpl, this,
          /*include_installing_version=*/false, std::move(callback)));
}

void ServiceWorkerContextWrapper::GetAllRegistrations(
    GetRegistrationsInfosCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!context_core_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       blink::ServiceWorkerStatusCode::kErrorAbort,
                       std::vector<ServiceWorkerRegistrationInfo>()));
    return;
  }
  context_core_->registry()->GetAllRegistrationsInfos(std::move(callback));
}

void ServiceWorkerContextWrapper::GetRegistrationsForStorageKey(
    const blink::StorageKey& key,
    GetRegistrationsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!context_core_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            std::move(callback), blink::ServiceWorkerStatusCode::kErrorAbort,
            std::vector<scoped_refptr<ServiceWorkerRegistration>>()));
    return;
  }
  context_core_->registry()->GetRegistrationsForStorageKey(key,
                                                           std::move(callback));
}

void ServiceWorkerContextWrapper::GetRegistrationUserData(
    int64_t registration_id,
    const std::vector<std::string>& keys,
    GetUserDataCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!context_core_) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), std::vector<std::string>(),
                       blink::ServiceWorkerStatusCode::kErrorAbort));
    return;
  }
  context_core_->registry()->GetUserData(registration_id, keys,
                                         std::move(callback));
}

void ServiceWorkerContextWrapper::GetRegistrationUserDataByKeyPrefix(
    int64_t registration_id,
    const std::string& key_prefix,
    GetUserDataCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!context_core_) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), std::vector<std::string>(),
                       blink::ServiceWorkerStatusCode::kErrorAbort));
    return;
  }
  context_core_->registry()->GetUserDataByKeyPrefix(registration_id, key_prefix,
                                                    std::move(callback));
}

void ServiceWorkerContextWrapper::GetRegistrationUserKeysAndDataByKeyPrefix(
    int64_t registration_id,
    const std::string& key_prefix,
    GetUserKeysAndDataCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!context_core_) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  blink::ServiceWorkerStatusCode::kErrorAbort,
                                  base::flat_map<std::string, std::string>()));
    return;
  }
  context_core_->registry()->GetUserKeysAndDataByKeyPrefix(
      registration_id, key_prefix, std::move(callback));
}

void ServiceWorkerContextWrapper::StoreRegistrationUserData(
    int64_t registration_id,
    const blink::StorageKey& key,
    const std::vector<std::pair<std::string, std::string>>& key_value_pairs,
    StatusCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!context_core_) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  blink::ServiceWorkerStatusCode::kErrorAbort));
    return;
  }
  context_core_->registry()->StoreUserData(
      registration_id, key, key_value_pairs, std::move(callback));
}

void ServiceWorkerContextWrapper::ClearRegistrationUserData(
    int64_t registration_id,
    const std::vector<std::string>& keys,
    StatusCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Ensure the callback is called asynchronously.
  if (!context_core_) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  blink::ServiceWorkerStatusCode::kErrorAbort));
    return;
  }
  context_core_->registry()->ClearUserData(registration_id, keys,
                                           std::move(callback));
}

void ServiceWorkerContextWrapper::ClearRegistrationUserDataByKeyPrefixes(
    int64_t registration_id,
    const std::vector<std::string>& key_prefixes,
    StatusCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!context_core_) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  blink::ServiceWorkerStatusCode::kErrorAbort));
    return;
  }
  context_core_->registry()->ClearUserDataByKeyPrefixes(
      registration_id, key_prefixes, std::move(callback));
}

void ServiceWorkerContextWrapper::GetUserDataForAllRegistrations(
    const std::string& key,
    GetUserDataForAllRegistrationsCallback callback) {
  if (!context_core_) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       std::vector<std::pair<int64_t, std::string>>(),
                       blink::ServiceWorkerStatusCode::kErrorAbort));
    return;
  }
  context_core_->registry()->GetUserDataForAllRegistrations(
      key, std::move(callback));
}

void ServiceWorkerContextWrapper::GetUserDataForAllRegistrationsByKeyPrefix(
    const std::string& key_prefix,
    GetUserDataForAllRegistrationsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Ensure the callback is called asynchronously.
  if (!context_core_) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       std::vector<std::pair<int64_t, std::string>>(),
                       blink::ServiceWorkerStatusCode::kErrorAbort));
    return;
  }

  context_core_->registry()->GetUserDataForAllRegistrationsByKeyPrefix(
      key_prefix, std::move(callback));
}

void ServiceWorkerContextWrapper::ClearUserDataForAllRegistrationsByKeyPrefix(
    const std::string& key_prefix,
    StatusCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Ensure the callback is called asynchronously.
  if (!context_core_) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  blink::ServiceWorkerStatusCode::kErrorAbort));
    return;
  }
  context_core_->registry()->ClearUserDataForAllRegistrationsByKeyPrefix(
      key_prefix, std::move(callback));
}

void ServiceWorkerContextWrapper::StartActiveServiceWorker(
    const GURL& scope,
    const blink::StorageKey& key,
    StatusCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!context_core_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  blink::ServiceWorkerStatusCode::kErrorAbort));
    return;
  }
  context_core_->registry()->FindRegistrationForScope(
      net::SimplifyUrlForRequest(scope), key,
      base::BindOnce(&DidFindRegistrationForStartActiveWorker,
                     std::move(callback)));
}

void ServiceWorkerContextWrapper::SkipWaitingWorker(
    const GURL& scope,
    const blink::StorageKey& key) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!context_core_)
    return;
  context_core_->registry()->FindRegistrationForScope(
      net::SimplifyUrlForRequest(scope), key,
      base::BindOnce([](blink::ServiceWorkerStatusCode status,
                        scoped_refptr<ServiceWorkerRegistration> registration) {
        if (status != blink::ServiceWorkerStatusCode::kOk ||
            !registration->waiting_version())
          return;

        registration->waiting_version()->set_skip_waiting(true);
        registration->ActivateWaitingVersionWhenReady();
      }));
}

void ServiceWorkerContextWrapper::UpdateRegistration(
    const GURL& scope,
    const blink::StorageKey& key) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!context_core_)
    return;
  context_core_->registry()->FindRegistrationForScope(
      net::SimplifyUrlForRequest(scope), key,
      base::BindOnce(&ServiceWorkerContextWrapper::DidFindRegistrationForUpdate,
                     this));
}

void ServiceWorkerContextWrapper::SetForceUpdateOnPageLoad(
    bool force_update_on_page_load) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!context_core_)
    return;
  context_core_->set_force_update_on_page_load(force_update_on_page_load);
}

void ServiceWorkerContextWrapper::AddObserver(
    ServiceWorkerContextCoreObserver* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  core_observer_list_->AddObserver(observer);
}

void ServiceWorkerContextWrapper::RemoveObserver(
    ServiceWorkerContextCoreObserver* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  core_observer_list_->RemoveObserver(observer);
}

ServiceWorkerContextWrapper::~ServiceWorkerContextWrapper() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  for (auto& observer : observer_list_)
    observer.OnDestruct(static_cast<ServiceWorkerContext*>(this));

  // Explicitly remove this object as an observer to avoid use-after-frees in
  // tests where this object is not guaranteed to outlive the
  // ServiceWorkerContextCore it wraps.
  core_observer_list_->RemoveObserver(this);
  if (identifiability_metrics_)
    core_observer_list_->RemoveObserver(identifiability_metrics_.get());
}

void ServiceWorkerContextWrapper::FindRegistrationForScopeImpl(
    const GURL& scope,
    const blink::StorageKey& key,
    bool include_installing_version,
    FindRegistrationCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!context_core_) {
    std::move(callback).Run(blink::ServiceWorkerStatusCode::kErrorAbort,
                            nullptr);
    return;
  }
  context_core_->registry()->FindRegistrationForScope(
      net::SimplifyUrlForRequest(scope), key,
      base::BindOnce(
          &ServiceWorkerContextWrapper::DidFindRegistrationForFindImpl, this,
          include_installing_version, std::move(callback)));
}

void ServiceWorkerContextWrapper::MaybeProcessPendingWarmUpRequest() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!context_core_) {
    return;
  }

  context_core_->EndProcessingWarmingUp();

  std::optional<ServiceWorkerContextCore::WarmUpRequest> request =
      context_core_->PopNextWarmUpRequest();

  if (!request) {
    return;
  }

  auto [document_url, key, callback] = std::move(*request);

  DCHECK(document_url.is_valid());
  TRACE_EVENT1("ServiceWorker",
               "ServiceWorkerContextWrapper::MaybeProcessPendingWarmUpRequest",
               "document_url", document_url.spec());

  context_core_->registry()->FindRegistrationForClientUrl(
      ServiceWorkerRegistry::Purpose::kNotForNavigation,
      net::SimplifyUrlForRequest(document_url), key,
      base::BindOnce(&ServiceWorkerContextWrapper::DidFindRegistrationForWarmUp,
                     this, std::move(callback)));
}

void ServiceWorkerContextWrapper::DidFindRegistrationForFindImpl(
    bool include_installing_version,
    FindRegistrationCallback callback,
    blink::ServiceWorkerStatusCode status,
    scoped_refptr<ServiceWorkerRegistration> registration) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (status != blink::ServiceWorkerStatusCode::kOk) {
    std::move(callback).Run(status, nullptr);
    return;
  }

  // Attempt to activate the waiting version because the registration retrieved
  // from the disk might have only the waiting version.
  if (registration->waiting_version())
    registration->ActivateWaitingVersionWhenReady();

  scoped_refptr<ServiceWorkerVersion> active_version =
      registration->active_version();
  if (active_version) {
    if (active_version->status() == ServiceWorkerVersion::ACTIVATING) {
      // Wait until the version is activated.
      active_version->RegisterStatusChangeCallback(base::BindOnce(
          &ServiceWorkerContextWrapper::OnStatusChangedForFindReadyRegistration,
          this, std::move(callback), std::move(registration)));
      return;
    }
    DCHECK_EQ(ServiceWorkerVersion::ACTIVATED, active_version->status());
    std::move(callback).Run(blink::ServiceWorkerStatusCode::kOk,
                            std::move(registration));
    return;
  }

  if (include_installing_version && registration->installing_version()) {
    std::move(callback).Run(blink::ServiceWorkerStatusCode::kOk,
                            std::move(registration));
    return;
  }

  std::move(callback).Run(blink::ServiceWorkerStatusCode::kErrorNotFound,
                          nullptr);
}

void ServiceWorkerContextWrapper::OnStatusChangedForFindReadyRegistration(
    FindRegistrationCallback callback,
    scoped_refptr<ServiceWorkerRegistration> registration) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  scoped_refptr<ServiceWorkerVersion> active_version =
      registration->active_version();
  if (!active_version ||
      active_version->status() != ServiceWorkerVersion::ACTIVATED) {
    std::move(callback).Run(blink::ServiceWorkerStatusCode::kErrorNotFound,
                            nullptr);
    return;
  }
  std::move(callback).Run(blink::ServiceWorkerStatusCode::kOk, registration);
}

void ServiceWorkerContextWrapper::DidDeleteAndStartOver(
    blink::ServiceWorkerStatusCode status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(running_service_workers_.empty());
  is_deleting_and_starting_over_ = false;
  storage_control_.reset();
  if (status != blink::ServiceWorkerStatusCode::kOk) {
    context_core_.reset();
    return;
  }
  context_core_ = std::make_unique<ServiceWorkerContextCore>(
      std::move(context_core_), this);
  DVLOG(1) << "Restarted ServiceWorkerContextCore successfully.";
  context_core_->OnStorageWiped();
}

void ServiceWorkerContextWrapper::DidGetAllRegistrationsForGetAllStorageKeys(
    GetUsageInfoCallback callback,
    blink::ServiceWorkerStatusCode status,
    const std::vector<ServiceWorkerRegistrationInfo>& registrations) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::vector<StorageUsageInfo> usage_infos;

  std::map<blink::StorageKey, StorageUsageInfo> storage_keys;
  for (const auto& registration_info : registrations) {
    blink::StorageKey storage_key = registration_info.key;

    auto it = storage_keys.find(storage_key);
    if (it == storage_keys.end()) {
      storage_keys[storage_key] = StorageUsageInfo(
          storage_key, registration_info.stored_version_size_bytes,
          base::Time());
    } else {
      it->second.total_size_bytes +=
          registration_info.stored_version_size_bytes;
    }
  }

  for (const auto& storage_key_info_pair : storage_keys) {
    usage_infos.push_back(storage_key_info_pair.second);
  }

  std::move(callback).Run(usage_infos);
}

void ServiceWorkerContextWrapper::DidFindRegistrationForUpdate(
    blink::ServiceWorkerStatusCode status,
    scoped_refptr<ServiceWorkerRegistration> registration) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (status != blink::ServiceWorkerStatusCode::kOk)
    return;
  if (!context_core_)
    return;
  DCHECK(registration);
  // TODO(jungkees): |force_bypass_cache| is set to true because the call stack
  // is initiated by an update button on DevTools that expects the cache is
  // bypassed. However, in order to provide options for callers to choose the
  // cache bypass mode, plumb |force_bypass_cache| through to
  // UpdateRegistration().
  context_core_->UpdateServiceWorkerWithoutExecutionContext(
      registration.get(), true /* force_bypass_cache */);
}

void ServiceWorkerContextWrapper::DidFindRegistrationForNavigationHint(
    StartServiceWorkerForNavigationHintCallback callback,
    blink::ServiceWorkerStatusCode status,
    scoped_refptr<ServiceWorkerRegistration> registration) {
  TRACE_EVENT1("ServiceWorker", "DidFindRegistrationForNavigationHint",
               "status", blink::ServiceWorkerStatusToString(status));
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!registration) {
    DCHECK_NE(status, blink::ServiceWorkerStatusCode::kOk);
    std::move(callback).Run(StartServiceWorkerForNavigationHintResult::
                                NO_SERVICE_WORKER_REGISTRATION);
    return;
  }
  if (!registration->active_version()) {
    std::move(callback).Run(StartServiceWorkerForNavigationHintResult::
                                NO_ACTIVE_SERVICE_WORKER_VERSION);
    return;
  }
  if (registration->active_version()->fetch_handler_existence() ==
      ServiceWorkerVersion::FetchHandlerExistence::DOES_NOT_EXIST) {
    std::move(callback).Run(
        StartServiceWorkerForNavigationHintResult::NO_FETCH_HANDLER);
    return;
  }
  if (registration->active_version()->running_status() ==
      blink::EmbeddedWorkerStatus::kRunning) {
    std::move(callback).Run(
        StartServiceWorkerForNavigationHintResult::ALREADY_RUNNING);
    return;
  }

  registration->active_version()->StartWorker(
      ServiceWorkerMetrics::EventType::NAVIGATION_HINT,
      base::BindOnce(
          &ServiceWorkerContextWrapper::DidStartServiceWorkerForNavigationHint,
          this, registration->scope(), std::move(callback)));
}

void ServiceWorkerContextWrapper::DidStartServiceWorkerForNavigationHint(
    const GURL& scope,
    StartServiceWorkerForNavigationHintCallback callback,
    blink::ServiceWorkerStatusCode code) {
  TRACE_EVENT2("ServiceWorker", "DidStartServiceWorkerForNavigationHint", "url",
               scope.spec(), "code", blink::ServiceWorkerStatusToString(code));
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  std::move(callback).Run(
      code == blink::ServiceWorkerStatusCode::kOk
          ? StartServiceWorkerForNavigationHintResult::STARTED
          : StartServiceWorkerForNavigationHintResult::FAILED);
}

void ServiceWorkerContextWrapper::DidFindRegistrationForWarmUp(
    WarmUpServiceWorkerCallback callback,
    blink::ServiceWorkerStatusCode status,
    scoped_refptr<ServiceWorkerRegistration> registration) {
  TRACE_EVENT1("ServiceWorker", "DidFindRegistrationForWarmUp", "status",
               blink::ServiceWorkerStatusToString(status));
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!registration) {
    CHECK_NE(status, blink::ServiceWorkerStatusCode::kOk);
  }
  if (!registration || !registration->active_version() ||
      registration->active_version()->fetch_handler_existence() ==
          ServiceWorkerVersion::FetchHandlerExistence::DOES_NOT_EXIST ||
      registration->active_version()->running_status() ==
          blink::EmbeddedWorkerStatus::kRunning) {
    std::move(callback).Run();

    // This code can be called from `ServiceWorkerVersion::FinishStartWorker`
    // and `ServiceWorkerVersion::OnTimeoutTimer` just before stopping service
    // worker. To avoid start warming up the next warm-up candidate,
    // `MaybeProcessPendingWarmUpRequest` needs to be asynchronously called to
    // wait for stopping the current service worker. (see: http://b/40874535)
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &ServiceWorkerContextWrapper::MaybeProcessPendingWarmUpRequest,
            this));
    return;
  }

  registration->active_version()->StartWorker(
      ServiceWorkerMetrics::EventType::WARM_UP,
      base::BindOnce(&ServiceWorkerContextWrapper::DidWarmUpServiceWorker, this,
                     registration->scope(), std::move(callback)));
}

void ServiceWorkerContextWrapper::DidWarmUpServiceWorker(
    const GURL& scope,
    WarmUpServiceWorkerCallback callback,
    blink::ServiceWorkerStatusCode code) {
  TRACE_EVENT2("ServiceWorker", "DidWarmUpServiceWorker", "url", scope.spec(),
               "code", blink::ServiceWorkerStatusToString(code));
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  std::move(callback).Run();

  // This code can be called from `ServiceWorkerVersion::FinishStartWorker` and
  // `ServiceWorkerVersion::OnTimeoutTimer` just before stopping service worker.
  // To avoid start warming up the next warm-up candidate,
  // `MaybeProcessPendingWarmUpRequest` needs to be asynchronously called to
  // wait for stopping the current service worker. (see: http://b/40874535)
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &ServiceWorkerContextWrapper::MaybeProcessPendingWarmUpRequest,
          this));
}

ServiceWorkerContextCore* ServiceWorkerContextWrapper::context() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return context_core_.get();
}

std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
ServiceWorkerContextWrapper::
    CreateNonNetworkPendingURLLoaderFactoryBundleForUpdateCheck(
        BrowserContext* browser_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ContentBrowserClient::NonNetworkURLLoaderFactoryMap non_network_factories;
  GetContentClient()
      ->browser()
      ->RegisterNonNetworkServiceWorkerUpdateURLLoaderFactories(
          browser_context, &non_network_factories);

  auto factory_bundle =
      std::make_unique<blink::PendingURLLoaderFactoryBundle>();
  for (auto& pair : non_network_factories) {
    const std::string& scheme = pair.first;
    mojo::PendingRemote<network::mojom::URLLoaderFactory>& factory_remote =
        pair.second;

    factory_bundle->pending_scheme_specific_factories().emplace(
        scheme, std::move(factory_remote));
  }

  return factory_bundle;
}

void ServiceWorkerContextWrapper::BindStorageControl(
    mojo::PendingReceiver<storage::mojom::ServiceWorkerStorageControl>
        receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (storage_control_binder_for_test_) {
    storage_control_binder_for_test_.Run(std::move(receiver));
    return;
  }

  // The database task runner is BLOCK_SHUTDOWN in order to support
  // ClearSessionOnlyOrigins() (called due to the "clear on browser exit"
  // content setting).
  // The ServiceWorkerStorageControl receiver runs on thread pool by using
  // |database_task_runner| SequencedTaskRunner.
  // TODO(falken): Only block shutdown for that particular task, when someday
  // task runners support mixing task shutdown behaviors.
  scoped_refptr<base::SequencedTaskRunner> database_task_runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskShutdownBehavior::BLOCK_SHUTDOWN});
  database_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(
          base::IgnoreResult(&storage::ServiceWorkerStorageControlImpl::Create),
          std::move(receiver), user_data_directory_, database_task_runner));
}

void ServiceWorkerContextWrapper::SetStorageControlBinderForTest(
    StorageControlBinder binder) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  storage_control_binder_for_test_ = std::move(binder);
}

void ServiceWorkerContextWrapper::SetForceUpdateOnPageLoadForTesting(
    bool force_update_on_page_load) {
  SetForceUpdateOnPageLoad(force_update_on_page_load);
}

void ServiceWorkerContextWrapper::SetLoaderFactoryForUpdateCheckForTest(
    scoped_refptr<network::SharedURLLoaderFactory> loader_factory) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  loader_factory_for_test_ = std::move(loader_factory);
}

scoped_refptr<network::SharedURLLoaderFactory>
ServiceWorkerContextWrapper::GetLoaderFactoryForUpdateCheck(
    const GURL& scope,
    network::mojom::ClientSecurityStatePtr client_security_state) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // TODO(crbug.com/40767578): Do we want to instrument this with
  // devtools? It is currently not recorded at all.
  return GetLoaderFactoryForBrowserInitiatedRequest(
      scope,
      /*version_id=*/std::nullopt, std::move(client_security_state));
}

scoped_refptr<network::SharedURLLoaderFactory>
ServiceWorkerContextWrapper::GetLoaderFactoryForMainScriptFetch(
    const GURL& scope,
    int64_t version_id,
    network::mojom::ClientSecurityStatePtr client_security_state) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return GetLoaderFactoryForBrowserInitiatedRequest(
      scope, version_id, std::move(client_security_state));
}

scoped_refptr<network::SharedURLLoaderFactory>
ServiceWorkerContextWrapper::GetLoaderFactoryForBrowserInitiatedRequest(
    const GURL& scope,
    std::optional<int64_t> version_id,
    network::mojom::ClientSecurityStatePtr client_security_state) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // TODO(falken): Replace this with URLLoaderInterceptor.
  if (loader_factory_for_test_)
    return loader_factory_for_test_;

  if (!storage_partition()) {
    return nullptr;
  }

  const url::Origin scope_origin = url::Origin::Create(scope);

  mojo::PendingRemote<network::mojom::URLLoaderFactory> remote;
  network::URLLoaderFactoryBuilder factory_builder;
  mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>
      header_client;
  bool bypass_redirect_checks = false;
  // Here we give nullptr for |factory_override|, because CORS is no-op for
  // requests for this factory.
  // TODO(yhirano): Use |factory_override| because someday not just CORS but
  // ORB/CORP will use the factory and those are not no-ops for it
  GetContentClient()->browser()->WillCreateURLLoaderFactory(
      storage_partition_->browser_context(), /*frame=*/nullptr,
      ChildProcessHost::kInvalidUniqueID,
      ContentBrowserClient::URLLoaderFactoryType::kServiceWorkerScript,
      scope_origin, net::IsolationInfo(),
      /*navigation_id=*/std::nullopt, ukm::kInvalidSourceIdObj, factory_builder,
      &header_client, &bypass_redirect_checks,
      /*disable_secure_dns=*/nullptr,
      /*factory_override=*/nullptr,
      /*navigation_response_task_runner=*/nullptr);

  // If we have a version_id, we are fetching a worker main script. We have a
  // DevtoolsAgentHost ready for the worker and we can add the devtools override
  // before instantiating the URLFactoryLoader.
  if (auto params = devtools_instrumentation::WillCreateURLLoaderFactoryParams::
          ForServiceWorkerMainScript(this, version_id)) {
    params->Run(
        /*is_navigation=*/true, /*is_download=*/false, factory_builder,
        /*factory_override=*/nullptr);
  }

  bool use_client_header_factory = header_client.is_valid();
  if (use_client_header_factory) {
    remote = NavigationURLLoaderImpl::CreateURLLoaderFactoryWithHeaderClient(
        std::move(header_client), std::move(factory_builder),
        storage_partition());
  } else {
    DCHECK(storage_partition());
    if (base::FeatureList::IsEnabled(
            features::kPrivateNetworkAccessForWorkers)) {
      if (url_loader_factory::GetTestingInterceptor()) {
        url_loader_factory::GetTestingInterceptor().Run(
            network::mojom::kBrowserProcessId, factory_builder);
      }

      network::mojom::URLLoaderFactoryParamsPtr params =
          storage_partition_->CreateURLLoaderFactoryParams();
      params->client_security_state = std::move(client_security_state);
      remote =
          std::move(factory_builder)
              .Finish<mojo::PendingRemote<network::mojom::URLLoaderFactory>>(
                  storage_partition_->GetNetworkContext(), std::move(params));
    } else {
      // Set up a Mojo connection to the network loader factory if it's not been
      // created yet.
      remote =
          std::move(factory_builder)
              .Finish<mojo::PendingRemote<network::mojom::URLLoaderFactory>>(
                  storage_partition_->GetURLLoaderFactoryForBrowserProcess());
    }
  }

  // Clone context()->loader_factory_bundle_for_update_check() and set up the
  // default factory.
  std::unique_ptr<network::PendingSharedURLLoaderFactory>
      loader_factory_bundle_info =
          context()->loader_factory_bundle_for_update_check()->Clone();

  if (auto* config = content::WebUIConfigMap::GetInstance().GetConfig(
          browser_context(), scope)) {
    // If this is a Service Worker for a WebUI, the WebUI's URLDataSource
    // needs to be registered. Registering a URLDataSource allows the
    // WebUIURLLoaderFactory below to serve the resources for the WebUI. We
    // register the URLDataSource here because the WebUI's resources are
    // needed for the Service Worker update check to be performed which
    // fetches the service worker script.
    //
    // This is similar to how we create a `WebUI` object in
    // RenderFrameHostManager::GetFrameHostForNavigation(). Creating a `WebUI`
    // also creates a `WebUIController` which register the URLDataSource for
    // the WebUI which allows the navigation to be served correctly. We don't
    // create a `WebUI` or a `WebUIController` for WebUI Service Workers so we
    // register the URLDataSource directly.
    if (base::FeatureList::IsEnabled(
            features::kEnableServiceWorkersForChromeScheme) &&
        scope.scheme_piece() == kChromeUIScheme) {
      config->RegisterURLDataSource(browser_context());
      static_cast<blink::PendingURLLoaderFactoryBundle*>(
          loader_factory_bundle_info.get())
          ->pending_scheme_specific_factories()
          .emplace(kChromeUIScheme, CreateWebUIServiceWorkerLoaderFactory(
                                        browser_context(), kChromeUIScheme,
                                        base::flat_set<std::string>()));
    } else if (base::FeatureList::IsEnabled(
                   features::kEnableServiceWorkersForChromeUntrusted) &&
               scope.scheme_piece() == kChromeUIUntrustedScheme) {
      config->RegisterURLDataSource(browser_context());
      static_cast<blink::PendingURLLoaderFactoryBundle*>(
          loader_factory_bundle_info.get())
          ->pending_scheme_specific_factories()
          .emplace(kChromeUIUntrustedScheme,
                   CreateWebUIServiceWorkerLoaderFactory(
                       browser_context(), kChromeUIUntrustedScheme,
                       base::flat_set<std::string>()));
    }
  }

  static_cast<blink::PendingURLLoaderFactoryBundle*>(
      loader_factory_bundle_info.get())
      ->pending_default_factory() = std::move(remote);
  static_cast<blink::PendingURLLoaderFactoryBundle*>(
      loader_factory_bundle_info.get())
      ->set_bypass_redirect_checks(bypass_redirect_checks);
  return network::SharedURLLoaderFactory::Create(
      std::move(loader_factory_bundle_info));
}

void ServiceWorkerContextWrapper::ClearRunningServiceWorkers() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  for (const auto& kv : running_service_workers_) {
    int64_t version_id = kv.first;
    for (auto& observer : observer_list_)
      observer.OnVersionStoppedRunning(version_id);
  }
  running_service_workers_.clear();
}

ServiceWorkerVersion* ServiceWorkerContextWrapper::GetLiveServiceWorker(
    int64_t service_worker_version_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // We might be shutting down.
  if (!context()) {
    return nullptr;
  }

  return context()->GetLiveVersion(service_worker_version_id);
}

}  // namespace content
