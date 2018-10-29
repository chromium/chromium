// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_context_core.h"

#include <limits>
#include <set>
#include <utility>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/service_worker/embedded_worker_registry.h"
#include "content/browser/service_worker/embedded_worker_status.h"
#include "content/browser/service_worker/service_worker_consts.h"
#include "content/browser/service_worker/service_worker_context_core_observer.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_info.h"
#include "content/browser/service_worker/service_worker_job_coordinator.h"
#include "content/browser/service_worker/service_worker_process_manager.h"
#include "content/browser/service_worker/service_worker_provider_host.h"
#include "content/browser/service_worker/service_worker_register_job.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_storage.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/browser/url_loader_factory_getter.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/child_process_host.h"
#include "ipc/ipc_message.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_provider_type.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"
#include "url/gurl.h"

namespace content {
namespace {

void CheckFetchHandlerOfInstalledServiceWorker(
    ServiceWorkerContext::CheckHasServiceWorkerCallback callback,
    scoped_refptr<ServiceWorkerRegistration> registration) {
  // Waiting Service Worker is a newer version, prefer that if available.
  ServiceWorkerVersion* preferred_version =
      registration->waiting_version() ? registration->waiting_version()
                                      : registration->active_version();

  DCHECK(preferred_version);

  ServiceWorkerVersion::FetchHandlerExistence existence =
      preferred_version->fetch_handler_existence();

  DCHECK_NE(existence, ServiceWorkerVersion::FetchHandlerExistence::UNKNOWN);

  std::move(callback).Run(
      existence == ServiceWorkerVersion::FetchHandlerExistence::EXISTS
          ? ServiceWorkerCapability::SERVICE_WORKER_WITH_FETCH_HANDLER
          : ServiceWorkerCapability::SERVICE_WORKER_NO_FETCH_HANDLER);
}

void SuccessCollectorCallback(base::OnceClosure done_closure,
                              bool* overall_success,
                              blink::ServiceWorkerStatusCode status) {
  if (status != blink::ServiceWorkerStatusCode::kOk) {
    *overall_success = false;
  }
  std::move(done_closure).Run();
}

void SuccessReportingCallback(
    const bool* success,
    base::OnceCallback<void(blink::ServiceWorkerStatusCode)> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  bool result = *success;
  std::move(callback).Run(result
                              ? blink::ServiceWorkerStatusCode::kOk
                              : blink::ServiceWorkerStatusCode::kErrorFailed);
}

bool IsSameOriginClientProviderHost(const GURL& origin,
                                    bool allow_reserved_client,
                                    ServiceWorkerProviderHost* host) {
  return host->IsProviderForClient() &&
         host->document_url().GetOrigin() == origin &&
         (allow_reserved_client || host->is_execution_ready());
}

bool IsSameOriginWindowProviderHost(const GURL& origin,
                                    ServiceWorkerProviderHost* host) {
  return host->provider_type() ==
             blink::mojom::ServiceWorkerProviderType::kForWindow &&
         host->document_url().GetOrigin() == origin &&
         host->is_execution_ready();
}

// Returns true if any of the frames specified by |frames| is a top-level frame.
// |frames| is a vector of (render process id, frame id) pairs.
bool FrameListContainsMainFrameOnUI(
    std::unique_ptr<std::vector<std::pair<int, int>>> frames) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  for (const auto& frame : *frames) {
    RenderFrameHostImpl* render_frame_host =
        RenderFrameHostImpl::FromID(frame.first, frame.second);
    if (!render_frame_host)
      continue;
    if (!render_frame_host->GetParent())
      return true;
  }
  return false;
}

class ClearAllServiceWorkersHelper
    : public base::RefCounted<ClearAllServiceWorkersHelper> {
 public:
  explicit ClearAllServiceWorkersHelper(base::OnceClosure callback)
      : callback_(std::move(callback)) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
  }

