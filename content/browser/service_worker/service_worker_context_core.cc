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
#include "base/callback_helpers.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/log_console_message.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/service_worker/embedded_worker_status.h"
#include "content/browser/service_worker/service_worker_consts.h"
#include "content/browser/service_worker/service_worker_container_host.h"
#include "content/browser/service_worker/service_worker_context_core_observer.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_info.h"
#include "content/browser/service_worker/service_worker_job_coordinator.h"
#include "content/browser/service_worker/service_worker_offline_capability_checker.h"
#include "content/browser/service_worker/service_worker_process_manager.h"
#include "content/browser/service_worker/service_worker_register_job.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/url_loader_factory_getter.h"
#include "content/common/content_navigation_policy.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/console_message.h"
#include "content/public/common/child_process_host.h"
#include "content/public/common/url_utils.h"
#include "ipc/ipc_message.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_container_type.mojom.h"
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

// Waits until a |registration| is deleted and calls |callback|.
class RegistrationDeletionListener
    : public ServiceWorkerRegistration::Listener {
 public:
  RegistrationDeletionListener(
      scoped_refptr<ServiceWorkerRegistration> registration,
      base::OnceClosure callback)
      : registration_(std::move(registration)), callback_(std::move(callback)) {
    DCHECK(!registration_->is_deleted());
    registration_->AddListener(this);
  }

  virtual ~RegistrationDeletionListener() {
    registration_->RemoveListener(this);
  }

  void OnRegistrationDeleted(ServiceWorkerRegistration* registration) override {
    if (callback_)
      std::move(callback_).Run();
  }

  scoped_refptr<ServiceWorkerRegistration> registration_;
  base::OnceClosure callback_;
};

// This function will call |callback| after |*expected_calls| reaches zero or
// when an error occurs. In case of an error, |*expected_calls| is set to -1
// to prevent calling |callback| again.
void SuccessReportingCallback(
    int* expected_calls,
    std::vector<std::unique_ptr<RegistrationDeletionListener>>* listeners,
    const base::RepeatingCallback<void(blink::ServiceWorkerStatusCode)>&
        callback,
    blink::ServiceWorkerStatusCode status) {
  if (status != blink::ServiceWorkerStatusCode::kOk) {
    *expected_calls = -1;
    listeners->clear();
    callback.Run(blink::ServiceWorkerStatusCode::kErrorFailed);
    return;
  }
  (*expected_calls)--;
  if (*expected_calls == 0) {
    listeners->clear();
    callback.Run(blink::ServiceWorkerStatusCode::kOk);
  }
}

bool IsSameOriginClientContainerHost(
    const GURL& origin,
    bool allow_reserved_client,
    bool allow_back_forward_cached_client,
    ServiceWorkerContainerHost* container_host) {
  DCHECK(container_host->IsContainerForClient());
  // If |container_host| is in BackForwardCache, it should be skipped in
  // iteration, because (1) hosts in BackForwardCache should never be exposed to
  // web as clients and (2) hosts could be in an unknown state after eviction
  // and before deletion.
  // When |allow_back_forward_cached_client| is true, do not skip the cached
  // client.
  if (!allow_back_forward_cached_client &&
      container_host->IsInBackForwardCache()) {
    return false;
  }
  return container_host->url().GetOrigin() == origin &&
         (allow_reserved_client || container_host->is_execution_ready());
}

bool IsSameOriginWindowClientContainerHost(
    const GURL& origin,
    bool allow_reserved_client,
    ServiceWorkerContainerHost* container_host) {
  DCHECK(container_host->IsContainerForClient());
  // If |container_host| is in BackForwardCache, it should be skipped in
  // iteration, because (1) hosts in BackForwardCache should never be exposed to
  // web as clients and (2) hosts could be in an unknown state after eviction
  // and before deletion.
  if (IsBackForwardCacheEnabled()) {
    if (container_host->IsInBackForwardCache())
      return false;
  }
  return container_host->IsContainerForWindowClient() &&
         container_host->url().GetOrigin() == origin &&
         (allow_reserved_client || container_host->is_execution_ready());
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
    DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  }

  void OnResult(blink::ServiceWorkerStatusCode) {
    DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
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
          registration_info.scope, /*is_immediate=*/false,
          base::BindOnce(&ClearAllServiceWorkersHelper::OnResult, this));
    }
  }

 private:
  friend class base::RefCounted<ClearAllServiceWorkersHelper>;
  ~ClearAllServiceWorkersHelper() {
    DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
    GetUIThreadTaskRunner({})->PostTask(FROM_HERE, std::move(callback_));
  }

  base::OnceClosure callback_;
  DISALLOW_COPY_AND_ASSIGN(ClearAllServiceWorkersHelper);
};

}  // namespace

