// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_context_wrapper.h"

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/loader/navigation_url_loader_impl.h"
#include "content/browser/service_worker/embedded_worker_status.h"
#include "content/browser/service_worker/service_worker_context_watcher.h"
#include "content/browser/service_worker/service_worker_process_manager.h"
#include "content/browser/service_worker/service_worker_quota_client.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/browser/storage_partition_impl.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/service_worker_context_observer.h"
#include "content/public/browser/storage_usage_info.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/base/url_util.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/quota/special_storage_policy.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"
#include "third_party/blink/public/common/service_worker/service_worker_utils.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"

namespace content {

namespace {

void WorkerStarted(ServiceWorkerContextWrapper::StatusCallback callback,
                   blink::ServiceWorkerStatusCode status) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  base::PostTask(FROM_HERE, {BrowserThread::UI},
                 base::BindOnce(std::move(callback), status));
}

void StartActiveWorkerOnCoreThread(
    ServiceWorkerContextWrapper::StatusCallback callback,
    blink::ServiceWorkerStatusCode status,
    scoped_refptr<ServiceWorkerRegistration> registration) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  if (status == blink::ServiceWorkerStatusCode::kOk) {
    // Pass the reference of |registration| to WorkerStarted callback to prevent
    // it from being deleted while starting the worker. If the refcount of
    // |registration| is 1, it will be deleted after WorkerStarted is called.
    registration->active_version()->StartWorker(
        ServiceWorkerMetrics::EventType::UNKNOWN,
        base::BindOnce(WorkerStarted, std::move(callback)));
    return;
  }
  base::PostTask(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(std::move(callback),
                     blink::ServiceWorkerStatusCode::kErrorNotFound));
}

void SkipWaitingWorkerOnCoreThread(
    blink::ServiceWorkerStatusCode status,
    scoped_refptr<ServiceWorkerRegistration> registration) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  if (status != blink::ServiceWorkerStatusCode::kOk ||
      !registration->waiting_version())
    return;

  registration->waiting_version()->set_skip_waiting(true);
  registration->ActivateWaitingVersionWhenReady();
}

void DidStartWorker(scoped_refptr<ServiceWorkerVersion> version,
                    ServiceWorkerContext::StartWorkerCallback info_callback,
                    base::OnceClosure error_callback,
                    scoped_refptr<base::TaskRunner> callback_runner,
                    blink::ServiceWorkerStatusCode start_worker_status) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  if (start_worker_status != blink::ServiceWorkerStatusCode::kOk) {
    callback_runner->PostTask(FROM_HERE, std::move(error_callback));
    return;
  }
  EmbeddedWorkerInstance* instance = version->embedded_worker();
  callback_runner->PostTask(
      FROM_HERE, base::BindOnce(std::move(info_callback), version->version_id(),
                                instance->process_id(), instance->thread_id()));
}

void FoundRegistrationForStartWorker(
    ServiceWorkerContext::StartWorkerCallback info_callback,
    base::OnceClosure failure_callback,
    scoped_refptr<base::TaskRunner> callback_runner,
    blink::ServiceWorkerStatusCode service_worker_status,
    scoped_refptr<ServiceWorkerRegistration> registration) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  if (service_worker_status != blink::ServiceWorkerStatusCode::kOk) {
    callback_runner->PostTask(FROM_HERE, std::move(failure_callback));
    return;
  }

  ServiceWorkerVersion* version_ptr = registration->active_version()
                                          ? registration->active_version()
                                          : registration->installing_version();
  // Since FindRegistrationForScope returned
  // blink::ServiceWorkerStatusCode::kOk, there must be either: -
  // an active version, which optionally might have activated from a waiting
  //   version (as DidFindRegistrationForFindImpl will activate any waiting
  //   version).
  // - or an installing version.
  DCHECK(version_ptr);

  // Note: There might be a remote possibility that |registration|'s |version|
  // might change between here and DidStartWorker, so bind |version| to
  // RunAfterStartWorker.
  scoped_refptr<ServiceWorkerVersion> version =
      base::WrapRefCounted(version_ptr);
  version->RunAfterStartWorker(
      ServiceWorkerMetrics::EventType::EXTERNAL_REQUEST,
      base::BindOnce(&DidStartWorker, version, std::move(info_callback),
                     std::move(failure_callback), std::move(callback_runner)));
}

void FinishRegistrationOnCoreThread(
    ServiceWorkerContext::ResultCallback callback,
    blink::ServiceWorkerStatusCode status,
    const std::string& status_message,
    int64_t registration_id) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  base::PostTask(FROM_HERE, {BrowserThread::UI},
                 base::BindOnce(std::move(callback),
                                status == blink::ServiceWorkerStatusCode::kOk));
}

void FinishUnregistrationOnCoreThread(
    ServiceWorkerContext::ResultCallback callback,
    blink::ServiceWorkerStatusCode status) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  base::PostTask(FROM_HERE, {BrowserThread::UI},
                 base::BindOnce(std::move(callback),
                                status == blink::ServiceWorkerStatusCode::kOk));
}

void MessageFinishedSending(ServiceWorkerContext::ResultCallback callback,
                            scoped_refptr<base::TaskRunner> callback_runner,
                            blink::ServiceWorkerStatusCode status) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  callback_runner->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback),
                                status == blink::ServiceWorkerStatusCode::kOk));
}

void RunOnceClosure(scoped_refptr<ServiceWorkerContextWrapper> ref_holder,
                    base::OnceClosure task) {
  std::move(task).Run();
}

}  // namespace

// static
void ServiceWorkerContextWrapper::RunOrPostTaskOnCoreThread(
    const base::Location& location,
    base::OnceClosure task) {
  RunOrPostTaskOnThread(location, GetCoreThreadId(), std::move(task));
}

// static
bool ServiceWorkerContext::ScopeMatches(const GURL& scope, const GURL& url) {
  return ServiceWorkerUtils::ScopeMatches(scope, url);
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

ServiceWorkerContextWrapper::ServiceWorkerContextWrapper(
    BrowserContext* browser_context)
    : core_observer_list_(
          base::MakeRefCounted<ServiceWorkerContextObserverList>()),
      process_manager_(
          std::make_unique<ServiceWorkerProcessManager>(browser_context)),
      core_thread_task_runner_(
          base::CreateSequencedTaskRunner({GetCoreThreadId()})) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Add this object as an observer of the wrapped |context_core_|. This lets us
  // forward observer methods to observers outside of content.
  core_observer_list_->AddObserver(this);

  watcher_ = base::MakeRefCounted<ServiceWorkerContextWatcher>(
      this,
      base::BindRepeating(&ServiceWorkerContextWrapper::OnRegistrationUpdated,
                          base::Unretained(this)),
      base::DoNothing(), base::DoNothing());
}

void ServiceWorkerContextWrapper::Init(
    const base::FilePath& user_data_directory,
    storage::QuotaManagerProxy* quota_manager_proxy,
    storage::SpecialStoragePolicy* special_storage_policy,
    ChromeBlobStorageContext* blob_context,
    URLLoaderFactoryGetter* loader_factory_getter) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(storage_partition_);

  is_incognito_ = user_data_directory.empty();
  // The database task runner is BLOCK_SHUTDOWN in order to support
  // ClearSessionOnlyOrigins() (called due to the "clear on browser exit"
  // content setting).
  // TODO(falken): Only block shutdown for that particular task, when someday
  // task runners support mixing task shutdown behaviors.
  scoped_refptr<base::SequencedTaskRunner> database_task_runner =
      base::CreateSequencedTaskRunner(
          {base::ThreadPool(), base::MayBlock(),
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN});
  std::unique_ptr<blink::URLLoaderFactoryBundleInfo>
      non_network_loader_factory_bundle_info_for_update_check;
  if (blink::ServiceWorkerUtils::IsImportedScriptUpdateCheckEnabled()) {
    non_network_loader_factory_bundle_info_for_update_check =
        CreateNonNetworkURLLoaderFactoryBundleInfoForUpdateCheck(
            storage_partition_->browser_context());
  }

  RunOrPostTaskOnCoreThread(
      FROM_HERE,
      base::BindOnce(
          &ServiceWorkerContextWrapper::InitOnCoreThread, this,
          user_data_directory, std::move(database_task_runner),
          base::RetainedRef(quota_manager_proxy),
          base::RetainedRef(special_storage_policy),
          base::RetainedRef(blob_context),
          base::RetainedRef(loader_factory_getter),
          std::move(non_network_loader_factory_bundle_info_for_update_check)));

  // The watcher also runs or posts a core thread task which must run after
  // InitOnCoreThread(), so start it after posting that task above.
  if (watcher_)
    watcher_->Start();
}