  void OnResult(blink::ServiceWorkerStatusCode) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    // We do nothing in this method. We use this class to wait for all callbacks
    // to be called using the refcount.
  }

  void DidGetAllRegistrations(
      const base::WeakPtr<ServiceWorkerContextCore>& context,
      blink::ServiceWorkerStatusCode status,
      const std::vector<ServiceWorkerRegistrationInfo>& registrations) {
    if (!context || status != blink::ServiceWorkerStatusCode::kOk)
      return;
    // Make a copy of live versions map because StopWorker() removes the version
    // from it when we were starting up and don't have a process yet.
    const std::map<int64_t, ServiceWorkerVersion*> live_versions_copy =
        context->GetLiveVersions();
    for (const auto& version_itr : live_versions_copy) {
      ServiceWorkerVersion* version(version_itr.second);
      if (version->running_status() == EmbeddedWorkerStatus::STARTING ||
          version->running_status() == EmbeddedWorkerStatus::RUNNING) {
        version->StopWorker(base::DoNothing());
      }
    }
    for (const auto& registration_info : registrations) {
      context->UnregisterServiceWorker(
          registration_info.scope,
          base::BindOnce(&ClearAllServiceWorkersHelper::OnResult, this));
    }
  }

 private:
  friend class base::RefCounted<ClearAllServiceWorkersHelper>;
  ~ClearAllServiceWorkersHelper() {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI},
                             std::move(callback_));
  }

  base::OnceClosure callback_;
  DISALLOW_COPY_AND_ASSIGN(ClearAllServiceWorkersHelper);
};

class RegistrationDeletionListener
    : public ServiceWorkerRegistration::Listener {
 public:
  // Wait until a |registration| is deleted and call |callback|.
  static void WaitForDeletion(
      scoped_refptr<ServiceWorkerRegistration> registration,
      base::OnceClosure callback) {
    DCHECK(!registration->is_deleted());
    registration->AddListener(
        new RegistrationDeletionListener(registration, std::move(callback)));
  }

  void OnRegistrationDeleted(ServiceWorkerRegistration* registration) override {
    registration->RemoveListener(this);
    std::move(callback_).Run();
    delete this;
  }

 private:
  RegistrationDeletionListener(
      scoped_refptr<ServiceWorkerRegistration> registration,
      base::OnceClosure callback)
      : registration_(std::move(registration)),
        callback_(std::move(callback)) {}
  virtual ~RegistrationDeletionListener() = default;

  scoped_refptr<ServiceWorkerRegistration> registration_;
  base::OnceClosure callback_;
};

}  // namespace

const base::FilePath::CharType
    ServiceWorkerContextCore::kServiceWorkerDirectory[] =
        FILE_PATH_LITERAL("Service Worker");

ServiceWorkerContextCore::ProviderHostIterator::~ProviderHostIterator() {}

ServiceWorkerProviderHost*
ServiceWorkerContextCore::ProviderHostIterator::GetProviderHost() {
  DCHECK(!IsAtEnd());
  return provider_host_iterator_->GetCurrentValue();
}

void ServiceWorkerContextCore::ProviderHostIterator::Advance() {
  DCHECK(!IsAtEnd());
  DCHECK(!provider_host_iterator_->IsAtEnd());
  DCHECK(!process_iterator_->IsAtEnd());

  // Advance the inner iterator. If an element is reached, we're done.
  provider_host_iterator_->Advance();
  if (ForwardUntilMatchingProviderHost())
    return;

  // Advance the outer iterator until an element is reached, or end is hit.
  while (true) {
    process_iterator_->Advance();
    if (process_iterator_->IsAtEnd())
      return;
    ProviderMap* provider_map = process_iterator_->GetCurrentValue();
    provider_host_iterator_.reset(new ProviderMap::iterator(provider_map));
    if (ForwardUntilMatchingProviderHost())
      return;
  }
}

bool ServiceWorkerContextCore::ProviderHostIterator::IsAtEnd() {
  return process_iterator_->IsAtEnd() &&
         (!provider_host_iterator_ || provider_host_iterator_->IsAtEnd());
}

ServiceWorkerContextCore::ProviderHostIterator::ProviderHostIterator(
    ProcessToProviderMap* map,
    ProviderHostPredicate predicate)
    : map_(map), predicate_(std::move(predicate)) {
  DCHECK(map);
  Initialize();
}