ServiceWorkerContextCore::ContainerHostIterator::~ContainerHostIterator() =
    default;

ServiceWorkerContainerHost*
ServiceWorkerContextCore::ContainerHostIterator::GetContainerHost() {
  DCHECK(!IsAtEnd());
  return container_host_iterator_->second.get();
}

void ServiceWorkerContextCore::ContainerHostIterator::Advance() {
  DCHECK(!IsAtEnd());
  container_host_iterator_++;
  ForwardUntilMatchingContainerHost();
}

bool ServiceWorkerContextCore::ContainerHostIterator::IsAtEnd() {
  return container_host_iterator_ == map_->end();
}

ServiceWorkerContextCore::ContainerHostIterator::ContainerHostIterator(
    ContainerHostByClientUUIDMap* map,
    ContainerHostPredicate predicate)
    : map_(map),
      predicate_(std::move(predicate)),
      container_host_iterator_(map_->begin()) {
  ForwardUntilMatchingContainerHost();
}

void ServiceWorkerContextCore::ContainerHostIterator::
    ForwardUntilMatchingContainerHost() {
  while (!IsAtEnd()) {
    if (predicate_.is_null() || predicate_.Run(GetContainerHost()))
      return;
    container_host_iterator_++;
  }
  return;
}

ServiceWorkerContextCore::ServiceWorkerContextCore(
    const base::FilePath& user_data_directory,
    scoped_refptr<base::SequencedTaskRunner> database_task_runner,
    storage::QuotaManagerProxy* quota_manager_proxy,
    storage::SpecialStoragePolicy* special_storage_policy,
    URLLoaderFactoryGetter* url_loader_factory_getter,
    std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
        non_network_pending_loader_factory_bundle_for_update_check,
    base::ObserverListThreadSafe<ServiceWorkerContextCoreObserver>*
        observer_list,
    ServiceWorkerContextWrapper* wrapper)
    : wrapper_(wrapper),
      container_host_receivers_(std::make_unique<mojo::AssociatedReceiverSet<
                                    blink::mojom::ServiceWorkerContainerHost,
                                    ServiceWorkerContainerHost*>>()),
      registry_(std::make_unique<ServiceWorkerRegistry>(
          user_data_directory,
          this,
          std::move(database_task_runner),
          quota_manager_proxy,
          special_storage_policy)),
      job_coordinator_(std::make_unique<ServiceWorkerJobCoordinator>(this)),
      loader_factory_getter_(url_loader_factory_getter),
      force_update_on_page_load_(false),
      was_service_worker_registered_(false),
      observer_list_(observer_list) {
  DCHECK(observer_list_);
  if (non_network_pending_loader_factory_bundle_for_update_check) {
    loader_factory_bundle_for_update_check_ =
        base::MakeRefCounted<blink::URLLoaderFactoryBundle>(std::move(
            non_network_pending_loader_factory_bundle_for_update_check));
  }

  container_host_receivers_->set_disconnect_handler(base::BindRepeating(
      &ServiceWorkerContextCore::OnContainerHostReceiverDisconnected,
      base::Unretained(this)));
}

ServiceWorkerContextCore::ServiceWorkerContextCore(
    ServiceWorkerContextCore* old_context,
    ServiceWorkerContextWrapper* wrapper)
    : wrapper_(wrapper),
      container_host_by_uuid_(std::move(old_context->container_host_by_uuid_)),
      container_host_receivers_(
          std::move(old_context->container_host_receivers_)),
      registry_(
          std::make_unique<ServiceWorkerRegistry>(this,
                                                  old_context->registry())),
      job_coordinator_(std::make_unique<ServiceWorkerJobCoordinator>(this)),
      loader_factory_getter_(old_context->loader_factory_getter()),
      loader_factory_bundle_for_update_check_(
          std::move(old_context->loader_factory_bundle_for_update_check_)),
      was_service_worker_registered_(
          old_context->was_service_worker_registered_),
      observer_list_(old_context->observer_list_),
      next_embedded_worker_id_(old_context->next_embedded_worker_id_) {
  container_host_receivers_->set_disconnect_handler(base::BindRepeating(
      &ServiceWorkerContextCore::OnContainerHostReceiverDisconnected,
      base::Unretained(this)));
}