void ServiceWorkerContextWrapper::Shutdown() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  storage_partition_ = nullptr;
  process_manager_->Shutdown();
  if (watcher_) {
    watcher_->Stop();
    watcher_ = nullptr;
  }

  // Use explicit feature check here instead of RunOrPostTaskOnThread(), since
  // the feature may be disabled but in unit tests we are considered both on the
  // UI and IO thread here, and not posting a task causes a race with callers
  // setting the |resource_context_|.
  if (ServiceWorkerContext::IsServiceWorkerOnUIEnabled()) {
    ShutdownOnCoreThread();
  } else {
    base::PostTask(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(&ServiceWorkerContextWrapper::ShutdownOnCoreThread,
                       this));
  }
}

void ServiceWorkerContextWrapper::InitializeResourceContext(
    ResourceContext* resource_context) {
  DCHECK(!ServiceWorkerContext::IsServiceWorkerOnUIEnabled());
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  resource_context_ = resource_context;
}

void ServiceWorkerContextWrapper::DeleteAndStartOver() {
  DCHECK_CURRENTLY_ON(GetCoreThreadId());
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
  return process_manager()->browser_context();
}

ResourceContext* ServiceWorkerContextWrapper::resource_context() {
  DCHECK(!ServiceWorkerContext::IsServiceWorkerOnUIEnabled());
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return resource_context_;
}

// static
bool ServiceWorkerContext::IsServiceWorkerOnUIEnabled() {
  return base::FeatureList::IsEnabled(features::kServiceWorkerOnUI);
}

// static
BrowserThread::ID ServiceWorkerContext::GetCoreThreadId() {
  return IsServiceWorkerOnUIEnabled() ? BrowserThread::UI : BrowserThread::IO;
}

void ServiceWorkerContextWrapper::OnRegistrationCompleted(
    int64_t registration_id,
    const GURL& scope) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  for (auto& observer : observer_list_)
    observer.OnRegistrationCompleted(scope);
}

void ServiceWorkerContextWrapper::OnRegistrationStored(int64_t registration_id,
                                                       const GURL& scope) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  for (auto& observer : observer_list_)
    observer.OnRegistrationStored(registration_id, scope);
}

void ServiceWorkerContextWrapper::OnReportConsoleMessage(
    int64_t version_id,
    const ConsoleMessage& message) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  for (auto& observer : observer_list_)
    observer.OnReportConsoleMessage(version_id, message);
}

void ServiceWorkerContextWrapper::OnNoControllees(int64_t version_id,
                                                  const GURL& scope) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  for (auto& observer : observer_list_)
    observer.OnNoControllees(version_id, scope);
}

void ServiceWorkerContextWrapper::OnStarted(int64_t version_id,
                                            const GURL& scope,
                                            int process_id,
                                            const GURL& script_url) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto insertion_result = running_service_workers_.insert(std::make_pair(
      version_id, ServiceWorkerRunningInfo(script_url, scope, process_id)));
  DCHECK(insertion_result.second);

  const auto& running_info = insertion_result.first->second;
  for (auto& observer : observer_list_) {
    observer.OnVersionStartedRunning(this, version_id, running_info);
  }
}

void ServiceWorkerContextWrapper::OnStopped(int64_t version_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto it = running_service_workers_.find(version_id);
  if (it != running_service_workers_.end()) {
    running_service_workers_.erase(it);
    for (auto& observer : observer_list_)
      observer.OnVersionStoppedRunning(this, version_id);
  }
}

void ServiceWorkerContextWrapper::OnDeleteAndStartOver() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  for (const auto& kv : running_service_workers_) {
    int64_t version_id = kv.first;
    for (auto& observer : observer_list_)
      observer.OnVersionStoppedRunning(this, version_id);
  }
  running_service_workers_.clear();
}

void ServiceWorkerContextWrapper::OnVersionStateChanged(
    int64_t version_id,
    const GURL& scope,
    ServiceWorkerVersion::Status status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

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

void ServiceWorkerContextWrapper::RegisterServiceWorker(
    const GURL& script_url,
    const blink::mojom::ServiceWorkerRegistrationOptions& options,
    ResultCallback callback) {
  if (!BrowserThread::CurrentlyOn(GetCoreThreadId())) {
    base::PostTask(
        FROM_HERE, {GetCoreThreadId()},
        base::BindOnce(&ServiceWorkerContextWrapper::RegisterServiceWorker,
                       this, script_url, options, std::move(callback)));
    return;
  }
  if (!context_core_) {
    base::PostTask(FROM_HERE, {BrowserThread::UI},
                   base::BindOnce(std::move(callback), false));
    return;
  }
  blink::mojom::ServiceWorkerRegistrationOptions options_to_pass(
      net::SimplifyUrlForRequest(options.scope), options.type,
      options.update_via_cache);
  // TODO(bashi): Pass a valid outside fetch client settings object. Perhaps
  // changing this method to take a settings object.
  context()->RegisterServiceWorker(
      net::SimplifyUrlForRequest(script_url), options_to_pass,
      blink::mojom::FetchClientSettingsObject::New(
          network::mojom::ReferrerPolicy::kDefault,
          /*outgoing_referrer=*/script_url,
          blink::mojom::InsecureRequestsPolicy::kDoNotUpgrade),
      base::BindOnce(&FinishRegistrationOnCoreThread, std::move(callback)));
}

void ServiceWorkerContextWrapper::UnregisterServiceWorker(
    const GURL& scope,
    ResultCallback callback) {
  if (!BrowserThread::CurrentlyOn(GetCoreThreadId())) {
    base::PostTask(
        FROM_HERE, {GetCoreThreadId()},
        base::BindOnce(&ServiceWorkerContextWrapper::UnregisterServiceWorker,
                       this, scope, std::move(callback)));
    return;
  }
  if (!context_core_) {
    base::PostTask(FROM_HERE, {BrowserThread::UI},
                   base::BindOnce(std::move(callback), false));
    return;
  }

  context()->UnregisterServiceWorker(
      net::SimplifyUrlForRequest(scope),
      base::BindOnce(&FinishUnregistrationOnCoreThread, std::move(callback)));
}

ServiceWorkerExternalRequestResult
ServiceWorkerContextWrapper::StartingExternalRequest(
    int64_t service_worker_version_id,
    const std::string& request_uuid) {
  DCHECK_CURRENTLY_ON(GetCoreThreadId());
  if (!context())
    return ServiceWorkerExternalRequestResult::kNullContext;
  ServiceWorkerVersion* version =
      context()->GetLiveVersion(service_worker_version_id);
  if (!version)
    return ServiceWorkerExternalRequestResult::kWorkerNotFound;
  return version->StartExternalRequest(request_uuid);
}

ServiceWorkerExternalRequestResult
ServiceWorkerContextWrapper::FinishedExternalRequest(
    int64_t service_worker_version_id,
    const std::string& request_uuid) {
  DCHECK_CURRENTLY_ON(GetCoreThreadId());
  if (!context())
    return ServiceWorkerExternalRequestResult::kNullContext;
  ServiceWorkerVersion* version =
      context()->GetLiveVersion(service_worker_version_id);
  if (!version)
    return ServiceWorkerExternalRequestResult::kWorkerNotFound;
  return version->FinishExternalRequest(request_uuid);
}

void ServiceWorkerContextWrapper::CountExternalRequestsForTest(
    const GURL& origin,
    CountExternalRequestsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::PostTask(
      FROM_HERE, {GetCoreThreadId()},
      base::BindOnce(&ServiceWorkerContextWrapper::CountExternalRequests, this,
                     origin, std::move(callback)));
}

void ServiceWorkerContextWrapper::GetAllOriginsInfo(
    GetUsageInfoCallback callback) {
  RunOrPostTaskOnCoreThread(
      FROM_HERE,
      base::BindOnce(
          &ServiceWorkerContextWrapper::GetAllOriginsInfoOnCoreThread, this,
          std::move(callback), base::ThreadTaskRunnerHandle::Get()));
}

void ServiceWorkerContextWrapper::GetAllOriginsInfoOnCoreThread(
    GetUsageInfoCallback callback,
    scoped_refptr<base::TaskRunner> callback_runner) {
  if (!context_core_) {
    callback_runner->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), std::vector<StorageUsageInfo>()));
    return;
  }
  context()->storage()->GetAllRegistrationsInfos(base::BindOnce(
      &ServiceWorkerContextWrapper::DidGetAllRegistrationsForGetAllOrigins,
      this, std::move(callback), std::move(callback_runner)));
}

void ServiceWorkerContextWrapper::DeleteForOrigin(const GURL& origin,
                                                  ResultCallback callback) {
  RunOrPostTaskOnCoreThread(
      FROM_HERE,
      base::BindOnce(&ServiceWorkerContextWrapper::DeleteForOriginOnCoreThread,
                     this, origin, std::move(callback),
                     base::ThreadTaskRunnerHandle::Get()));
}