void ServiceWorkerContextCore::ProviderHostIterator::Initialize() {
  process_iterator_.reset(new ProcessToProviderMap::iterator(map_));
  // Advance to the first element.
  while (!process_iterator_->IsAtEnd()) {
    ProviderMap* provider_map = process_iterator_->GetCurrentValue();
    provider_host_iterator_.reset(new ProviderMap::iterator(provider_map));
    if (ForwardUntilMatchingProviderHost())
      return;
    process_iterator_->Advance();
  }
}

bool ServiceWorkerContextCore::ProviderHostIterator::
    ForwardUntilMatchingProviderHost() {
  while (!provider_host_iterator_->IsAtEnd()) {
    if (predicate_.is_null() || predicate_.Run(GetProviderHost()))
      return true;
    provider_host_iterator_->Advance();
  }
  return false;
}

ServiceWorkerContextCore::ServiceWorkerContextCore(
    const base::FilePath& user_data_directory,
    scoped_refptr<base::SequencedTaskRunner> database_task_runner,
    storage::QuotaManagerProxy* quota_manager_proxy,
    storage::SpecialStoragePolicy* special_storage_policy,
    URLLoaderFactoryGetter* url_loader_factory_getter,
    base::ObserverListThreadSafe<ServiceWorkerContextCoreObserver>*
        observer_list,
    ServiceWorkerContextWrapper* wrapper)
    : wrapper_(wrapper),
      providers_(std::make_unique<ProcessToProviderMap>()),
      provider_by_uuid_(std::make_unique<ProviderByClientUUIDMap>()),
      loader_factory_getter_(url_loader_factory_getter),
      force_update_on_page_load_(false),
      was_service_worker_registered_(false),
      observer_list_(observer_list),
      weak_factory_(this) {
  DCHECK(observer_list_);
  // These get a WeakPtr from |weak_factory_|, so must be set after
  // |weak_factory_| is initialized.
  storage_ = ServiceWorkerStorage::Create(
      user_data_directory, AsWeakPtr(), std::move(database_task_runner),
      quota_manager_proxy, special_storage_policy);
  embedded_worker_registry_ = EmbeddedWorkerRegistry::Create(AsWeakPtr());
  job_coordinator_ = std::make_unique<ServiceWorkerJobCoordinator>(AsWeakPtr());
}

ServiceWorkerContextCore::ServiceWorkerContextCore(
    ServiceWorkerContextCore* old_context,
    ServiceWorkerContextWrapper* wrapper)
    : wrapper_(wrapper),
      providers_(old_context->providers_.release()),
      provider_by_uuid_(old_context->provider_by_uuid_.release()),
      loader_factory_getter_(old_context->loader_factory_getter()),
      was_service_worker_registered_(
          old_context->was_service_worker_registered_),
      observer_list_(old_context->observer_list_),
      weak_factory_(this) {
  DCHECK(observer_list_);

  // These get a WeakPtr from |weak_factory_|, so must be set after
  // |weak_factory_| is initialized.
  storage_ = ServiceWorkerStorage::Create(AsWeakPtr(), old_context->storage());
  embedded_worker_registry_ = EmbeddedWorkerRegistry::Create(
      AsWeakPtr(),
      old_context->embedded_worker_registry());
  job_coordinator_ = std::make_unique<ServiceWorkerJobCoordinator>(AsWeakPtr());
}

ServiceWorkerContextCore::~ServiceWorkerContextCore() {
  DCHECK(storage_);
  for (const auto& it : live_versions_)
    it.second->RemoveObserver(this);
  weak_factory_.InvalidateWeakPtrs();
}

void ServiceWorkerContextCore::AddProviderHost(
    std::unique_ptr<ServiceWorkerProviderHost> host) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  int process_id = host->process_id();
  int provider_id = host->provider_id();
  // Precreated hosts are stored in the same map regardless of process.
  if (ServiceWorkerUtils::IsBrowserAssignedProviderId(provider_id))
    process_id = ChildProcessHost::kInvalidUniqueID;
  ProviderMap* map = GetProviderMapForProcess(process_id);
  if (!map) {
    providers_->AddWithID(std::make_unique<ProviderMap>(), process_id);
    map = GetProviderMapForProcess(process_id);
  }
  map->AddWithID(std::move(host), provider_id);
}