ServiceWorkerContextCore::~ServiceWorkerContextCore() {
  DCHECK(registry_);
  for (const auto& it : live_versions_)
    it.second->RemoveObserver(this);

  job_coordinator_->ClearForShutdown();
}

std::unique_ptr<ServiceWorkerContextCore::ContainerHostIterator>
ServiceWorkerContextCore::GetClientContainerHostIterator(
    const GURL& origin,
    bool include_reserved_clients,
    bool include_back_forward_cached_clients) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  return base::WrapUnique(new ContainerHostIterator(
      &container_host_by_uuid_,
      base::BindRepeating(IsSameOriginClientContainerHost, origin,
                          include_reserved_clients,
                          include_back_forward_cached_clients)));
}

std::unique_ptr<ServiceWorkerContextCore::ContainerHostIterator>
ServiceWorkerContextCore::GetWindowClientContainerHostIterator(
    const GURL& origin,
    bool include_reserved_clients) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  return base::WrapUnique(new ContainerHostIterator(
      &container_host_by_uuid_,
      base::BindRepeating(IsSameOriginWindowClientContainerHost, origin,
                          include_reserved_clients)));
}

void ServiceWorkerContextCore::HasMainFrameWindowClient(const GURL& origin,
                                                        BoolCallback callback) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  std::unique_ptr<ContainerHostIterator> container_host_iterator =
      GetWindowClientContainerHostIterator(origin,
                                           /*include_reserved_clients=*/false);

  if (container_host_iterator->IsAtEnd()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
    return;
  }

  std::unique_ptr<std::vector<std::pair<int, int>>> render_frames(
      new std::vector<std::pair<int, int>>());

  while (!container_host_iterator->IsAtEnd()) {
    ServiceWorkerContainerHost* container_host =
        container_host_iterator->GetContainerHost();
    DCHECK(container_host->IsContainerForWindowClient());
    render_frames->push_back(std::make_pair(container_host->process_id(),
                                            container_host->frame_id()));
    container_host_iterator->Advance();
  }

  if (ServiceWorkerContext::IsServiceWorkerOnUIEnabled()) {
    bool result = FrameListContainsMainFrameOnUI(std::move(render_frames));
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), result));
  } else {
    GetUIThreadTaskRunner({})->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(&FrameListContainsMainFrameOnUI,
                       std::move(render_frames)),
        std::move(callback));
  }
}

base::WeakPtr<ServiceWorkerContainerHost>
ServiceWorkerContextCore::CreateContainerHostForWindow(
    mojo::PendingAssociatedReceiver<blink::mojom::ServiceWorkerContainerHost>
        host_receiver,
    bool are_ancestors_secure,
    mojo::PendingAssociatedRemote<blink::mojom::ServiceWorkerContainer>
        container_remote,
    int frame_tree_node_id) {
  auto container_host = std::make_unique<ServiceWorkerContainerHost>(
      AsWeakPtr(), are_ancestors_secure, std::move(container_remote),
      frame_tree_node_id);

  ServiceWorkerContainerHost* container_host_ptr = container_host.get();

  auto inserted =
      container_host_by_uuid_
          .emplace(container_host_ptr->client_uuid(), std::move(container_host))
          .second;
  DCHECK(inserted);

  // Bind the host receiver.
  container_host_receivers_->Add(container_host_ptr, std::move(host_receiver),
                                 container_host_ptr);

  return container_host_ptr->GetWeakPtr();
}