void ServiceWorkerContextWrapper::DeleteForOriginOnCoreThread(
    const GURL& origin,
    ResultCallback callback,
    scoped_refptr<base::TaskRunner> callback_runner) {
  DCHECK_CURRENTLY_ON(GetCoreThreadId());
  if (!context_core_) {
    callback_runner->PostTask(FROM_HERE,
                              base::BindOnce(std::move(callback), false));
    return;
  }
  context()->DeleteForOrigin(
      origin.GetOrigin(),
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

void ServiceWorkerContextWrapper::PerformStorageCleanup(
    base::OnceClosure callback) {
  RunOrPostTaskOnCoreThread(
      FROM_HERE,
      base::BindOnce(
          &ServiceWorkerContextWrapper::PerformStorageCleanupOnCoreThread, this,
          std::move(callback), base::ThreadTaskRunnerHandle::Get()));
}

void ServiceWorkerContextWrapper::PerformStorageCleanupOnCoreThread(
    base::OnceClosure callback,
    scoped_refptr<base::TaskRunner> callback_runner) {
  if (!context_core_) {
    callback_runner->PostTask(FROM_HERE, std::move(callback));
    return;
  }
  context()->PerformStorageCleanup(base::BindOnce(
      [](base::OnceClosure callback,
         scoped_refptr<base::TaskRunner> callback_runner) {
        callback_runner->PostTask(FROM_HERE, std::move(callback));
      },
      std::move(callback), std::move(callback_runner)));
}

void ServiceWorkerContextWrapper::CheckHasServiceWorker(
    const GURL& url,
    CheckHasServiceWorkerCallback callback) {
  if (!BrowserThread::CurrentlyOn(GetCoreThreadId())) {
    base::PostTask(
        FROM_HERE, {GetCoreThreadId()},
        base::BindOnce(&ServiceWorkerContextWrapper::CheckHasServiceWorker,
                       this, url, std::move(callback)));
    return;
  }
  if (!context_core_) {
    base::PostTask(FROM_HERE, {BrowserThread::UI},
                   base::BindOnce(std::move(callback),
                                  ServiceWorkerCapability::NO_SERVICE_WORKER));
    return;
  }
  context()->CheckHasServiceWorker(
      net::SimplifyUrlForRequest(url),
      base::BindOnce(&ServiceWorkerContextWrapper::DidCheckHasServiceWorker,
                     this, std::move(callback)));
}

void ServiceWorkerContextWrapper::ClearAllServiceWorkersForTest(
    base::OnceClosure callback) {
  if (!BrowserThread::CurrentlyOn(GetCoreThreadId())) {
    base::PostTask(
        FROM_HERE, {GetCoreThreadId()},
        base::BindOnce(
            &ServiceWorkerContextWrapper::ClearAllServiceWorkersForTest, this,
            std::move(callback)));
    return;
  }

  if (!context_core_) {
    base::PostTask(FROM_HERE, {BrowserThread::UI}, std::move(callback));
    return;
  }
  context_core_->ClearAllServiceWorkersForTest(std::move(callback));
}

void ServiceWorkerContextWrapper::StartWorkerForScope(
    const GURL& scope,
    StartWorkerCallback info_callback,
    base::OnceClosure failure_callback) {
  RunOrPostTaskOnCoreThread(
      FROM_HERE,
      base::BindOnce(
          &ServiceWorkerContextWrapper::FindRegistrationForScopeOnCoreThread,
          this, scope, /*include_installing_version=*/true,
          base::BindOnce(&FoundRegistrationForStartWorker,
                         std::move(info_callback), std::move(failure_callback),
                         base::ThreadTaskRunnerHandle::Get()),
          core_thread_task_runner_));
}

void ServiceWorkerContextWrapper::StartServiceWorkerAndDispatchMessage(
    const GURL& scope,
    blink::TransferableMessage message,
    ResultCallback result_callback) {
  RunOrPostTaskOnCoreThread(
      FROM_HERE,
      base::BindOnce(&ServiceWorkerContextWrapper::
                         StartServiceWorkerAndDispatchMessageOnCoreThread,
                     this, scope, std::move(message),
                     std::move(result_callback),
                     base::ThreadTaskRunnerHandle::Get()));
}

void ServiceWorkerContextWrapper::
    StartServiceWorkerAndDispatchMessageOnCoreThread(
        const GURL& scope,
        blink::TransferableMessage message,
        ResultCallback result_callback,
        scoped_refptr<base::TaskRunner> callback_runner) {
  DCHECK_CURRENTLY_ON(GetCoreThreadId());

  if (!context_core_) {
    callback_runner->PostTask(
        FROM_HERE, base::BindOnce(std::move(result_callback), false));
    return;
  }

  FindRegistrationForScopeOnCoreThread(
      net::SimplifyUrlForRequest(scope), false /* include_installing_version */,
      base::BindOnce(
          &ServiceWorkerContextWrapper::DidFindRegistrationForMessageDispatch,
          this, std::move(message), scope, std::move(result_callback),
          std::move(callback_runner)),
      core_thread_task_runner_);
}

void ServiceWorkerContextWrapper::DidFindRegistrationForMessageDispatch(
    blink::TransferableMessage message,
    const GURL& source_origin,
    ResultCallback result_callback,
    scoped_refptr<base::TaskRunner> callback_runner,
    blink::ServiceWorkerStatusCode service_worker_status,
    scoped_refptr<ServiceWorkerRegistration> registration) {
  DCHECK_CURRENTLY_ON(GetCoreThreadId());
  if (service_worker_status != blink::ServiceWorkerStatusCode::kOk) {
    LOG(WARNING) << "No registration available, status: "
                 << static_cast<int>(service_worker_status);
    callback_runner->PostTask(
        FROM_HERE, base::BindOnce(std::move(result_callback), false));
    return;
  }
  registration->active_version()->StartWorker(
      ServiceWorkerMetrics::EventType::MESSAGE,
      base::BindOnce(
          &ServiceWorkerContextWrapper::DidStartServiceWorkerForMessageDispatch,
          this, std::move(message), source_origin, registration,
          std::move(result_callback), std::move(callback_runner)));
}

void ServiceWorkerContextWrapper::DidStartServiceWorkerForMessageDispatch(
    blink::TransferableMessage message,
    const GURL& source_origin,
    scoped_refptr<ServiceWorkerRegistration> registration,
    ResultCallback result_callback,
    scoped_refptr<base::TaskRunner> callback_runner,
    blink::ServiceWorkerStatusCode status) {
  DCHECK_CURRENTLY_ON(GetCoreThreadId());
  if (status != blink::ServiceWorkerStatusCode::kOk) {
    callback_runner->PostTask(
        FROM_HERE, base::BindOnce(std::move(result_callback), false));
    return;
  }

  scoped_refptr<ServiceWorkerVersion> version = registration->active_version();

  blink::mojom::ExtendableMessageEventPtr event =
      blink::mojom::ExtendableMessageEvent::New();
  event->message = std::move(message);
  event->source_origin = url::Origin::Create(source_origin);
  event->source_info_for_service_worker =
      version->provider_host()
          ->GetOrCreateServiceWorkerObjectHost(version)
          ->CreateCompleteObjectInfoToSend();

  int request_id = version->StartRequest(
      ServiceWorkerMetrics::EventType::MESSAGE,
      base::BindOnce(&MessageFinishedSending, std::move(result_callback),
                     std::move(callback_runner)));
  version->endpoint()->DispatchExtendableMessageEvent(
      std::move(event), version->CreateSimpleEventCallback(request_id));
}

void ServiceWorkerContextWrapper::StartServiceWorkerForNavigationHint(
    const GURL& document_url,
    StartServiceWorkerForNavigationHintCallback callback) {
  TRACE_EVENT1("ServiceWorker", "StartServiceWorkerForNavigationHint",
               "document_url", document_url.spec());
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  RunOrPostTaskOnCoreThread(
      FROM_HERE,
      base::BindOnce(
          &ServiceWorkerContextWrapper::
              StartServiceWorkerForNavigationHintOnCoreThread,
          this, document_url,
          base::BindOnce(&ServiceWorkerContextWrapper::
                             RecordStartServiceWorkerForNavigationHintResult,
                         this, std::move(callback))));
}

void ServiceWorkerContextWrapper::StopAllServiceWorkersForOrigin(
    const GURL& origin) {
  if (!BrowserThread::CurrentlyOn(GetCoreThreadId())) {
    base::PostTask(
        FROM_HERE, {GetCoreThreadId()},
        base::BindOnce(
            &ServiceWorkerContextWrapper::StopAllServiceWorkersForOrigin, this,
            origin));
    return;
  }
  if (!context_core_.get()) {
    return;
  }
  std::vector<ServiceWorkerVersionInfo> live_versions = GetAllLiveVersionInfo();
  for (const ServiceWorkerVersionInfo& info : live_versions) {
    ServiceWorkerVersion* version = GetLiveVersion(info.version_id);
    if (version && version->scope().GetOrigin() == origin)
      version->StopWorker(base::DoNothing());
  }
}

void ServiceWorkerContextWrapper::StopAllServiceWorkers(
    base::OnceClosure callback) {
  RunOrPostTaskOnCoreThread(
      FROM_HERE,
      base::BindOnce(
          &ServiceWorkerContextWrapper::StopAllServiceWorkersOnCoreThread, this,
          std::move(callback), base::ThreadTaskRunnerHandle::Get()));
}

const base::flat_map<int64_t, ServiceWorkerRunningInfo>&
ServiceWorkerContextWrapper::GetRunningServiceWorkerInfos() {
  return running_service_workers_;
}

ServiceWorkerRegistration* ServiceWorkerContextWrapper::GetLiveRegistration(
    int64_t registration_id) {
  DCHECK_CURRENTLY_ON(GetCoreThreadId());
  if (!context_core_)
    return nullptr;
  return context_core_->GetLiveRegistration(registration_id);
}

ServiceWorkerVersion* ServiceWorkerContextWrapper::GetLiveVersion(
    int64_t version_id) {
  DCHECK_CURRENTLY_ON(GetCoreThreadId());
  if (!context_core_)
    return nullptr;
  return context_core_->GetLiveVersion(version_id);
}

std::vector<ServiceWorkerRegistrationInfo>
ServiceWorkerContextWrapper::GetAllLiveRegistrationInfo() {
  DCHECK_CURRENTLY_ON(GetCoreThreadId());
  if (!context_core_)
    return std::vector<ServiceWorkerRegistrationInfo>();
  return context_core_->GetAllLiveRegistrationInfo();
}

std::vector<ServiceWorkerVersionInfo>
ServiceWorkerContextWrapper::GetAllLiveVersionInfo() {
  DCHECK_CURRENTLY_ON(GetCoreThreadId());
  if (!context_core_)
    return std::vector<ServiceWorkerVersionInfo>();
  return context_core_->GetAllLiveVersionInfo();
}

void ServiceWorkerContextWrapper::HasMainFrameProviderHost(
    const GURL& origin,
    BoolCallback callback) const {
  RunOrPostTaskOnCoreThread(
      FROM_HERE,
      base::BindOnce(
          &ServiceWorkerContextWrapper::HasMainFrameProviderHostOnCoreThread,
          this, origin, std::move(callback),
          base::ThreadTaskRunnerHandle::Get()));
}

void ServiceWorkerContextWrapper::HasMainFrameProviderHostOnCoreThread(
    const GURL& origin,
    BoolCallback callback,
    scoped_refptr<base::TaskRunner> callback_runner) const {
  DCHECK_CURRENTLY_ON(GetCoreThreadId());
  if (!context_core_) {
    callback_runner->PostTask(FROM_HERE,
                              base::BindOnce(std::move(callback), false));
    return;
  }
  context_core_->HasMainFrameProviderHost(
      origin,
      base::BindOnce(
          [](BoolCallback callback,
             scoped_refptr<base::TaskRunner> callback_runner, bool result) {
            callback_runner->PostTask(
                FROM_HERE, base::BindOnce(std::move(callback), result));
          },
          std::move(callback), std::move(callback_runner)));
}

std::unique_ptr<std::vector<GlobalFrameRoutingId>>
ServiceWorkerContextWrapper::GetProviderHostIds(const GURL& origin) const {
  DCHECK_CURRENTLY_ON(GetCoreThreadId());

  std::unique_ptr<std::vector<GlobalFrameRoutingId>> provider_host_ids(
      new std::vector<GlobalFrameRoutingId>());
  if (!context_core_)
    return provider_host_ids;

  for (std::unique_ptr<ServiceWorkerContextCore::ProviderHostIterator> it =
           context_core_->GetClientProviderHostIterator(
               origin, false /* include_reserved_clients */);
       !it->IsAtEnd(); it->Advance()) {
    ServiceWorkerProviderHost* provider_host = it->GetProviderHost();
    provider_host_ids->push_back(GlobalFrameRoutingId(
        provider_host->process_id(), provider_host->frame_id()));
  }

  return provider_host_ids;
}

void ServiceWorkerContextWrapper::FindReadyRegistrationForClientUrl(
    const GURL& client_url,
    FindRegistrationCallback callback) {
  RunOrPostTaskOnCoreThread(
      FROM_HERE,
      base::BindOnce(&ServiceWorkerContextWrapper::
                         FindReadyRegistrationForClientUrlOnCoreThread,
                     this, client_url, std::move(callback),
                     base::ThreadTaskRunnerHandle::Get()));
}

void ServiceWorkerContextWrapper::FindReadyRegistrationForClientUrlOnCoreThread(
    const GURL& client_url,
    FindRegistrationCallback callback,
    scoped_refptr<base::TaskRunner> callback_runner) {
  DCHECK_CURRENTLY_ON(GetCoreThreadId());
  if (!context_core_) {
    callback_runner->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       blink::ServiceWorkerStatusCode::kErrorAbort, nullptr));
    return;
  }
  context_core_->storage()->FindRegistrationForClientUrl(
      net::SimplifyUrlForRequest(client_url),
      base::BindOnce(
          &ServiceWorkerContextWrapper::DidFindRegistrationForFindReady, this,
          std::move(callback), std::move(callback_runner)));
}