ServiceWorkerProviderHost* ServiceWorkerContextCore::GetProviderHost(
    int process_id,
    int provider_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  // Precreated hosts are stored in the same map regardless of process.
  if (ServiceWorkerUtils::IsBrowserAssignedProviderId(provider_id))
    process_id = ChildProcessHost::kInvalidUniqueID;
  ProviderMap* map = GetProviderMapForProcess(process_id);
  if (!map)
    return nullptr;
  return map->Lookup(provider_id);
}

void ServiceWorkerContextCore::RemoveProviderHost(
    int process_id, int provider_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  // Precreated hosts are stored in the same map regardless of process.
  if (ServiceWorkerUtils::IsBrowserAssignedProviderId(provider_id))
    process_id = ChildProcessHost::kInvalidUniqueID;
  ProviderMap* map = GetProviderMapForProcess(process_id);
  DCHECK(map);
  map->Remove(provider_id);
}

void ServiceWorkerContextCore::RemoveAllProviderHostsForProcess(
    int process_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (providers_->Lookup(process_id))
    providers_->Remove(process_id);
  // This function is used to prevent <process_id, provider_id> collisions when
  // a render process host is reused with the same process id. Don't bother
  // removing the providers in this process with browser-assigned ids, which
  // live in a different map, since they have unique ids.
}

std::unique_ptr<ServiceWorkerContextCore::ProviderHostIterator>
ServiceWorkerContextCore::GetClientProviderHostIterator(
    const GURL& origin,
    bool include_reserved_clients) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return base::WrapUnique(new ProviderHostIterator(
      providers_.get(), base::BindRepeating(IsSameOriginClientProviderHost,
                                            origin, include_reserved_clients)));
}

void ServiceWorkerContextCore::HasMainFrameProviderHost(
    const GURL& origin,
    BoolCallback callback) const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  ProviderHostIterator provider_host_iterator(
      providers_.get(),
      base::BindRepeating(IsSameOriginWindowProviderHost, origin));

  if (provider_host_iterator.IsAtEnd()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
    return;
  }

  std::unique_ptr<std::vector<std::pair<int, int>>> render_frames(
      new std::vector<std::pair<int, int>>());

  while (!provider_host_iterator.IsAtEnd()) {
    ServiceWorkerProviderHost* provider_host =
        provider_host_iterator.GetProviderHost();
    render_frames->push_back(
        std::make_pair(provider_host->process_id(), provider_host->frame_id()));
    provider_host_iterator.Advance();
  }

  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&FrameListContainsMainFrameOnUI, std::move(render_frames)),
      std::move(callback));
}

void ServiceWorkerContextCore::RegisterProviderHostByClientID(
    const std::string& client_uuid,
    ServiceWorkerProviderHost* provider_host) {
  DCHECK(!ContainsKey(*provider_by_uuid_, client_uuid));
  (*provider_by_uuid_)[client_uuid] = provider_host;
}

void ServiceWorkerContextCore::UnregisterProviderHostByClientID(
    const std::string& client_uuid) {
  DCHECK(ContainsKey(*provider_by_uuid_, client_uuid));
  provider_by_uuid_->erase(client_uuid);
}

ServiceWorkerProviderHost* ServiceWorkerContextCore::GetProviderHostByClientID(
    const std::string& client_uuid) {
  auto found = provider_by_uuid_->find(client_uuid);
  if (found == provider_by_uuid_->end())
    return nullptr;
  return found->second;
}

void ServiceWorkerContextCore::RegisterServiceWorker(
    const GURL& script_url,
    const blink::mojom::ServiceWorkerRegistrationOptions& options,
    RegistrationCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  std::string error_message;
  if (!IsValidRegisterRequest(script_url, options.scope, &error_message)) {
    std::move(callback).Run(
        blink::ServiceWorkerStatusCode::kErrorInvalidArguments, error_message,
        blink::mojom::kInvalidServiceWorkerRegistrationId);
    return;
  }
  was_service_worker_registered_ = true;
  job_coordinator_->Register(
      script_url, options,
      base::BindOnce(&ServiceWorkerContextCore::RegistrationComplete,
                     AsWeakPtr(), options.scope, std::move(callback)));
}

void ServiceWorkerContextCore::UpdateServiceWorker(
    ServiceWorkerRegistration* registration,
    bool force_bypass_cache) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  job_coordinator_->Update(registration, force_bypass_cache);
}