base::WeakPtr<ServiceWorkerContainerHost>
ServiceWorkerContextCore::CreateContainerHostForWorker(
    mojo::PendingAssociatedReceiver<blink::mojom::ServiceWorkerContainerHost>
        host_receiver,
    int process_id,
    mojo::PendingAssociatedRemote<blink::mojom::ServiceWorkerContainer>
        container_remote,
    ServiceWorkerClientInfo client_info) {
  auto container_host = std::make_unique<ServiceWorkerContainerHost>(
      AsWeakPtr(), process_id, std::move(container_remote), client_info);

  ServiceWorkerContainerHost* container_host_ptr = container_host.get();

  bool inserted =
      container_host_by_uuid_
          .emplace(container_host_ptr->client_uuid(), std::move(container_host))
          .second;
  DCHECK(inserted);

  // Bind the host receiver.
  container_host_receivers_->Add(container_host_ptr, std::move(host_receiver),
                                 container_host_ptr);

  return container_host_ptr->GetWeakPtr();
}

void ServiceWorkerContextCore::UpdateContainerHostClientID(
    const std::string& current_client_uuid,
    const std::string& new_client_uuid) {
  auto it = container_host_by_uuid_.find(current_client_uuid);
  DCHECK(it != container_host_by_uuid_.end());
  std::unique_ptr<ServiceWorkerContainerHost> container_host =
      std::move(it->second);
  container_host_by_uuid_.erase(it);

  bool inserted = container_host_by_uuid_
                      .emplace(new_client_uuid, std::move(container_host))
                      .second;
  DCHECK(inserted);
}

ServiceWorkerContainerHost*
ServiceWorkerContextCore::GetContainerHostByClientID(
    const std::string& client_uuid) {
  auto it = container_host_by_uuid_.find(client_uuid);
  if (it == container_host_by_uuid_.end())
    return nullptr;
  DCHECK(it->second->IsContainerForClient());
  return it->second.get();
}

void ServiceWorkerContextCore::OnContainerHostReceiverDisconnected() {
  ServiceWorkerContainerHost* container_host =
      container_host_receivers_->current_context();

  size_t removed = container_host_by_uuid_.erase(container_host->client_uuid());
  DCHECK_EQ(removed, 1u);
}

void ServiceWorkerContextCore::RegisterServiceWorker(
    const GURL& script_url,
    const blink::mojom::ServiceWorkerRegistrationOptions& options,
    blink::mojom::FetchClientSettingsObjectPtr
        outside_fetch_client_settings_object,
    RegistrationCallback callback) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  std::string error_message;
  if (!IsValidRegisterRequest(script_url, options.scope, &error_message)) {
    std::move(callback).Run(
        blink::ServiceWorkerStatusCode::kErrorInvalidArguments, error_message,
        blink::mojom::kInvalidServiceWorkerRegistrationId);
    return;
  }
  was_service_worker_registered_ = true;
  job_coordinator_->Register(
      script_url, options, std::move(outside_fetch_client_settings_object),
      base::BindOnce(&ServiceWorkerContextCore::RegistrationComplete,
                     AsWeakPtr(), options.scope, std::move(callback)));
}

void ServiceWorkerContextCore::UpdateServiceWorker(
    ServiceWorkerRegistration* registration,
    bool force_bypass_cache) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  job_coordinator_->Update(registration, force_bypass_cache);
}

void ServiceWorkerContextCore::UpdateServiceWorker(
    ServiceWorkerRegistration* registration,
    bool force_bypass_cache,
    bool skip_script_comparison,
    blink::mojom::FetchClientSettingsObjectPtr
        outside_fetch_client_settings_object,
    UpdateCallback callback) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  job_coordinator_->Update(
      registration, force_bypass_cache, skip_script_comparison,
      std::move(outside_fetch_client_settings_object),
      base::BindOnce(&ServiceWorkerContextCore::UpdateComplete, AsWeakPtr(),
                     std::move(callback)));
}

void ServiceWorkerContextCore::UnregisterServiceWorker(
    const GURL& scope,
    bool is_immediate,
    UnregistrationCallback callback) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  job_coordinator_->Unregister(
      scope, is_immediate,
      base::BindOnce(&ServiceWorkerContextCore::UnregistrationComplete,
                     AsWeakPtr(), scope, std::move(callback)));
}