void ServiceWorkerContextWrapper::FindReadyRegistrationForScope(
    const GURL& scope,
    FindRegistrationCallback callback) {
  const bool include_installing_version = false;
  RunOrPostTaskOnCoreThread(
      FROM_HERE,
      base::BindOnce(
          &ServiceWorkerContextWrapper::FindRegistrationForScopeOnCoreThread,
          this, scope, include_installing_version, std::move(callback),
          base::ThreadTaskRunnerHandle::Get()));
}

void ServiceWorkerContextWrapper::FindRegistrationForScope(
    const GURL& scope,
    FindRegistrationCallback callback) {
  const bool include_installing_version = true;
  RunOrPostTaskOnCoreThread(
      FROM_HERE,
      base::BindOnce(
          &ServiceWorkerContextWrapper::FindRegistrationForScopeOnCoreThread,
          this, scope, include_installing_version, std::move(callback),
          base::ThreadTaskRunnerHandle::Get()));
}

void ServiceWorkerContextWrapper::FindReadyRegistrationForId(
    int64_t registration_id,
    const GURL& origin,
    FindRegistrationCallback callback) {
  RunOrPostTaskOnCoreThread(
      FROM_HERE,
      base::BindOnce(
          &ServiceWorkerContextWrapper::FindReadyRegistrationForIdOnCoreThread,
          this, registration_id, origin, std::move(callback),
          base::ThreadTaskRunnerHandle::Get()));
}

void ServiceWorkerContextWrapper::FindReadyRegistrationForIdOnCoreThread(
    int64_t registration_id,
    const GURL& origin,
    FindRegistrationCallback callback,
    scoped_refptr<base::TaskRunner> callback_runner) {
  DCHECK_CURRENTLY_ON(GetCoreThreadId());
  if (!context_core_) {
    callback_runner->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       blink::ServiceWorkerStatusCode::kErrorAbort, nullptr));
    return;
  }
  context_core_->storage()->FindRegistrationForId(
      registration_id, origin.GetOrigin(),
      base::BindOnce(
          &ServiceWorkerContextWrapper::DidFindRegistrationForFindReady, this,
          std::move(callback), std::move(callback_runner)));
}

void ServiceWorkerContextWrapper::FindReadyRegistrationForIdOnly(
    int64_t registration_id,
    FindRegistrationCallback callback) {
  RunOrPostTaskOnCoreThread(
      FROM_HERE, base::BindOnce(&ServiceWorkerContextWrapper::
                                    FindReadyRegistrationForIdOnlyOnCoreThread,
                                this, registration_id, std::move(callback),
                                base::ThreadTaskRunnerHandle::Get()));
}

void ServiceWorkerContextWrapper::FindReadyRegistrationForIdOnlyOnCoreThread(
    int64_t registration_id,
    FindRegistrationCallback callback,
    scoped_refptr<base::TaskRunner> callback_runner) {
  DCHECK_CURRENTLY_ON(GetCoreThreadId());
  if (!context_core_) {
    callback_runner->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       blink::ServiceWorkerStatusCode::kErrorAbort, nullptr));
    return;
  }
  context_core_->storage()->FindRegistrationForIdOnly(
      registration_id,
      base::BindOnce(
          &ServiceWorkerContextWrapper::DidFindRegistrationForFindReady, this,
          std::move(callback), std::move(callback_runner)));
}