void ServiceWorkerContextCore::UpdateServiceWorker(
    ServiceWorkerRegistration* registration,
    bool force_bypass_cache,
    bool skip_script_comparison,
    UpdateCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  job_coordinator_->Update(
      registration, force_bypass_cache, skip_script_comparison,
      base::BindOnce(&ServiceWorkerContextCore::UpdateComplete, AsWeakPtr(),
                     std::move(callback)));
}

void ServiceWorkerContextCore::UnregisterServiceWorker(
    const GURL& scope,
    UnregistrationCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  job_coordinator_->Unregister(
      scope, base::BindOnce(&ServiceWorkerContextCore::UnregistrationComplete,
                            AsWeakPtr(), scope, std::move(callback)));
}

void ServiceWorkerContextCore::DeleteForOrigin(const GURL& origin,
                                               StatusCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  storage()->GetRegistrationsForOrigin(
      origin,
      base::BindOnce(
          &ServiceWorkerContextCore::DidGetRegistrationsForDeleteForOrigin,
          AsWeakPtr(), std::move(callback)));
}

void ServiceWorkerContextCore::DidGetRegistrationsForDeleteForOrigin(
    base::OnceCallback<void(blink::ServiceWorkerStatusCode)> callback,
    blink::ServiceWorkerStatusCode status,
    const std::vector<scoped_refptr<ServiceWorkerRegistration>>&
        registrations) {
  if (status != blink::ServiceWorkerStatusCode::kOk) {
    std::move(callback).Run(status);
    return;
  }
  bool* overall_success = new bool(true);
  // The barrier must be executed twice for each registration: once for
  // unregistration and once for deletion.
  base::RepeatingClosure barrier = base::BarrierClosure(
      2 * registrations.size(),
      base::BindOnce(&SuccessReportingCallback, base::Owned(overall_success),
                     std::move(callback)));
  for (const auto& registration : registrations) {
    DCHECK(registration);
    if (!registration->is_deleted()) {
      RegistrationDeletionListener::WaitForDeletion(registration, barrier);
    } else {
      barrier.Run();
    }
    UnregisterServiceWorker(
        registration->scope(),
        base::BindOnce(&SuccessCollectorCallback, barrier, overall_success));
  }
}

ServiceWorkerContextCore::ProviderMap*
ServiceWorkerContextCore::GetProviderMapForProcess(int process_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return providers_->Lookup(process_id);
}

void ServiceWorkerContextCore::RegistrationComplete(
    const GURL& scope,
    ServiceWorkerContextCore::RegistrationCallback callback,
    blink::ServiceWorkerStatusCode status,
    const std::string& status_message,
    ServiceWorkerRegistration* registration) {
  if (status != blink::ServiceWorkerStatusCode::kOk) {
    DCHECK(!registration);
    std::move(callback).Run(status, status_message,
                            blink::mojom::kInvalidServiceWorkerRegistrationId);
    return;
  }

  DCHECK(registration);
  std::move(callback).Run(status, status_message, registration->id());
  // At this point the registration promise is resolved, but we haven't
  // persisted anything to storage yet.
  observer_list_->Notify(
      FROM_HERE, &ServiceWorkerContextCoreObserver::OnRegistrationCompleted,
      registration->id(), scope);
}

void ServiceWorkerContextCore::UpdateComplete(
    ServiceWorkerContextCore::UpdateCallback callback,
    blink::ServiceWorkerStatusCode status,
    const std::string& status_message,
    ServiceWorkerRegistration* registration) {
  if (status != blink::ServiceWorkerStatusCode::kOk) {
    DCHECK(!registration);
    std::move(callback).Run(status, status_message,
                            blink::mojom::kInvalidServiceWorkerRegistrationId);
    return;
  }

  DCHECK(registration);
  std::move(callback).Run(status, status_message, registration->id());
}

void ServiceWorkerContextCore::UnregistrationComplete(
    const GURL& scope,
    ServiceWorkerContextCore::UnregistrationCallback callback,
    int64_t registration_id,
    blink::ServiceWorkerStatusCode status) {
  std::move(callback).Run(status);
  if (status == blink::ServiceWorkerStatusCode::kOk) {
    observer_list_->Notify(
        FROM_HERE, &ServiceWorkerContextCoreObserver::OnRegistrationDeleted,
        registration_id, scope);
  }
}