void ServiceWorkerContextCore::DeleteForOrigin(const url::Origin& origin,
                                               StatusCallback callback) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  registry()->GetRegistrationsForOrigin(
      origin,
      base::BindOnce(
          &ServiceWorkerContextCore::DidGetRegistrationsForDeleteForOrigin,
          AsWeakPtr(), origin, std::move(callback)));
}

void ServiceWorkerContextCore::PerformStorageCleanup(
    base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  GetStorageControl()->PerformStorageCleanup(std::move(callback));
}

void ServiceWorkerContextCore::DidGetRegistrationsForDeleteForOrigin(
    const url::Origin& origin,
    base::OnceCallback<void(blink::ServiceWorkerStatusCode)> callback,
    blink::ServiceWorkerStatusCode status,
    const std::vector<scoped_refptr<ServiceWorkerRegistration>>&
        registrations) {
  if (status != blink::ServiceWorkerStatusCode::kOk) {
    std::move(callback).Run(status);
    return;
  }

  // Clear all unregistered registrations that are waiting for controllees to
  // unload.
  std::vector<scoped_refptr<ServiceWorkerRegistration>>
      uninstalling_registrations =
          registry()->GetUninstallingRegistrationsForOrigin(origin);
  for (const auto& uninstalling_registration : uninstalling_registrations) {
    job_coordinator_->Abort(uninstalling_registration->scope());
    uninstalling_registration->DeleteAndClearImmediately();
  }

  if (registrations.empty()) {
    std::move(callback).Run(blink::ServiceWorkerStatusCode::kOk);
    return;
  }

  int* expected_calls = new int(2 * registrations.size());
  auto* listeners =
      new std::vector<std::unique_ptr<RegistrationDeletionListener>>();

  // The barrier must be executed twice for each registration: once for
  // unregistration and once for deletion. It will call |callback| immediately
  // if an error occurs.
  base::RepeatingCallback<void(blink::ServiceWorkerStatusCode)> barrier =
      base::BindRepeating(SuccessReportingCallback, base::Owned(expected_calls),
                          base::Owned(listeners),
                          base::AdaptCallbackForRepeating(std::move(callback)));
  for (const auto& registration : registrations) {
    DCHECK(registration);
    if (*expected_calls != -1) {
      if (!registration->is_deleted()) {
        listeners->emplace_back(std::make_unique<RegistrationDeletionListener>(
            registration,
            base::BindOnce(barrier, blink::ServiceWorkerStatusCode::kOk)));
      } else {
        barrier.Run(blink::ServiceWorkerStatusCode::kOk);
      }
    }
    job_coordinator_->Abort(registration->scope());
    UnregisterServiceWorker(registration->scope(), /*is_immediate=*/true,
                            barrier);
  }
}

int ServiceWorkerContextCore::GetNextEmbeddedWorkerId() {
  return next_embedded_worker_id_++;
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
  DCHECK(live_registrations_.find(id) != live_registrations_.end());
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
  auto it = live_versions_.find(id);
  DCHECK(it != live_versions_.end());
  ServiceWorkerVersion* version = it->second;

  if (version->running_status() != EmbeddedWorkerStatus::STOPPED) {
    // Notify all observers that this live version is stopped, as it will
    // be removed from |live_versions_|.
    observer_list_->Notify(FROM_HERE,
                           &ServiceWorkerContextCoreObserver::OnStopped, id);
  }

  live_versions_.erase(it);
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
  registry()->PrepareForDeleteAndStarOver();
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&ServiceWorkerContextWrapper::DeleteAndStartOver,
                     wrapper_));
}

void ServiceWorkerContextCore::DeleteAndStartOver(StatusCallback callback) {
  job_coordinator_->AbortAll();

  observer_list_->Notify(
      FROM_HERE, &ServiceWorkerContextCoreObserver::OnDeleteAndStartOver);

  registry()->DeleteAndStartOver(std::move(callback));
}

void ServiceWorkerContextCore::ClearAllServiceWorkersForTest(
    base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  // |callback| will be called in the destructor of |helper| on the UI thread.
  auto helper =
      base::MakeRefCounted<ClearAllServiceWorkersHelper>(std::move(callback));
  if (!was_service_worker_registered_)
    return;
  was_service_worker_registered_ = false;
  registry()->GetAllRegistrationsInfos(
      base::BindOnce(&ClearAllServiceWorkersHelper::DidGetAllRegistrations,
                     helper, AsWeakPtr()));
}