void ServiceWorkerContextWrapper::GetAllRegistrations(
    GetRegistrationsInfosCallback callback) {
  RunOrPostTaskOnCoreThread(
      FROM_HERE,
      base::BindOnce(
          &ServiceWorkerContextWrapper::GetAllRegistrationsOnCoreThread, this,
          base::BindOnce(
              [](GetRegistrationsInfosCallback callback,
                 scoped_refptr<base::TaskRunner> callback_runner,
                 blink::ServiceWorkerStatusCode status,
                 const std::vector<ServiceWorkerRegistrationInfo>&
                     registrations) {
                callback_runner->PostTask(
                    FROM_HERE,
                    base::BindOnce(std::move(callback), status, registrations));
              },
              std::move(callback), base::ThreadTaskRunnerHandle::Get())));
}

void ServiceWorkerContextWrapper::GetAllRegistrationsOnCoreThread(
    GetRegistrationsInfosCallback callback) {
  DCHECK_CURRENTLY_ON(GetCoreThreadId());
  if (!context_core_) {
    std::move(callback).Run(blink::ServiceWorkerStatusCode::kErrorAbort,
                            std::vector<ServiceWorkerRegistrationInfo>());
    return;
  }
  context_core_->storage()->GetAllRegistrationsInfos(std::move(callback));
}

void ServiceWorkerContextWrapper::GetRegistrationsForOrigin(
    const url::Origin& origin,
    GetRegistrationsCallback callback) {
  DCHECK_CURRENTLY_ON(GetCoreThreadId());
  if (!context_core_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(
            std::move(callback), blink::ServiceWorkerStatusCode::kErrorAbort,
            std::vector<scoped_refptr<ServiceWorkerRegistration>>()));
    return;
  }
  context_core_->storage()->GetRegistrationsForOrigin(origin.GetURL(),
                                                      std::move(callback));
}

void ServiceWorkerContextWrapper::GetRegistrationUserData(
    int64_t registration_id,
    const std::vector<std::string>& keys,
    GetUserDataCallback callback) {
  RunOrPostTaskOnCoreThread(
      FROM_HERE,
      base::BindOnce(
          &ServiceWorkerContextWrapper::GetRegistrationUserDataOnCoreThread,
          this, registration_id, keys,
          base::BindOnce(
              [](GetUserDataCallback callback,
                 scoped_refptr<base::TaskRunner> callback_runner,
                 const std::vector<std::string>& data,
                 blink::ServiceWorkerStatusCode status) {
                callback_runner->PostTask(
                    FROM_HERE,
                    base::BindOnce(std::move(callback), data, status));
              },
              std::move(callback), base::ThreadTaskRunnerHandle::Get())));
}

void ServiceWorkerContextWrapper::GetRegistrationUserDataOnCoreThread(
    int64_t registration_id,
    const std::vector<std::string>& keys,
    GetUserDataCallback callback) {
  DCHECK_CURRENTLY_ON(GetCoreThreadId());
  if (!context_core_) {
    std::move(callback).Run(std::vector<std::string>(),
                            blink::ServiceWorkerStatusCode::kErrorAbort);
    return;
  }
  context_core_->storage()->GetUserData(registration_id, keys,
                                        std::move(callback));
}

void ServiceWorkerContextWrapper::GetRegistrationUserDataByKeyPrefix(
    int64_t registration_id,
    const std::string& key_prefix,
    GetUserDataCallback callback) {
  RunOrPostTaskOnCoreThread(
      FROM_HERE,
      base::BindOnce(
          &ServiceWorkerContextWrapper::
              GetRegistrationUserDataByKeyPrefixOnCoreThread,
          this, registration_id, key_prefix,
          base::BindOnce(
              [](GetUserDataCallback callback,
                 scoped_refptr<base::TaskRunner> callback_runner,
                 const std::vector<std::string>& data,
                 blink::ServiceWorkerStatusCode status) {
                callback_runner->PostTask(
                    FROM_HERE,
                    base::BindOnce(std::move(callback), data, status));
              },
              std::move(callback), base::ThreadTaskRunnerHandle::Get())));
}

void ServiceWorkerContextWrapper::
    GetRegistrationUserDataByKeyPrefixOnCoreThread(
        int64_t registration_id,
        const std::string& key_prefix,
        GetUserDataCallback callback) {
  DCHECK_CURRENTLY_ON(GetCoreThreadId());
  if (!context_core_) {
    std::move(callback).Run(std::vector<std::string>(),
                            blink::ServiceWorkerStatusCode::kErrorAbort);
    return;
  }
  context_core_->storage()->GetUserDataByKeyPrefix(registration_id, key_prefix,
                                                   std::move(callback));
}

void ServiceWorkerContextWrapper::GetRegistrationUserKeysAndDataByKeyPrefix(
    int64_t registration_id,
    const std::string& key_prefix,
    GetUserKeysAndDataCallback callback) {
  RunOrPostTaskOnCoreThread(
      FROM_HERE,
      base::BindOnce(
          &ServiceWorkerContextWrapper::
              GetRegistrationUserKeysAndDataByKeyPrefixOnCoreThread,
          this, registration_id, key_prefix,
          base::BindOnce(
              [](GetUserKeysAndDataCallback callback,
                 scoped_refptr<base::TaskRunner> callback_runner,
                 const base::flat_map<std::string, std::string>& data_map,
                 blink::ServiceWorkerStatusCode status) {
                callback_runner->PostTask(
                    FROM_HERE,
                    base::BindOnce(std::move(callback), data_map, status));
              },
              std::move(callback), base::ThreadTaskRunnerHandle::Get())));
}

void ServiceWorkerContextWrapper::
    GetRegistrationUserKeysAndDataByKeyPrefixOnCoreThread(
        int64_t registration_id,
        const std::string& key_prefix,
        GetUserKeysAndDataCallback callback) {
  DCHECK_CURRENTLY_ON(GetCoreThreadId());
  if (!context_core_) {
    std::move(callback).Run(base::flat_map<std::string, std::string>(),
                            blink::ServiceWorkerStatusCode::kErrorAbort);
    return;
  }
  context_core_->storage()->GetUserKeysAndDataByKeyPrefix(
      registration_id, key_prefix, std::move(callback));
}

void ServiceWorkerContextWrapper::StoreRegistrationUserData(
    int64_t registration_id,
    const GURL& origin,
    const std::vector<std::pair<std::string, std::string>>& key_value_pairs,
    StatusCallback callback) {
  RunOrPostTaskOnCoreThread(
      FROM_HERE,
      base::BindOnce(
          &ServiceWorkerContextWrapper::StoreRegistrationUserDataOnCoreThread,
          this, registration_id, origin, key_value_pairs,
          base::BindOnce(
              [](StatusCallback callback,
                 scoped_refptr<base::TaskRunner> callback_runner,
                 blink::ServiceWorkerStatusCode status) {
                callback_runner->PostTask(
                    FROM_HERE, base::BindOnce(std::move(callback), status));
              },
              std::move(callback), base::ThreadTaskRunnerHandle::Get())));
}

void ServiceWorkerContextWrapper::StoreRegistrationUserDataOnCoreThread(
    int64_t registration_id,
    const GURL& origin,
    const std::vector<std::pair<std::string, std::string>>& key_value_pairs,
    StatusCallback callback) {
  DCHECK_CURRENTLY_ON(GetCoreThreadId());
  if (!context_core_) {
    std::move(callback).Run(blink::ServiceWorkerStatusCode::kErrorAbort);
    return;
  }
  context_core_->storage()->StoreUserData(registration_id, origin.GetOrigin(),
                                          key_value_pairs, std::move(callback));
}

void ServiceWorkerContextWrapper::ClearRegistrationUserData(
    int64_t registration_id,
    const std::vector<std::string>& keys,
    StatusCallback callback) {
  RunOrPostTaskOnCoreThread(
      FROM_HERE,
      base::BindOnce(
          &ServiceWorkerContextWrapper::ClearRegistrationUserDataOnCoreThread,
          this, registration_id, keys,
          base::BindOnce(
              [](StatusCallback callback,
                 scoped_refptr<base::TaskRunner> callback_runner,
                 blink::ServiceWorkerStatusCode status) {
                callback_runner->PostTask(
                    FROM_HERE, base::BindOnce(std::move(callback), status));
              },
              std::move(callback), base::ThreadTaskRunnerHandle::Get())));
}

void ServiceWorkerContextWrapper::ClearRegistrationUserDataOnCoreThread(
    int64_t registration_id,
    const std::vector<std::string>& keys,
    StatusCallback callback) {
  DCHECK_CURRENTLY_ON(GetCoreThreadId());
  if (!context_core_) {
    std::move(callback).Run(blink::ServiceWorkerStatusCode::kErrorAbort);
    return;
  }
  context_core_->storage()->ClearUserData(registration_id, keys,
                                          std::move(callback));
}