bool ServiceWorkerContextCore::IsValidRegisterRequest(
    const GURL& script_url,
    const GURL& scope_url,
    std::string* out_error) const {
  if (!scope_url.is_valid() || !script_url.is_valid()) {
    *out_error = ServiceWorkerConsts::kBadMessageInvalidURL;
    return false;
  }
  if (ServiceWorkerUtils::ContainsDisallowedCharacter(scope_url, script_url,
                                                      out_error)) {
    return false;
  }
  std::vector<GURL> urls = {scope_url, script_url};
  if (!ServiceWorkerUtils::AllOriginsMatchAndCanAccessServiceWorkers(urls)) {
    *out_error = ServiceWorkerConsts::kBadMessageImproperOrigins;
    return false;
  }
  return true;
}

ServiceWorkerRegistration* ServiceWorkerContextCore::GetLiveRegistration(
    int64_t id) {
  auto it = live_registrations_.find(id);
  return (it != live_registrations_.end()) ? it->second : nullptr;
}

void ServiceWorkerContextCore::AddLiveRegistration(
    ServiceWorkerRegistration* registration) {
  DCHECK(!GetLiveRegistration(registration->id()));
  live_registrations_[registration->id()] = registration;
  observer_list_->Notify(
      FROM_HERE, &ServiceWorkerContextCoreObserver::OnNewLiveRegistration,
      registration->id(), registration->scope());
}

void ServiceWorkerContextCore::RemoveLiveRegistration(int64_t id) {
  live_registrations_.erase(id);
}

ServiceWorkerVersion* ServiceWorkerContextCore::GetLiveVersion(int64_t id) {
  auto it = live_versions_.find(id);
  return (it != live_versions_.end()) ? it->second : nullptr;
}

void ServiceWorkerContextCore::AddLiveVersion(ServiceWorkerVersion* version) {
  // TODO(horo): If we will see crashes here, we have to find the root cause of
  // the version ID conflict. Otherwise change CHECK to DCHECK.
  CHECK(!GetLiveVersion(version->version_id()));
  live_versions_[version->version_id()] = version;
  version->AddObserver(this);
  ServiceWorkerVersionInfo version_info = version->GetInfo();
  observer_list_->Notify(FROM_HERE,
                         &ServiceWorkerContextCoreObserver::OnNewLiveVersion,
                         version_info);
}

void ServiceWorkerContextCore::RemoveLiveVersion(int64_t id) {
  live_versions_.erase(id);
}

std::vector<ServiceWorkerRegistrationInfo>
ServiceWorkerContextCore::GetAllLiveRegistrationInfo() {
  std::vector<ServiceWorkerRegistrationInfo> infos;
  for (std::map<int64_t, ServiceWorkerRegistration*>::const_iterator iter =
           live_registrations_.begin();
       iter != live_registrations_.end(); ++iter) {
    infos.push_back(iter->second->GetInfo());
  }
  return infos;
}

std::vector<ServiceWorkerVersionInfo>
ServiceWorkerContextCore::GetAllLiveVersionInfo() {
  std::vector<ServiceWorkerVersionInfo> infos;
  for (std::map<int64_t, ServiceWorkerVersion*>::const_iterator iter =
           live_versions_.begin();
       iter != live_versions_.end(); ++iter) {
    infos.push_back(iter->second->GetInfo());
  }
  return infos;
}

void ServiceWorkerContextCore::ProtectVersion(
    const scoped_refptr<ServiceWorkerVersion>& version) {
  DCHECK(protected_versions_.find(version->version_id()) ==
         protected_versions_.end());
  protected_versions_[version->version_id()] = version;
}

void ServiceWorkerContextCore::UnprotectVersion(int64_t version_id) {
  DCHECK(protected_versions_.find(version_id) != protected_versions_.end());
  protected_versions_.erase(version_id);
}

void ServiceWorkerContextCore::ScheduleDeleteAndStartOver() const {
  storage_->Disable();
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&ServiceWorkerContextWrapper::DeleteAndStartOver,
                     wrapper_));
}