void ServiceWorkerContextCore::CheckHasServiceWorker(
    const GURL& url,
    ServiceWorkerContext::CheckHasServiceWorkerCallback callback) {
  registry()->FindRegistrationForClientUrl(
      url, base::BindOnce(&ServiceWorkerContextCore::
                              DidFindRegistrationForCheckHasServiceWorker,
                          AsWeakPtr(), std::move(callback)));
}

void ServiceWorkerContextCore::CheckOfflineCapability(
    const GURL& url,
    ServiceWorkerContext::CheckOfflineCapabilityCallback callback) {
  auto checker = std::make_unique<ServiceWorkerOfflineCapabilityChecker>(url);
  ServiceWorkerOfflineCapabilityChecker* checker_rawptr = checker.get();
  checker_rawptr->Start(
      registry(),
      // Bind unique_ptr to the |callback| so that
      // ServiceWorkerOfflineCapabilityChecker outlives |callback| and is surely
      // freed when |callback| is called.
      base::BindOnce(
          [](std::unique_ptr<ServiceWorkerOfflineCapabilityChecker> checker,
             ServiceWorkerContext::CheckOfflineCapabilityCallback callback,
             OfflineCapability result) { std::move(callback).Run(result); },
          std::move(checker), std::move(callback)));
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
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  observer_list_->Notify(
      FROM_HERE, &ServiceWorkerContextCoreObserver::OnRegistrationStored,
      registration_id, scope);
}

void ServiceWorkerContextCore::NotifyAllRegistrationsDeletedForOrigin(
    const url::Origin& origin) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  observer_list_->Notify(
      FROM_HERE,
      &ServiceWorkerContextCoreObserver::OnAllRegistrationsDeletedForOrigin,
      origin);
}

void ServiceWorkerContextCore::OnStorageWiped() {
  observer_list_->Notify(FROM_HERE,
                         &ServiceWorkerContextCoreObserver::OnStorageWiped);
}

void ServiceWorkerContextCore::OnMainScriptResponseSet(
    int64_t version_id,
    const ServiceWorkerVersion::MainScriptResponse& response) {
  observer_list_->Notify(
      FROM_HERE, &ServiceWorkerContextCoreObserver::OnMainScriptResponseSet,
      version_id, response.response_time, response.last_modified);
}

void ServiceWorkerContextCore::OnControlleeAdded(
    ServiceWorkerVersion* version,
    const std::string& client_uuid,
    const ServiceWorkerClientInfo& client_info) {
  DCHECK_EQ(this, version->context().get());
  observer_list_->Notify(FROM_HERE,
                         &ServiceWorkerContextCoreObserver::OnControlleeAdded,
                         version->version_id(), client_uuid, client_info);
}

void ServiceWorkerContextCore::OnControlleeRemoved(
    ServiceWorkerVersion* version,
    const std::string& client_uuid) {
  DCHECK_EQ(this, version->context().get());
  observer_list_->Notify(FROM_HERE,
                         &ServiceWorkerContextCoreObserver::OnControlleeRemoved,
                         version->version_id(), client_uuid);
}

void ServiceWorkerContextCore::OnNoControllees(ServiceWorkerVersion* version) {
  DCHECK_EQ(this, version->context().get());

  ServiceWorkerRegistration* registration =
      GetLiveRegistration(version->registration_id());
  if (registration)
    registration->OnNoControllees(version);

  observer_list_->Notify(FROM_HERE,
                         &ServiceWorkerContextCoreObserver::OnNoControllees,
                         version->version_id(), version->scope());
}

void ServiceWorkerContextCore::OnControlleeNavigationCommitted(
    ServiceWorkerVersion* version,
    const std::string& client_uuid,
    GlobalFrameRoutingId render_frame_host_id) {
  DCHECK_EQ(this, version->context().get());

  observer_list_->Notify(
      FROM_HERE,
      &ServiceWorkerContextCoreObserver::OnControlleeNavigationCommitted,
      version->version_id(), client_uuid, render_frame_host_id);
}