void ServiceWorkerContextWrapper::ClearRegistrationUserDataByKeyPrefixes(
    int64_t registration_id,
    const std::vector<std::string>& key_prefixes,
    StatusCallback callback) {
  RunOrPostTaskOnCoreThread(
      FROM_HERE,
      base::BindOnce(
          &ServiceWorkerContextWrapper::
              ClearRegistrationUserDataByKeyPrefixesOnCoreThread,
          this, registration_id, key_prefixes,
          base::BindOnce(
              [](StatusCallback callback,
                 scoped_refptr<base::TaskRunner> callback_runner,
                 blink::ServiceWorkerStatusCode status) {
                callback_runner->PostTask(
                    FROM_HERE, base::BindOnce(std::move(callback), status));
              },
              std::move(callback), base::ThreadTaskRunnerHandle::Get())));
}

void ServiceWorkerContextWrapper::
    ClearRegistrationUserDataByKeyPrefixesOnCoreThread(
        int64_t registration_id,
        const std::vector<std::string>& key_prefixes,
        StatusCallback callback) {
  DCHECK_CURRENTLY_ON(GetCoreThreadId());
  if (!context_core_) {
    std::move(callback).Run(blink::ServiceWorkerStatusCode::kErrorAbort);
    return;
  }
  context_core_->storage()->ClearUserDataByKeyPrefixes(
      registration_id, key_prefixes, std::move(callback));
}

void ServiceWorkerContextWrapper::GetUserDataForAllRegistrations(
    const std::string& key,
    GetUserDataForAllRegistrationsCallback callback) {
  RunOrPostTaskOnCoreThread(
      FROM_HERE,
      base::BindOnce(
          &ServiceWorkerContextWrapper::
              GetUserDataForAllRegistrationsOnCoreThread,
          this, key,
          base::BindOnce(
              [](GetUserDataForAllRegistrationsCallback callback,
                 scoped_refptr<base::TaskRunner> callback_runner,
                 const std::vector<std::pair<int64_t, std::string>>& user_data,
                 blink::ServiceWorkerStatusCode status) {
                callback_runner->PostTask(
                    FROM_HERE,
                    base::BindOnce(std::move(callback), user_data, status));
              },
              std::move(callback), base::ThreadTaskRunnerHandle::Get())));
}

void ServiceWorkerContextWrapper::GetUserDataForAllRegistrationsOnCoreThread(
    const std::string& key,
    GetUserDataForAllRegistrationsCallback callback) {
  DCHECK_CURRENTLY_ON(GetCoreThreadId());
  if (!context_core_) {
    std::move(callback).Run(std::vector<std::pair<int64_t, std::string>>(),
                            blink::ServiceWorkerStatusCode::kErrorAbort);
    return;
  }
  context_core_->storage()->GetUserDataForAllRegistrations(key,
                                                           std::move(callback));
}

void ServiceWorkerContextWrapper::GetUserDataForAllRegistrationsByKeyPrefix(
    const std::string& key_prefix,
    GetUserDataForAllRegistrationsCallback callback) {
  RunOrPostTaskOnCoreThread(
      FROM_HERE,
      base::BindOnce(
          &ServiceWorkerContextWrapper::
              GetUserDataForAllRegistrationsByKeyPrefixOnCoreThread,
          this, key_prefix,
          base::BindOnce(
              [](GetUserDataForAllRegistrationsCallback callback,
                 scoped_refptr<base::TaskRunner> callback_runner,
                 const std::vector<std::pair<int64_t, std::string>>& user_data,
                 blink::ServiceWorkerStatusCode status) {
                callback_runner->PostTask(
                    FROM_HERE,
                    base::BindOnce(std::move(callback), user_data, status));
              },
              std::move(callback), base::ThreadTaskRunnerHandle::Get())));
}

void ServiceWorkerContextWrapper::
    GetUserDataForAllRegistrationsByKeyPrefixOnCoreThread(
        const std::string& key_prefix,
        GetUserDataForAllRegistrationsCallback callback) {
  DCHECK_CURRENTLY_ON(GetCoreThreadId());
  if (!context_core_) {
    std::move(callback).Run(std::vector<std::pair<int64_t, std::string>>(),
                            blink::ServiceWorkerStatusCode::kErrorAbort);
    return;
  }
  context_core_->storage()->GetUserDataForAllRegistrationsByKeyPrefix(
      key_prefix, std::move(callback));
}

void ServiceWorkerContextWrapper::ClearUserDataForAllRegistrationsByKeyPrefix(
    const std::string& key_prefix,
    StatusCallback callback) {
  RunOrPostTaskOnCoreThread(
      FROM_HERE,
      base::BindOnce(
          &ServiceWorkerContextWrapper::
              ClearUserDataForAllRegistrationsByKeyPrefixOnCoreThread,
          this, key_prefix,
          base::BindOnce(
              [](StatusCallback callback,
                 scoped_refptr<base::TaskRunner> callback_runner,
                 blink::ServiceWorkerStatusCode status) {
                callback_runner->PostTask(
                    FROM_HERE, base::BindOnce(std::move(callback), status));
              },
              std::move(callback), base::ThreadTaskRunnerHandle::Get())));
}

void ServiceWorkerContextWrapper::
    ClearUserDataForAllRegistrationsByKeyPrefixOnCoreThread(
        const std::string& key_prefix,
        StatusCallback callback) {
  DCHECK_CURRENTLY_ON(GetCoreThreadId());
  if (!context_core_) {
    std::move(callback).Run(blink::ServiceWorkerStatusCode::kErrorAbort);
    return;
  }
  context_core_->storage()->ClearUserDataForAllRegistrationsByKeyPrefix(
      key_prefix, std::move(callback));
}

void ServiceWorkerContextWrapper::StartServiceWorker(const GURL& scope,
                                                     StatusCallback callback) {
  if (!BrowserThread::CurrentlyOn(GetCoreThreadId())) {
    base::PostTask(
        FROM_HERE, {GetCoreThreadId()},
        base::BindOnce(&ServiceWorkerContextWrapper::StartServiceWorker, this,
                       scope, std::move(callback)));
    return;
  }
  if (!context_core_) {
    base::PostTask(FROM_HERE, {BrowserThread::UI},
                   base::BindOnce(std::move(callback),
                                  blink::ServiceWorkerStatusCode::kErrorAbort));
    return;
  }
  context_core_->storage()->FindRegistrationForScope(
      net::SimplifyUrlForRequest(scope),
      base::BindOnce(&StartActiveWorkerOnCoreThread, std::move(callback)));
}

void ServiceWorkerContextWrapper::SkipWaitingWorker(const GURL& scope) {
  if (!BrowserThread::CurrentlyOn(GetCoreThreadId())) {
    base::PostTask(
        FROM_HERE, {GetCoreThreadId()},
        base::BindOnce(&ServiceWorkerContextWrapper::SkipWaitingWorker, this,
                       scope));
    return;
  }
  if (!context_core_)
    return;
  context_core_->storage()->FindRegistrationForScope(
      net::SimplifyUrlForRequest(scope),
      base::BindOnce(&SkipWaitingWorkerOnCoreThread));
}

void ServiceWorkerContextWrapper::UpdateRegistration(const GURL& scope) {
  if (!BrowserThread::CurrentlyOn(GetCoreThreadId())) {
    base::PostTask(
        FROM_HERE, {GetCoreThreadId()},
        base::BindOnce(&ServiceWorkerContextWrapper::UpdateRegistration, this,
                       scope));
    return;
  }
  if (!context_core_)
    return;
  context_core_->storage()->FindRegistrationForScope(
      net::SimplifyUrlForRequest(scope),
      base::BindOnce(&ServiceWorkerContextWrapper::DidFindRegistrationForUpdate,
                     this));
}

void ServiceWorkerContextWrapper::SetForceUpdateOnPageLoad(
    bool force_update_on_page_load) {
  if (!BrowserThread::CurrentlyOn(GetCoreThreadId())) {
    base::PostTask(
        FROM_HERE, {GetCoreThreadId()},
        base::BindOnce(&ServiceWorkerContextWrapper::SetForceUpdateOnPageLoad,
                       this, force_update_on_page_load));
    return;
  }
  if (!context_core_)
    return;
  context_core_->set_force_update_on_page_load(force_update_on_page_load);
}

void ServiceWorkerContextWrapper::AddObserver(
    ServiceWorkerContextCoreObserver* observer) {
  core_observer_list_->AddObserver(observer);
}

void ServiceWorkerContextWrapper::RemoveObserver(
    ServiceWorkerContextCoreObserver* observer) {
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
  DCHECK(!resource_context_);
}