void ServiceWorkerContextCore::DeleteAndStartOver(StatusCallback callback) {
  job_coordinator_->AbortAll();
  storage_->DeleteAndStartOver(std::move(callback));
}

void ServiceWorkerContextCore::ClearAllServiceWorkersForTest(
    base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // |callback| will be called in the destructor of |helper| on the UI thread.
  auto helper =
      base::MakeRefCounted<ClearAllServiceWorkersHelper>(std::move(callback));
  if (!was_service_worker_registered_)
    return;
  was_service_worker_registered_ = false;
  storage()->GetAllRegistrationsInfos(
      base::BindOnce(&ClearAllServiceWorkersHelper::DidGetAllRegistrations,
                     helper, AsWeakPtr()));
}

void ServiceWorkerContextCore::CheckHasServiceWorker(
    const GURL& url,
    const GURL& other_url,
    ServiceWorkerContext::CheckHasServiceWorkerCallback callback) {
  storage()->FindRegistrationForDocument(
      url, base::BindOnce(&ServiceWorkerContextCore::
                              DidFindRegistrationForCheckHasServiceWorker,
                          AsWeakPtr(), other_url, std::move(callback)));
}

void ServiceWorkerContextCore::UpdateVersionFailureCount(
    int64_t version_id,
    blink::ServiceWorkerStatusCode status) {
  // Don't count these, they aren't start worker failures.
  if (status == blink::ServiceWorkerStatusCode::kErrorDisallowed)
    return;

  auto it = failure_counts_.find(version_id);
  if (it != failure_counts_.end()) {
    ServiceWorkerMetrics::RecordStartStatusAfterFailure(it->second.count,
                                                        status);
  }

  if (status == blink::ServiceWorkerStatusCode::kOk) {
    if (it != failure_counts_.end())
      failure_counts_.erase(it);
    return;
  }

  if (it != failure_counts_.end()) {
    FailureInfo& info = it->second;
    DCHECK_GT(info.count, 0);
    if (info.count < std::numeric_limits<int>::max()) {
      ++info.count;
      info.last_failure = status;
    }
  } else {
    FailureInfo info;
    info.count = 1;
    info.last_failure = status;
    failure_counts_[version_id] = info;
  }
}

int ServiceWorkerContextCore::GetVersionFailureCount(int64_t version_id) {
  auto it = failure_counts_.find(version_id);
  if (it == failure_counts_.end())
    return 0;
  return it->second.count;
}

void ServiceWorkerContextCore::NotifyRegistrationStored(int64_t registration_id,
                                                        const GURL& scope) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  observer_list_->Notify(
      FROM_HERE, &ServiceWorkerContextCoreObserver::OnRegistrationStored,
      registration_id, scope);
}

void ServiceWorkerContextCore::OnStorageWiped() {
  observer_list_->Notify(FROM_HERE,
                         &ServiceWorkerContextCoreObserver::OnStorageWiped);
}

void ServiceWorkerContextCore::OnRunningStateChanged(
    ServiceWorkerVersion* version) {
  observer_list_->Notify(
      FROM_HERE, &ServiceWorkerContextCoreObserver::OnRunningStateChanged,
      version->version_id(), version->running_status());
}

void ServiceWorkerContextCore::OnVersionStateChanged(
    ServiceWorkerVersion* version) {
  observer_list_->Notify(
      FROM_HERE, &ServiceWorkerContextCoreObserver::OnVersionStateChanged,
      version->version_id(), version->scope(), version->status());
}

void ServiceWorkerContextCore::OnDevToolsRoutingIdChanged(
    ServiceWorkerVersion* version) {
  if (!version->embedded_worker())
    return;
  observer_list_->Notify(
      FROM_HERE,
      &ServiceWorkerContextCoreObserver::OnVersionDevToolsRoutingIdChanged,
      version->version_id(), version->embedded_worker()->process_id(),
      version->embedded_worker()->worker_devtools_agent_route_id());
}