void ServiceWorkerContextCore::OnRunningStateChanged(
    ServiceWorkerVersion* version) {
  DCHECK_EQ(this, version->context().get());
  switch (version->running_status()) {
    case EmbeddedWorkerStatus::STOPPED:
      observer_list_->Notify(FROM_HERE,
                             &ServiceWorkerContextCoreObserver::OnStopped,
                             version->version_id());
      break;
    case EmbeddedWorkerStatus::STARTING:
      observer_list_->Notify(FROM_HERE,
                             &ServiceWorkerContextCoreObserver::OnStarting,
                             version->version_id());
      break;
    case EmbeddedWorkerStatus::RUNNING:
      observer_list_->Notify(
          FROM_HERE, &ServiceWorkerContextCoreObserver::OnStarted,
          version->version_id(), version->scope(),
          version->embedded_worker()->process_id(), version->script_url(),
          version->embedded_worker()->token().value());
      break;
    case EmbeddedWorkerStatus::STOPPING:
      observer_list_->Notify(FROM_HERE,
                             &ServiceWorkerContextCoreObserver::OnStopping,
                             version->version_id());
      break;
  }
}

void ServiceWorkerContextCore::OnVersionStateChanged(
    ServiceWorkerVersion* version) {
  DCHECK_EQ(this, version->context().get());
  observer_list_->Notify(
      FROM_HERE, &ServiceWorkerContextCoreObserver::OnVersionStateChanged,
      version->version_id(), version->scope(), version->status());
}

void ServiceWorkerContextCore::OnDevToolsRoutingIdChanged(
    ServiceWorkerVersion* version) {
  DCHECK_EQ(this, version->context().get());
  if (!version->embedded_worker())
    return;
  observer_list_->Notify(
      FROM_HERE,
      &ServiceWorkerContextCoreObserver::OnVersionDevToolsRoutingIdChanged,
      version->version_id(), version->embedded_worker()->process_id(),
      version->embedded_worker()->worker_devtools_agent_route_id());
}

void ServiceWorkerContextCore::OnErrorReported(
    ServiceWorkerVersion* version,
    const base::string16& error_message,
    int line_number,
    int column_number,
    const GURL& source_url) {
  DCHECK_EQ(this, version->context().get());
  observer_list_->Notify(
      FROM_HERE, &ServiceWorkerContextCoreObserver::OnErrorReported,
      version->version_id(), version->scope(),
      ServiceWorkerContextObserver::ErrorInfo(error_message, line_number,
                                              column_number, source_url));
}

void ServiceWorkerContextCore::OnReportConsoleMessage(
    ServiceWorkerVersion* version,
    blink::mojom::ConsoleMessageSource source,
    blink::mojom::ConsoleMessageLevel message_level,
    const base::string16& message,
    int line_number,
    const GURL& source_url) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  DCHECK_EQ(this, version->context().get());
  // NOTE: This differs slightly from
  // RenderFrameHostImpl::DidAddMessageToConsole, which also asks the
  // content embedder whether to classify the message as a builtin component.
  // This is called on the IO thread, though, so we can't easily get a
  // BrowserContext and call ContentBrowserClient::IsBuiltinComponent().
  const bool is_builtin_component = HasWebUIScheme(source_url);

  LogConsoleMessage(message_level, message, line_number, is_builtin_component,
                    wrapper_->is_incognito(),
                    base::UTF8ToUTF16(source_url.spec()));

  observer_list_->Notify(
      FROM_HERE, &ServiceWorkerContextCoreObserver::OnReportConsoleMessage,
      version->version_id(), version->scope(),
      ConsoleMessage(source, message_level, message, line_number, source_url));
}

mojo::Remote<storage::mojom::ServiceWorkerStorageControl>&
ServiceWorkerContextCore::GetStorageControl() {
  return registry_->GetRemoteStorageControl();
}

ServiceWorkerProcessManager* ServiceWorkerContextCore::process_manager() {
  return wrapper_->process_manager();
}

void ServiceWorkerContextCore::DidFindRegistrationForCheckHasServiceWorker(
    ServiceWorkerContext::CheckHasServiceWorkerCallback callback,
    blink::ServiceWorkerStatusCode status,
    scoped_refptr<ServiceWorkerRegistration> registration) {
  if (status != blink::ServiceWorkerStatusCode::kOk) {
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