void ServiceWorkerContextWrapper::InitOnCoreThread(
    const base::FilePath& user_data_directory,
    scoped_refptr<base::SequencedTaskRunner> database_task_runner,
    storage::QuotaManagerProxy* quota_manager_proxy,
    storage::SpecialStoragePolicy* special_storage_policy,
    ChromeBlobStorageContext* blob_context,
    URLLoaderFactoryGetter* loader_factory_getter,
    std::unique_ptr<blink::URLLoaderFactoryBundleInfo>
        non_network_loader_factory_bundle_info_for_update_check) {
  DCHECK_CURRENTLY_ON(GetCoreThreadId());
  DCHECK(!context_core_);

  if (quota_manager_proxy) {
    quota_manager_proxy->RegisterClient(new ServiceWorkerQuotaClient(this));
  }

  context_core_ = std::make_unique<ServiceWorkerContextCore>(
      user_data_directory, std::move(database_task_runner), quota_manager_proxy,
      special_storage_policy, loader_factory_getter,
      std::move(non_network_loader_factory_bundle_info_for_update_check),
      core_observer_list_.get(), this);
}

void ServiceWorkerContextWrapper::FindRegistrationForScopeOnCoreThread(
    const GURL& scope,
    bool include_installing_version,
    FindRegistrationCallback callback,
    scoped_refptr<base::TaskRunner> callback_runner) {
  DCHECK_CURRENTLY_ON(GetCoreThreadId());
  if (!context_core_) {
    callback_runner->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       blink::ServiceWorkerStatusCode::kErrorAbort, nullptr));
    return;
  }
  context_core_->storage()->FindRegistrationForScope(
      net::SimplifyUrlForRequest(scope),
      base::BindOnce(
          &ServiceWorkerContextWrapper::DidFindRegistrationForFindImpl, this,
          include_installing_version, std::move(callback),
          std::move(callback_runner)));
}

void ServiceWorkerContextWrapper::ShutdownOnCoreThread() {
  DCHECK_CURRENTLY_ON(GetCoreThreadId());
  if (!ServiceWorkerContext::IsServiceWorkerOnUIEnabled())
    resource_context_ = nullptr;
  context_core_.reset();
}

void ServiceWorkerContextWrapper::DidFindRegistrationForFindImpl(
    bool include_installing_version,
    FindRegistrationCallback callback,
    scoped_refptr<base::TaskRunner> callback_runner,
    blink::ServiceWorkerStatusCode status,
    scoped_refptr<ServiceWorkerRegistration> registration) {
  DCHECK_CURRENTLY_ON(GetCoreThreadId());
  if (status != blink::ServiceWorkerStatusCode::kOk) {
    callback_runner->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), status, nullptr));
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
          this, std::move(callback), std::move(callback_runner),
          std::move(registration)));
      return;
    }
    DCHECK_EQ(ServiceWorkerVersion::ACTIVATED, active_version->status());
    callback_runner->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), blink::ServiceWorkerStatusCode::kOk,
                       std::move(registration)));
    return;
  }

  if (include_installing_version && registration->installing_version()) {
    callback_runner->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), blink::ServiceWorkerStatusCode::kOk,
                       std::move(registration)));
    return;
  }

  callback_runner->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback),
                     blink::ServiceWorkerStatusCode::kErrorNotFound, nullptr));
}

void ServiceWorkerContextWrapper::DidFindRegistrationForFindReady(
    FindRegistrationCallback callback,
    scoped_refptr<base::TaskRunner> callback_runner,
    blink::ServiceWorkerStatusCode status,
    scoped_refptr<ServiceWorkerRegistration> registration) {
  DCHECK_CURRENTLY_ON(GetCoreThreadId());
  DidFindRegistrationForFindImpl(
      false /* include_installing_version */, std::move(callback),
      std::move(callback_runner), status, std::move(registration));
}

void ServiceWorkerContextWrapper::OnStatusChangedForFindReadyRegistration(
    FindRegistrationCallback callback,
    scoped_refptr<base::TaskRunner> callback_runner,
    scoped_refptr<ServiceWorkerRegistration> registration) {
  DCHECK_CURRENTLY_ON(GetCoreThreadId());
  scoped_refptr<ServiceWorkerVersion> active_version =
      registration->active_version();
  if (!active_version ||
      active_version->status() != ServiceWorkerVersion::ACTIVATED) {
    callback_runner->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       blink::ServiceWorkerStatusCode::kErrorNotFound,
                       nullptr));
    return;
  }
  callback_runner->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), blink::ServiceWorkerStatusCode::kOk,
                     registration));
}

void ServiceWorkerContextWrapper::DidDeleteAndStartOver(
    blink::ServiceWorkerStatusCode status) {
  DCHECK_CURRENTLY_ON(GetCoreThreadId());
  if (status != blink::ServiceWorkerStatusCode::kOk) {
    context_core_.reset();
    return;
  }
  context_core_.reset(new ServiceWorkerContextCore(context_core_.get(), this));
  DVLOG(1) << "Restarted ServiceWorkerContextCore successfully.";
  context_core_->OnStorageWiped();
}

void ServiceWorkerContextWrapper::DidGetAllRegistrationsForGetAllOrigins(
    GetUsageInfoCallback callback,
    scoped_refptr<base::TaskRunner> callback_runner,
    blink::ServiceWorkerStatusCode status,
    const std::vector<ServiceWorkerRegistrationInfo>& registrations) {
  DCHECK_CURRENTLY_ON(GetCoreThreadId());
  std::vector<StorageUsageInfo> usage_infos;

  std::map<GURL, StorageUsageInfo> origins;
  for (const auto& registration_info : registrations) {
    GURL origin = registration_info.scope.GetOrigin();

    auto it = origins.find(origin);
    if (it == origins.end()) {
      origins[origin] = StorageUsageInfo(
          url::Origin::Create(origin),
          registration_info.stored_version_size_bytes, base::Time());
    } else {
      it->second.total_size_bytes +=
          registration_info.stored_version_size_bytes;
    }
  }

  for (const auto& origin_info_pair : origins) {
    usage_infos.push_back(origin_info_pair.second);
  }
  callback_runner->PostTask(FROM_HERE,
                            base::BindOnce(std::move(callback), usage_infos));
}

void ServiceWorkerContextWrapper::DidCheckHasServiceWorker(
    CheckHasServiceWorkerCallback callback,
    ServiceWorkerCapability capability) {
  DCHECK_CURRENTLY_ON(GetCoreThreadId());

  base::PostTask(FROM_HERE, {BrowserThread::UI},
                 base::BindOnce(std::move(callback), capability));
}

void ServiceWorkerContextWrapper::DidFindRegistrationForUpdate(
    blink::ServiceWorkerStatusCode status,
    scoped_refptr<ServiceWorkerRegistration> registration) {
  DCHECK_CURRENTLY_ON(GetCoreThreadId());

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
  context_core_->UpdateServiceWorker(registration.get(),
                                     true /* force_bypass_cache */);
}

void ServiceWorkerContextWrapper::CountExternalRequests(
    const GURL& origin,
    CountExternalRequestsCallback callback) {
  DCHECK_CURRENTLY_ON(GetCoreThreadId());

  std::vector<ServiceWorkerVersionInfo> live_version_info =
      GetAllLiveVersionInfo();
  size_t pending_external_request_count = 0;
  for (const ServiceWorkerVersionInfo& info : live_version_info) {
    ServiceWorkerVersion* version = GetLiveVersion(info.version_id);
    if (version && version->scope().GetOrigin() == origin) {
      pending_external_request_count =
          version->GetExternalRequestCountForTest();
      break;
    }
  }

  base::PostTask(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(std::move(callback), pending_external_request_count));
}

void ServiceWorkerContextWrapper::
    StartServiceWorkerForNavigationHintOnCoreThread(
        const GURL& document_url,
        StartServiceWorkerForNavigationHintCallback callback) {
  TRACE_EVENT1("ServiceWorker",
               "StartServiceWorkerForNavigationHintOnCoreThread",
               "document_url", document_url.spec());
  DCHECK_CURRENTLY_ON(GetCoreThreadId());
  if (!context_core_) {
    std::move(callback).Run(StartServiceWorkerForNavigationHintResult::FAILED);
    return;
  }
  context_core_->storage()->FindRegistrationForClientUrl(
      net::SimplifyUrlForRequest(document_url),
      base::BindOnce(
          &ServiceWorkerContextWrapper::DidFindRegistrationForNavigationHint,
          this, std::move(callback)));
}

void ServiceWorkerContextWrapper::DidFindRegistrationForNavigationHint(
    StartServiceWorkerForNavigationHintCallback callback,
    blink::ServiceWorkerStatusCode status,
    scoped_refptr<ServiceWorkerRegistration> registration) {
  TRACE_EVENT1("ServiceWorker", "DidFindRegistrationForNavigationHint",
               "status", blink::ServiceWorkerStatusToString(status));
  DCHECK_CURRENTLY_ON(GetCoreThreadId());
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
      EmbeddedWorkerStatus::RUNNING) {
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
  DCHECK_CURRENTLY_ON(GetCoreThreadId());
  std::move(callback).Run(
      code == blink::ServiceWorkerStatusCode::kOk
          ? StartServiceWorkerForNavigationHintResult::STARTED
          : StartServiceWorkerForNavigationHintResult::FAILED);
}