void ServiceWorkerContextCore::OnMainScriptHttpResponseInfoSet(
    ServiceWorkerVersion* version) {
  const net::HttpResponseInfo* info = version->GetMainScriptHttpResponseInfo();
  DCHECK(info);
  base::Time lastModified;
  if (info->headers)
    info->headers->GetLastModifiedValue(&lastModified);
  observer_list_->Notify(
      FROM_HERE,
      &ServiceWorkerContextCoreObserver::OnMainScriptHttpResponseInfoSet,
      version->version_id(), info->response_time, lastModified);
}

void ServiceWorkerContextCore::OnErrorReported(
    ServiceWorkerVersion* version,
    const base::string16& error_message,
    int line_number,
    int column_number,
    const GURL& source_url) {
  observer_list_->Notify(
      FROM_HERE, &ServiceWorkerContextCoreObserver::OnErrorReported,
      version->version_id(),
      ServiceWorkerContextCoreObserver::ErrorInfo(error_message, line_number,
                                                  column_number, source_url));
}

void ServiceWorkerContextCore::OnReportConsoleMessage(
    ServiceWorkerVersion* version,
    int source_identifier,
    int message_level,
    const base::string16& message,
    int line_number,
    const GURL& source_url) {
  observer_list_->Notify(
      FROM_HERE, &ServiceWorkerContextCoreObserver::OnReportConsoleMessage,
      version->version_id(),
      ServiceWorkerContextCoreObserver::ConsoleMessage(
          source_identifier, message_level, message, line_number, source_url));
}

void ServiceWorkerContextCore::OnControlleeAdded(
    ServiceWorkerVersion* version,
    const std::string& client_uuid,
    const ServiceWorkerClientInfo& client_info) {
  observer_list_->Notify(
      FROM_HERE, &ServiceWorkerContextCoreObserver::OnControlleeAdded,
      version->version_id(), version->scope(), client_uuid, client_info);
}

void ServiceWorkerContextCore::OnControlleeRemoved(
    ServiceWorkerVersion* version,
    const std::string& client_uuid) {
  observer_list_->Notify(FROM_HERE,
                         &ServiceWorkerContextCoreObserver::OnControlleeRemoved,
                         version->version_id(), version->scope(), client_uuid);
}

void ServiceWorkerContextCore::OnNoControllees(ServiceWorkerVersion* version) {
  observer_list_->Notify(FROM_HERE,
                         &ServiceWorkerContextCoreObserver::OnNoControllees,
                         version->version_id(), version->scope());
}

ServiceWorkerProcessManager* ServiceWorkerContextCore::process_manager() {
  return wrapper_->process_manager();
}

void ServiceWorkerContextCore::DidFindRegistrationForCheckHasServiceWorker(
    const GURL& other_url,
    ServiceWorkerContext::CheckHasServiceWorkerCallback callback,
    blink::ServiceWorkerStatusCode status,
    scoped_refptr<ServiceWorkerRegistration> registration) {
  if (status != blink::ServiceWorkerStatusCode::kOk) {
    std::move(callback).Run(ServiceWorkerCapability::NO_SERVICE_WORKER);
    return;
  }

  if (!ServiceWorkerUtils::ScopeMatches(registration->scope(), other_url)) {
    std::move(callback).Run(ServiceWorkerCapability::NO_SERVICE_WORKER);
    return;
  }

  if (registration->is_uninstalling() || registration->is_uninstalled()) {
    std::move(callback).Run(ServiceWorkerCapability::NO_SERVICE_WORKER);
    return;
  }

  if (!registration->active_version() && !registration->waiting_version()) {
    registration->RegisterRegistrationFinishedCallback(
        base::BindOnce(&ServiceWorkerContextCore::
                           OnRegistrationFinishedForCheckHasServiceWorker,
                       AsWeakPtr(), std::move(callback), registration));
    return;
  }

  CheckFetchHandlerOfInstalledServiceWorker(std::move(callback), registration);
}

void ServiceWorkerContextCore::OnRegistrationFinishedForCheckHasServiceWorker(
    ServiceWorkerContext::CheckHasServiceWorkerCallback callback,
    scoped_refptr<ServiceWorkerRegistration> registration) {
  if (!registration->active_version() && !registration->waiting_version()) {
    std::move(callback).Run(ServiceWorkerCapability::NO_SERVICE_WORKER);
    return;
  }

  CheckFetchHandlerOfInstalledServiceWorker(std::move(callback), registration);
}

}  // namespace content