void ServiceWorkerContextWrapper::
    RecordStartServiceWorkerForNavigationHintResult(
        StartServiceWorkerForNavigationHintCallback callback,
        StartServiceWorkerForNavigationHintResult result) {
  DCHECK_CURRENTLY_ON(GetCoreThreadId());
  ServiceWorkerMetrics::RecordStartServiceWorkerForNavigationHintResult(result);
  base::PostTask(FROM_HERE, {BrowserThread::UI},
                 base::BindOnce(std::move(callback), result));
}

void ServiceWorkerContextWrapper::StopAllServiceWorkersOnCoreThread(
    base::OnceClosure callback,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner_for_callback) {
  DCHECK_CURRENTLY_ON(GetCoreThreadId());
  if (!context_core_.get()) {
    task_runner_for_callback->PostTask(FROM_HERE, std::move(callback));
    return;
  }
  std::vector<ServiceWorkerVersionInfo> live_versions = GetAllLiveVersionInfo();
  base::RepeatingClosure barrier = base::BarrierClosure(
      live_versions.size(),
      base::BindOnce(
          base::IgnoreResult(&base::SingleThreadTaskRunner::PostTask),
          std::move(task_runner_for_callback), FROM_HERE, std::move(callback)));
  for (const ServiceWorkerVersionInfo& info : live_versions) {
    ServiceWorkerVersion* version = GetLiveVersion(info.version_id);
    DCHECK(version);
    version->StopWorker(base::BindOnce(barrier));
  }
}

ServiceWorkerContextCore* ServiceWorkerContextWrapper::context() {
  DCHECK_CURRENTLY_ON(GetCoreThreadId());
  return context_core_.get();
}

std::unique_ptr<blink::URLLoaderFactoryBundleInfo> ServiceWorkerContextWrapper::
    CreateNonNetworkURLLoaderFactoryBundleInfoForUpdateCheck(
        BrowserContext* browser_context) {
  DCHECK(blink::ServiceWorkerUtils::IsImportedScriptUpdateCheckEnabled());
  ContentBrowserClient::NonNetworkURLLoaderFactoryMap non_network_factories;
  GetContentClient()
      ->browser()
      ->RegisterNonNetworkServiceWorkerUpdateURLLoaderFactories(
          browser_context, &non_network_factories);

  auto factory_bundle = std::make_unique<blink::URLLoaderFactoryBundleInfo>();
  for (auto& pair : non_network_factories) {
    const std::string& scheme = pair.first;
    std::unique_ptr<network::mojom::URLLoaderFactory> factory =
        std::move(pair.second);

    mojo::PendingRemote<network::mojom::URLLoaderFactory> factory_remote;
    mojo::MakeSelfOwnedReceiver(
        std::move(factory), factory_remote.InitWithNewPipeAndPassReceiver());
    factory_bundle->pending_scheme_specific_factories().emplace(
        scheme, std::move(factory_remote));
  }

  return factory_bundle;
}

void ServiceWorkerContextWrapper::GetLoaderFactoryForUpdateCheck(
    const GURL& scope,
    base::OnceCallback<void(scoped_refptr<network::SharedURLLoaderFactory>)>
        callback) {
  DCHECK_CURRENTLY_ON(GetCoreThreadId());

  RunOrPostTaskOnThread(
      FROM_HERE, BrowserThread::UI,
      base::BindOnce(
          &ServiceWorkerContextWrapper::SetUpLoaderFactoryForUpdateCheckOnUI,
          this, scope, std::move(callback)));
}

void ServiceWorkerContextWrapper::SetUpLoaderFactoryForUpdateCheckOnUI(
    const GURL& scope,
    base::OnceCallback<void(scoped_refptr<network::SharedURLLoaderFactory>)>
        callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!storage_partition()) {
    RunOrPostTaskOnThread(
        FROM_HERE, GetCoreThreadId(),
        base::BindOnce(
            &ServiceWorkerContextWrapper::DidSetUpLoaderFactoryForUpdateCheck,
            this, mojo::NullRemote(), mojo::NullReceiver(),
            /* bypass_redirect_checks=*/false, std::move(callback)));
    return;
  }

  mojo::PendingRemote<network::mojom::URLLoaderFactory> remote;
  mojo::PendingReceiver<network::mojom::URLLoaderFactory> pending_receiver =
      remote.InitWithNewPipeAndPassReceiver();
  mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>
      header_client;
  bool bypass_redirect_checks = false;
  GetContentClient()->browser()->WillCreateURLLoaderFactory(
      storage_partition_->browser_context(), /*frame=*/nullptr,
      ChildProcessHost::kInvalidUniqueID,
      ContentBrowserClient::URLLoaderFactoryType::kServiceWorkerScript,
      url::Origin::Create(scope), &pending_receiver, &header_client,
      &bypass_redirect_checks);
  if (header_client) {
    NavigationURLLoaderImpl::CreateURLLoaderFactoryWithHeaderClient(
        std::move(header_client), std::move(pending_receiver),
        storage_partition());
  }

  RunOrPostTaskOnThread(
      FROM_HERE, GetCoreThreadId(),
      base::BindOnce(
          &ServiceWorkerContextWrapper::DidSetUpLoaderFactoryForUpdateCheck,
          this, std::move(remote), std::move(pending_receiver),
          bypass_redirect_checks, std::move(callback)));
}

void ServiceWorkerContextWrapper::DidSetUpLoaderFactoryForUpdateCheck(
    mojo::PendingRemote<network::mojom::URLLoaderFactory> remote,
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> pending_receiver,
    bool bypass_redirect_checks,
    base::OnceCallback<void(scoped_refptr<network::SharedURLLoaderFactory>)>
        callback) {
  DCHECK_CURRENTLY_ON(GetCoreThreadId());

  // Return nullptr if preparation on the UI thread failed.
  if (!remote) {
    std::move(callback).Run(nullptr);
    return;
  }

  // Set up a Mojo connection to the network loader factory if it's not been
  // created yet.
  if (pending_receiver) {
    if (IsServiceWorkerOnUIEnabled()) {
      DCHECK(storage_partition());
      scoped_refptr<network::SharedURLLoaderFactory> network_factory =
          storage_partition_->GetURLLoaderFactoryForBrowserProcess();
      network_factory->Clone(std::move(pending_receiver));
    } else {
      context()->loader_factory_getter()->CloneNetworkFactory(
          std::move(pending_receiver));
    }
  }

  // Clone context()->loader_factory_bundle_for_update_check() and set up the
  // default factory.
  std::unique_ptr<network::SharedURLLoaderFactoryInfo>
      loader_factory_bundle_info =
          context()->loader_factory_bundle_for_update_check()->Clone();
  static_cast<blink::URLLoaderFactoryBundleInfo*>(
      loader_factory_bundle_info.get())
      ->pending_default_factory() = std::move(remote);
  static_cast<blink::URLLoaderFactoryBundleInfo*>(
      loader_factory_bundle_info.get())
      ->set_bypass_redirect_checks(bypass_redirect_checks);
  scoped_refptr<network::SharedURLLoaderFactory> loader_factory =
      network::SharedURLLoaderFactory::Create(
          std::move(loader_factory_bundle_info));
  std::move(callback).Run(std::move(loader_factory));
}

bool ServiceWorkerContextWrapper::HasRegistrationForOrigin(
    const GURL& origin) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return !registrations_initialized_ ||
         registrations_for_origin_.find(origin) !=
             registrations_for_origin_.end();
}

void ServiceWorkerContextWrapper::WaitForRegistrationsInitializedForTest() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (registrations_initialized_)
    return;
  base::RunLoop loop;
  on_registrations_initialized_ = loop.QuitClosure();
  loop.Run();
}

void ServiceWorkerContextWrapper::OnRegistrationUpdated(
    const std::vector<ServiceWorkerRegistrationInfo>& registrations) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // The first call will initialize stored registrations.
  registrations_initialized_ = true;

  for (const auto& registration : registrations) {
    GURL origin = registration.scope.GetOrigin();
    int64_t registration_id = registration.registration_id;
    if (registration.delete_flag == ServiceWorkerRegistrationInfo::IS_DELETED) {
      auto& registration_ids = registrations_for_origin_[origin];
      registration_ids.erase(registration_id);
      if (registration_ids.empty())
        registrations_for_origin_.erase(origin);
    } else {
      registrations_for_origin_[origin].insert(registration_id);
    }
  }

  if (on_registrations_initialized_)
    std::move(on_registrations_initialized_).Run();
}

}  // namespace content
