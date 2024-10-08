// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_context_core.h"

#include <limits>
#include <memory>
#include <set>
#include <tuple>
#include <utility>

#include "base/barrier_closure.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/not_fatal_until.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_runner.h"
#include "components/services/storage/public/cpp/quota_client_callback_wrapper.h"
#include "content/browser/log_console_message.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/service_worker/service_worker_client.h"
#include "content/browser/service_worker/service_worker_consts.h"
#include "content/browser/service_worker/service_worker_container_host.h"
#include "content/browser/service_worker/service_worker_context_core_observer.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_hid_delegate_observer.h"
#include "content/browser/service_worker/service_worker_host.h"
#include "content/browser/service_worker/service_worker_info.h"
#include "content/browser/service_worker/service_worker_job_coordinator.h"
#include "content/browser/service_worker/service_worker_process_manager.h"
#include "content/browser/service_worker/service_worker_quota_client.h"
#include "content/browser/service_worker/service_worker_register_job.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_security_utils.h"
#include "content/browser/service_worker/service_worker_usb_delegate_observer.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/browser/storage_partition_impl.h"
#include "content/common/content_navigation_policy.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_host.h"
#include "content/public/browser/console_message.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/service_worker_running_info.h"
#include "content/public/common/url_utils.h"
#include "ipc/ipc_message.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/service_worker/embedded_worker_status.h"
#include "third_party/blink/public/common/service_worker/service_worker_scope_match.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_container_type.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration_options.mojom.h"
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
    base::OnceCallback<void(blink::ServiceWorkerStatusCode)>& callback,
    blink::ServiceWorkerStatusCode status) {
  if (status != blink::ServiceWorkerStatusCode::kOk) {
    if (*expected_calls > 0) {
      *expected_calls = -1;
      listeners->clear();
      std::move(callback).Run(blink::ServiceWorkerStatusCode::kErrorFailed);
    }
    return;
  }
  (*expected_calls)--;
  if (*expected_calls == 0) {
    listeners->clear();
    std::move(callback).Run(blink::ServiceWorkerStatusCode::kOk);
  }
}

bool IsSameOriginServiceWorkerClient(
    const blink::StorageKey& key,
    bool allow_reserved_client,
    bool allow_back_forward_cached_client,
    ServiceWorkerClient& service_worker_client) {
  // If |service_worker_client| is in BackForwardCache, it should be skipped in
  // iteration, because (1) hosts in BackForwardCache should never be exposed to
  // web as clients and (2) hosts could be in an unknown state after eviction
  // and before deletion.
  // When |allow_back_forward_cached_client| is true, do not skip the cached
  // client.
  if (!allow_back_forward_cached_client &&
      service_worker_client.IsInBackForwardCache()) {
    return false;
  }
  return service_worker_client.key() == key &&
         (allow_reserved_client || service_worker_client.is_execution_ready());
}

bool IsSameOriginWindowServiceWorkerClient(
    const blink::StorageKey& key,
    bool allow_reserved_client,
    ServiceWorkerClient& service_worker_client) {
  // If |service_worker_client| is in BackForwardCache, it should be skipped in
  // iteration, because (1) service worker clients in BackForwardCache should
  // never be exposed to web as clients and (2) service worker clients could be
  // in an unknown state after eviction and before deletion.
  if (IsBackForwardCacheEnabled()) {
    if (service_worker_client.IsInBackForwardCache()) {
      return false;
    }
  }
  return service_worker_client.IsContainerForWindowClient() &&
         service_worker_client.key() == key &&
         (allow_reserved_client || service_worker_client.is_execution_ready());
}

class ClearAllServiceWorkersHelper
    : public base::RefCounted<ClearAllServiceWorkersHelper> {
 public:
  explicit ClearAllServiceWorkersHelper(base::OnceClosure callback)
      : callback_(std::move(callback)) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
  }

  ClearAllServiceWorkersHelper(const ClearAllServiceWorkersHelper&) = delete;
  ClearAllServiceWorkersHelper& operator=(const ClearAllServiceWorkersHelper&) =
      delete;

  void OnResult(blink::ServiceWorkerStatusCode) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
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
    const std::map<int64_t, raw_ptr<ServiceWorkerVersion, CtnExperimental>>
        live_versions_copy = context->GetLiveVersions();
    for (const auto& version_itr : live_versions_copy) {
      ServiceWorkerVersion* version(version_itr.second);
      if (version->running_status() == blink::EmbeddedWorkerStatus::kStarting ||
          version->running_status() == blink::EmbeddedWorkerStatus::kRunning) {
        version->StopWorker(base::DoNothing());
      }
    }
    for (const auto& registration_info : registrations) {
      context->UnregisterServiceWorker(
          registration_info.scope, registration_info.key,
          /*is_immediate=*/false,
          base::BindOnce(&ClearAllServiceWorkersHelper::OnResult, this));
    }
  }

 private:
  friend class base::RefCounted<ClearAllServiceWorkersHelper>;
  ~ClearAllServiceWorkersHelper() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    GetUIThreadTaskRunner({})->PostTask(FROM_HERE, std::move(callback_));
  }

  base::OnceClosure callback_;
};

int GetWarmedUpServiceWorkerCount(
    const std::map<int64_t, raw_ptr<ServiceWorkerVersion, CtnExperimental>>&
        live_versions) {
  return base::ranges::count_if(live_versions, [](const auto& iter) {
    ServiceWorkerVersion& service_worker_version = *iter.second;
    return service_worker_version.IsWarmingUp() ||
           service_worker_version.IsWarmedUp();
  });
}

}  // namespace

ServiceWorkerClientOwner::ServiceWorkerClientIterator::
    ~ServiceWorkerClientIterator() = default;

ServiceWorkerClient&
ServiceWorkerClientOwner::ServiceWorkerClientIterator::operator*() const {
  DCHECK(!IsAtEnd());
  return *iterator_->second;
}

ServiceWorkerClient*
ServiceWorkerClientOwner::ServiceWorkerClientIterator::operator->() const {
  DCHECK(!IsAtEnd());
  return iterator_->second.get();
}

ServiceWorkerClientOwner::ServiceWorkerClientIterator&
ServiceWorkerClientOwner::ServiceWorkerClientIterator::operator++() {
  DCHECK(!IsAtEnd());
  ++iterator_;
  ForwardUntilMatchingServiceWorkerClient();
  return *this;
}

bool ServiceWorkerClientOwner::ServiceWorkerClientIterator::IsAtEnd() const {
  return iterator_ == map_->end();
}

ServiceWorkerClientOwner::ServiceWorkerClientIterator::
    ServiceWorkerClientIterator(ServiceWorkerClientByClientUUIDMap* map,
                                ServiceWorkerClientPredicate predicate)
    : map_(map), predicate_(std::move(predicate)), iterator_(map_->begin()) {
  ForwardUntilMatchingServiceWorkerClient();
}

void ServiceWorkerClientOwner::ServiceWorkerClientIterator::
    ForwardUntilMatchingServiceWorkerClient() {
  while (!IsAtEnd()) {
    if (predicate_.is_null() || predicate_.Run(**this)) {
      return;
    }
    ++iterator_;
  }
  return;
}

ServiceWorkerClientOwner::ServiceWorkerClientOwner(
    ServiceWorkerContextCore& context)
    : context_(context),
      container_host_receivers_(std::make_unique<mojo::AssociatedReceiverSet<
                                    blink::mojom::ServiceWorkerContainerHost,
                                    ServiceWorkerContainerHostForClient*>>()) {
  container_host_receivers_->set_disconnect_handler(base::BindRepeating(
      &ServiceWorkerClientOwner::OnContainerHostReceiverDisconnected,
      base::Unretained(this)));
}

ServiceWorkerClientOwner::~ServiceWorkerClientOwner() = default;

void ServiceWorkerClientOwner::ResetContext(
    ServiceWorkerContextCore& new_context) {
  context_ = new_context;
}

ServiceWorkerContextCore::ServiceWorkerContextCore(
    storage::QuotaManagerProxy* quota_manager_proxy,
    storage::SpecialStoragePolicy* special_storage_policy,
    std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
        non_network_pending_loader_factory_bundle_for_update_check,
    base::ObserverListThreadSafe<ServiceWorkerContextCoreObserver>*
        observer_list,
    ServiceWorkerContextSynchronousObserverList* synchronous_observer_list,
    ServiceWorkerContextWrapper* wrapper)
    : wrapper_(wrapper),
      service_worker_client_owner_(
          std::make_unique<ServiceWorkerClientOwner>(*this)),
      registry_(
          std::make_unique<ServiceWorkerRegistry>(this,
                                                  quota_manager_proxy,
                                                  special_storage_policy)),
      job_coordinator_(std::make_unique<ServiceWorkerJobCoordinator>(this)),
      force_update_on_page_load_(false),
      was_service_worker_registered_(false),
      observer_list_(observer_list),
      sync_observer_list_(synchronous_observer_list),
      quota_client_(std::make_unique<ServiceWorkerQuotaClient>(*this)),
      quota_client_wrapper_(
          std::make_unique<storage::QuotaClientCallbackWrapper>(
              quota_client_.get())),
      quota_client_receiver_(
          std::make_unique<mojo::Receiver<storage::mojom::QuotaClient>>(
              quota_client_wrapper_.get())) {
  DCHECK(observer_list_);
  if (non_network_pending_loader_factory_bundle_for_update_check) {
    loader_factory_bundle_for_update_check_ =
        base::MakeRefCounted<blink::URLLoaderFactoryBundle>(std::move(
            non_network_pending_loader_factory_bundle_for_update_check));
  }

  if (quota_manager_proxy) {
    quota_manager_proxy->RegisterClient(
        quota_client_receiver_->BindNewPipeAndPassRemote(),
        storage::QuotaClientType::kServiceWorker,
        {blink::mojom::StorageType::kTemporary});
  }

  registry_->GetRegisteredStorageKeys(
      base::BindOnce(&ServiceWorkerContextCore::DidGetRegisteredStorageKeys,
                     AsWeakPtr(), base::TimeTicks::Now()));
}

ServiceWorkerContextCore::ServiceWorkerContextCore(
    std::unique_ptr<ServiceWorkerContextCore> old_context,
    ServiceWorkerContextWrapper* wrapper)
    : wrapper_(wrapper),
      service_worker_client_owner_(
          std::move(old_context->service_worker_client_owner_)),
      registry_(
          std::make_unique<ServiceWorkerRegistry>(this,
                                                  old_context->registry())),
      job_coordinator_(std::make_unique<ServiceWorkerJobCoordinator>(this)),
      loader_factory_bundle_for_update_check_(
          std::move(old_context->loader_factory_bundle_for_update_check_)),
      was_service_worker_registered_(
          old_context->was_service_worker_registered_),
      observer_list_(old_context->observer_list_),
      sync_observer_list_(old_context->sync_observer_list_),
      next_embedded_worker_id_(old_context->next_embedded_worker_id_),
      quota_client_(std::move(old_context->quota_client_)),
      quota_client_wrapper_(std::move(old_context->quota_client_wrapper_)),
      quota_client_receiver_(std::move(old_context->quota_client_receiver_)) {
  quota_client_->ResetContext(*this);
  service_worker_client_owner_->ResetContext(*this);

  // Uma (ServiceWorker.Storage.RegisteredStorageKeyCacheInitialization.Time)
  // shouldn't be recorded when ServiceWorkerContextCore is recreated. Hence we
  // specify a null TimeTicks here.
  registry_->GetRegisteredStorageKeys(
      base::BindOnce(&ServiceWorkerContextCore::DidGetRegisteredStorageKeys,
                     AsWeakPtr(), base::TimeTicks()));
}

ServiceWorkerContextCore::~ServiceWorkerContextCore() {
  DCHECK(registry_);
  for (const auto& it : live_versions_)
    it.second->RemoveObserver(this);

  job_coordinator_->AbortAll();
}

ServiceWorkerClientOwner::ServiceWorkerClientIterator
ServiceWorkerClientOwner::GetServiceWorkerClients(
    const blink::StorageKey& key,
    bool include_reserved_clients,
    bool include_back_forward_cached_clients) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return ServiceWorkerClientIterator(
      &service_worker_clients_by_uuid_,
      base::BindRepeating(IsSameOriginServiceWorkerClient, key,
                          include_reserved_clients,
                          include_back_forward_cached_clients));
}

ServiceWorkerClientOwner::ServiceWorkerClientIterator
ServiceWorkerClientOwner::GetWindowServiceWorkerClients(
    const blink::StorageKey& key,
    bool include_reserved_clients) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return ServiceWorkerClientIterator(
      &service_worker_clients_by_uuid_,
      base::BindRepeating(IsSameOriginWindowServiceWorkerClient, key,
                          include_reserved_clients));
}

void ServiceWorkerClientOwner::HasMainFrameWindowClient(
    const blink::StorageKey& key,
    BoolCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  bool has_main_frame = false;
  for (auto it =
           GetWindowServiceWorkerClients(key,
                                         /*include_reserved_clients=*/false);
       !it.IsAtEnd(); ++it) {
    DCHECK(it->IsContainerForWindowClient());
    auto* render_frame_host =
        RenderFrameHostImpl::FromID(it->GetRenderFrameHostId());
    if (render_frame_host && !render_frame_host->GetParent()) {
      has_main_frame = true;
      break;
    }
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), has_main_frame));
}

ScopedServiceWorkerClient
ServiceWorkerClientOwner::CreateServiceWorkerClientForWindow(
    bool are_ancestors_secure,
    FrameTreeNodeId frame_tree_node_id) {
  auto client = std::make_unique<ServiceWorkerClient>(
      context_->AsWeakPtr(), are_ancestors_secure, frame_tree_node_id);
  auto weak_client = client->AsWeakPtr();
  auto inserted = service_worker_clients_by_uuid_
                      .emplace(weak_client->client_uuid(), std::move(client))
                      .second;
  DCHECK(inserted);
  return ScopedServiceWorkerClient(std::move(weak_client));
}

ScopedServiceWorkerClient
ServiceWorkerClientOwner::CreateServiceWorkerClientForWorker(
    int process_id,
    ServiceWorkerClientInfo client_info) {
  auto client = std::make_unique<ServiceWorkerClient>(context_->AsWeakPtr(),
                                                      process_id, client_info);
  auto weak_client = client->AsWeakPtr();
  auto inserted = service_worker_clients_by_uuid_
                      .emplace(weak_client->client_uuid(), std::move(client))
                      .second;
  DCHECK(inserted);
  return ScopedServiceWorkerClient(std::move(weak_client));
}

void ServiceWorkerClientOwner::BindHost(
    ServiceWorkerContainerHostForClient& container_host,
    mojo::PendingAssociatedReceiver<blink::mojom::ServiceWorkerContainerHost>
        host_receiver) {
  container_host_receivers_->Add(&container_host, std::move(host_receiver),
                                 &container_host);
}

void ServiceWorkerClientOwner::UpdateServiceWorkerClientClientID(
    const std::string& current_client_uuid,
    const std::string& new_client_uuid) {
  auto it = service_worker_clients_by_uuid_.find(current_client_uuid);
  CHECK(it != service_worker_clients_by_uuid_.end(), base::NotFatalUntil::M130);
  std::unique_ptr<ServiceWorkerClient> service_worker_client =
      std::move(it->second);
  service_worker_clients_by_uuid_.erase(it);

  bool inserted =
      service_worker_clients_by_uuid_
          .emplace(new_client_uuid, std::move(service_worker_client))
          .second;
  DCHECK(inserted);
}

ServiceWorkerClient* ServiceWorkerClientOwner::GetServiceWorkerClientByClientID(
    const std::string& client_uuid) {
  auto it = service_worker_clients_by_uuid_.find(client_uuid);
  if (it == service_worker_clients_by_uuid_.end()) {
    return nullptr;
  }
  return it->second.get();
}

ServiceWorkerClient* ServiceWorkerClientOwner::GetServiceWorkerClientByWindowId(
    const base::UnguessableToken& window_id) {
  for (auto& it : service_worker_clients_by_uuid_) {
    if (it.second->fetch_request_window_id() == window_id)
      return it.second.get();
  }

  return nullptr;
}

void ServiceWorkerClientOwner::OnContainerHostReceiverDisconnected() {
  DestroyServiceWorkerClient(container_host_receivers_->current_context()
                                 ->service_worker_client()
                                 .AsWeakPtr());
}

void ServiceWorkerContextCore::OnClientDestroyed(
    ServiceWorkerClient& service_worker_client) {
  observer_list_->Notify(
      FROM_HERE, &ServiceWorkerContextCoreObserver::OnClientDestroyed,
      service_worker_client.container_host()
          ? service_worker_client.container_host()->ukm_source_id()
          : ukm::kInvalidSourceId,
      service_worker_client.url(), service_worker_client.GetClientType());
}

void ServiceWorkerClientOwner::DestroyServiceWorkerClient(
    base::WeakPtr<ServiceWorkerClient> service_worker_client) {
  if (!service_worker_client) {
    return;
  }

  context_->OnClientDestroyed(*service_worker_client);

  size_t removed = service_worker_clients_by_uuid_.erase(
      service_worker_client->client_uuid());
  CHECK_EQ(removed, 1u);
}

void ServiceWorkerContextCore::RegisterServiceWorker(
    const GURL& script_url,
    const blink::StorageKey& key,
    const blink::mojom::ServiceWorkerRegistrationOptions& options,
    blink::mojom::FetchClientSettingsObjectPtr
        outside_fetch_client_settings_object,
    RegistrationCallback callback,
    const GlobalRenderFrameHostId& requesting_frame_id,
    const PolicyContainerPolicies& policy_container_policies) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::string error_message;
  if (!IsValidRegisterRequest(script_url, options.scope, key, &error_message)) {
    std::move(callback).Run(
        blink::ServiceWorkerStatusCode::kErrorInvalidArguments, error_message,
        blink::mojom::kInvalidServiceWorkerRegistrationId);
    return;
  }

  auto* render_frame_host = RenderFrameHostImpl::FromID(requesting_frame_id);
  const blink::mojom::AncestorFrameType ancestor_frame_type =
      render_frame_host && render_frame_host->IsNestedWithinFencedFrame()
          ? blink::mojom::AncestorFrameType::kFencedFrame
          : blink::mojom::AncestorFrameType::kNormalFrame;

  was_service_worker_registered_ = true;
  job_coordinator_->Register(
      script_url, options, key, std::move(outside_fetch_client_settings_object),
      requesting_frame_id, ancestor_frame_type,
      base::BindOnce(&ServiceWorkerContextCore::RegistrationComplete,
                     AsWeakPtr(), options.scope, key, std::move(callback)),
      policy_container_policies);
}

void ServiceWorkerContextCore::UpdateServiceWorkerWithoutExecutionContext(
    ServiceWorkerRegistration* registration,
    bool force_bypass_cache) {
  // Use an empty fetch client settings object because this method is for
  // browser-initiated update and there is no associated execution context.
  UpdateServiceWorkerImpl(
      registration, force_bypass_cache, /*skip_script_comparison=*/false,
      blink::mojom::FetchClientSettingsObject::New(), base::NullCallback());
}

void ServiceWorkerContextCore::UpdateServiceWorker(
    ServiceWorkerRegistration* registration,
    bool force_bypass_cache,
    bool skip_script_comparison,
    blink::mojom::FetchClientSettingsObjectPtr
        outside_fetch_client_settings_object,
    UpdateCallback callback) {
  UpdateServiceWorkerImpl(
      registration, force_bypass_cache, skip_script_comparison,
      std::move(outside_fetch_client_settings_object), std::move(callback));
}

void ServiceWorkerContextCore::UnregisterServiceWorker(
    const GURL& scope,
    const blink::StorageKey& key,
    bool is_immediate,
    UnregistrationCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  BrowserContext* browser_context = wrapper_->browser_context();
  CHECK(browser_context);
  if (!GetContentClient()->browser()->MayDeleteServiceWorkerRegistration(
          scope, browser_context)) {
    std::move(callback).Run(blink::ServiceWorkerStatusCode::kErrorDisallowed);
    return;
  }

  job_coordinator_->Unregister(
      scope, key, is_immediate,
      base::BindOnce(&ServiceWorkerContextCore::UnregistrationComplete,
                     AsWeakPtr(), scope, key, std::move(callback)));
}

void ServiceWorkerContextCore::DeleteForStorageKey(const blink::StorageKey& key,
                                                   StatusCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  registry()->GetRegistrationsForStorageKey(
      key,
      base::BindOnce(
          &ServiceWorkerContextCore::DidGetRegistrationsForDeleteForStorageKey,
          AsWeakPtr(), key, std::move(callback)));
}

void ServiceWorkerContextCore::DidGetRegistrationsForDeleteForStorageKey(
    const blink::StorageKey& key,
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
          registry()->GetUninstallingRegistrationsForStorageKey(key);
  for (const auto& uninstalling_registration : uninstalling_registrations) {
    job_coordinator_->Abort(uninstalling_registration->scope(), key);
    uninstalling_registration->DeleteAndClearImmediately();
  }

  if (registrations.empty()) {
    std::move(callback).Run(blink::ServiceWorkerStatusCode::kOk);
    return;
  }

  // Ignore any registrations we are not permitted to delete.
  std::vector<scoped_refptr<ServiceWorkerRegistration>> filtered_registrations;
  ContentBrowserClient* browser_client = GetContentClient()->browser();
  BrowserContext* browser_context = wrapper_->browser_context();
  DCHECK(browser_context);
  for (auto registration : registrations) {
    if (browser_client->MayDeleteServiceWorkerRegistration(
            registration->scope(), browser_context)) {
      filtered_registrations.push_back(std::move(registration));
    }
  }

  int* expected_calls = new int(2 * filtered_registrations.size());
  auto* listeners =
      new std::vector<std::unique_ptr<RegistrationDeletionListener>>();

  // The barrier must be executed twice for each registration: once for
  // unregistration and once for deletion. It will call |callback| immediately
  // if an error occurs.
  base::RepeatingCallback<void(blink::ServiceWorkerStatusCode)> barrier =
      base::BindRepeating(SuccessReportingCallback, base::Owned(expected_calls),
                          base::Owned(listeners),
                          base::OwnedRef(std::move(callback)));
  for (const auto& registration : filtered_registrations) {
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
    job_coordinator_->Abort(registration->scope(), key);
    UnregisterServiceWorker(registration->scope(), key, /*is_immediate=*/true,
                            barrier);
  }
}

int ServiceWorkerContextCore::GetNextEmbeddedWorkerId() {
  return next_embedded_worker_id_++;
}

void ServiceWorkerContextCore::NotifyClientIsExecutionReady(
    const ServiceWorkerClient& service_worker_client) {
  CHECK(service_worker_client.is_execution_ready());
  observer_list_->Notify(
      FROM_HERE, &ServiceWorkerContextCoreObserver::OnClientIsExecutionReady,
      service_worker_client.container_host()->ukm_source_id(),
      service_worker_client.url(), service_worker_client.GetClientType());
}

bool ServiceWorkerContextCore::MaybeHasRegistrationForStorageKey(
    const blink::StorageKey& key) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!registrations_initialized_) {
    return true;
  }
  if (registered_storage_keys_.find(key) != registered_storage_keys_.end()) {
    return true;
  }
  return false;
}

void ServiceWorkerContextCore::WaitForRegistrationsInitializedForTest() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (registrations_initialized_)
    return;
  base::RunLoop loop;
  on_registrations_initialized_for_test_ = loop.QuitClosure();
  loop.Run();
}

void ServiceWorkerContextCore::AddWarmUpRequest(
    const GURL& document_url,
    const blink::StorageKey& key,
    ServiceWorkerContext::WarmUpServiceWorkerCallback callback) {
  // kSpeculativeServiceWorkerWarmUp enqueues navigation candidate URLs. This is
  // the queue length of the candidate URLs.
  static const size_t kRequestQueueLength =
      base::GetFieldTrialParamByFeatureAsInt(
          blink::features::kSpeculativeServiceWorkerWarmUp,
          "sw_warm_up_request_queue_length", 1000);

  // Erase redundant warm-up requests.
  std::vector<ServiceWorkerContext::WarmUpServiceWorkerCallback>
      callback_for_redundant_requests;
  std::erase_if(warm_up_requests_, [&](auto& it) {
    auto& [queued_url, _, queued_callback] = it;
    if (document_url == queued_url) {
      callback_for_redundant_requests.push_back(std::move(queued_callback));
      return true;
    } else {
      return false;
    }
  });
  for (auto& cb : callback_for_redundant_requests) {
    std::move(cb).Run();
  }

  warm_up_requests_.emplace_back(document_url, key, std::move(callback));

  while (warm_up_requests_.size() > kRequestQueueLength) {
    auto [front_url, front_key, front_callback] =
        std::move(warm_up_requests_.front());
    std::move(front_callback).Run();
    warm_up_requests_.pop_front();
  }
}

std::optional<ServiceWorkerContextCore::WarmUpRequest>
ServiceWorkerContextCore::PopNextWarmUpRequest() {
  DCHECK(!IsProcessingWarmingUp());

  if (warm_up_requests_.empty()) {
    return std::nullopt;
  }

  static const int kSpeculativeServiceWorkerWarmUpMaxCount =
      blink::features::kSpeculativeServiceWorkerWarmUpMaxCount.Get();
  if (GetWarmedUpServiceWorkerCount(live_versions_) >=
      kSpeculativeServiceWorkerWarmUpMaxCount) {
    warm_up_requests_.clear();
    return std::nullopt;
  }

  // Return the most recent queued request (LIFO order) to prioritize recently
  // added URLs. For example, the recent mouse-hoverd link will have a higher
  // chance to navigate than the previously mouse-hoverd link.
  std::optional<ServiceWorkerContextCore::WarmUpRequest> request(
      std::move(warm_up_requests_.back()));
  warm_up_requests_.pop_back();
  BeginProcessingWarmingUp();
  return request;
}

bool ServiceWorkerContextCore::IsWaitingForWarmUp(
    const blink::StorageKey& key) const {
  return std::find_if(
             warm_up_requests_.begin(), warm_up_requests_.end(), [&](auto& it) {
               const blink::StorageKey& warm_up_request_key = std::get<1>(it);
               return key == warm_up_request_key;
             }) != warm_up_requests_.end();
}

void ServiceWorkerContextCore::RegistrationComplete(
    const GURL& scope,
    const blink::StorageKey& key,
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
      registration->id(), scope, key);
}

void ServiceWorkerContextCore::UpdateServiceWorkerImpl(
    ServiceWorkerRegistration* registration,
    bool force_bypass_cache,
    bool skip_script_comparison,
    blink::mojom::FetchClientSettingsObjectPtr
        outside_fetch_client_settings_object,
    UpdateCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  BrowserContext* browser_context = wrapper_->browser_context();
  if (!browser_context) {
    // There is no associated browser context (this can happen while the context
    // is shutting down). Bail.
    return;
  }

  if (!GetContentClient()
           ->browser()
           ->ShouldTryToUpdateServiceWorkerRegistration(registration->scope(),
                                                        browser_context)) {
    return;
  }

  ServiceWorkerRegisterJob::RegistrationCallback callback_wrapper;
  if (callback) {
    callback_wrapper = base::BindOnce(&ServiceWorkerContextCore::UpdateComplete,
                                      AsWeakPtr(), std::move(callback));
  }
  job_coordinator_->Update(registration, force_bypass_cache,
                           skip_script_comparison,
                           std::move(outside_fetch_client_settings_object),
                           std::move(callback_wrapper));
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
    const blink::StorageKey& key,
    ServiceWorkerContextCore::UnregistrationCallback callback,
    int64_t registration_id,
    blink::ServiceWorkerStatusCode status) {
  std::move(callback).Run(status);
  if (status == blink::ServiceWorkerStatusCode::kOk) {
    observer_list_->Notify(
        FROM_HERE, &ServiceWorkerContextCoreObserver::OnRegistrationDeleted,
        registration_id, scope, key);
  }
}

bool ServiceWorkerContextCore::IsValidRegisterRequest(
    const GURL& script_url,
    const GURL& scope_url,
    const blink::StorageKey& key,
    std::string* out_error) const {
  if (!scope_url.is_valid() || !script_url.is_valid()) {
    *out_error = ServiceWorkerConsts::kBadMessageInvalidURL;
    return false;
  }
  if (blink::ServiceWorkerScopeOrScriptUrlContainsDisallowedCharacter(
          scope_url, script_url, out_error)) {
    return false;
  }
  std::vector<GURL> urls = {scope_url, script_url};

  if (key.origin().opaque())
    return false;

  urls.push_back(key.origin().GetURL());
  if (!service_worker_security_utils::AllOriginsMatchAndCanAccessServiceWorkers(
          urls)) {
    *out_error = ServiceWorkerConsts::kBadMessageImproperOrigins;
    return false;
  }
  return true;
}

scoped_refptr<ServiceWorkerRegistration>
ServiceWorkerContextCore::GetLiveRegistration(int64_t id) {
  auto it = live_registrations_.find(id);
  return (it != live_registrations_.end()) ? it->second.get() : nullptr;
}

void ServiceWorkerContextCore::AddLiveRegistration(
    ServiceWorkerRegistration* registration) {
  DCHECK(!GetLiveRegistration(registration->id()));
  live_registrations_[registration->id()] = registration;
  observer_list_->Notify(
      FROM_HERE, &ServiceWorkerContextCoreObserver::OnNewLiveRegistration,
      registration->id(), registration->scope(), registration->key());
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
  // TODO(crbug.com/335613089): Determine why we see these crashes. Once
  // resolved change DCHECK.
  CHECK(!GetLiveVersion(version->version_id()));
  live_versions_[version->version_id()] = version;
  version->AddObserver(this);
  ServiceWorkerVersionInfo version_info = version->GetInfo();
  observer_list_->Notify(FROM_HERE,
                         &ServiceWorkerContextCoreObserver::OnNewLiveVersion,
                         version_info);
  for (auto& observer : test_version_observers_) {
    observer.OnServiceWorkerVersionCreated(version);
  }
}

void ServiceWorkerContextCore::RemoveLiveVersion(int64_t id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto it = live_versions_.find(id);
  CHECK(it != live_versions_.end(), base::NotFatalUntil::M130);
  ServiceWorkerVersion* version = it->second;

  if (version->running_status() != blink::EmbeddedWorkerStatus::kStopped) {
    // Notify all observers that this live version is stopped, as it will
    // be removed from |live_versions_|.
    observer_list_->Notify(FROM_HERE,
                           &ServiceWorkerContextCoreObserver::OnStopped, id);
    for (auto& observer : sync_observer_list_->observers) {
      const std::optional<ServiceWorkerRunningInfo> running_info =
          wrapper_->GetRunningServiceWorkerInfo(id);
      if (running_info.has_value()) {
        observer.OnStopped(id, /*worker_info=*/running_info.value());
      }
    }
  }

  // Send any final reports and allow the reporting configuration to be
  // removed.
  if (wrapper_->storage_partition()) {
    wrapper_->storage_partition()
        ->GetNetworkContext()
        ->SendReportsAndRemoveSource(version->reporting_source());
  }

  observer_list_->Notify(
      FROM_HERE, &ServiceWorkerContextCoreObserver::OnLiveVersionDestroyed, id);

  live_versions_.erase(it);
}

std::vector<ServiceWorkerRegistrationInfo>
ServiceWorkerContextCore::GetAllLiveRegistrationInfo() {
  std::vector<ServiceWorkerRegistrationInfo> infos;
  for (std::map<int64_t, raw_ptr<ServiceWorkerRegistration, CtnExperimental>>::
           const_iterator iter = live_registrations_.begin();
       iter != live_registrations_.end(); ++iter) {
    infos.push_back(iter->second->GetInfo());
  }
  return infos;
}

std::vector<ServiceWorkerVersionInfo>
ServiceWorkerContextCore::GetAllLiveVersionInfo() {
  std::vector<ServiceWorkerVersionInfo> infos;
  for (std::map<int64_t,
                raw_ptr<ServiceWorkerVersion, CtnExperimental>>::const_iterator
           iter = live_versions_.begin();
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
  registry()->PrepareForDeleteAndStartOver();
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&ServiceWorkerContextWrapper::DeleteAndStartOver,
                     wrapper_));
}

void ServiceWorkerContextCore::DeleteAndStartOver(StatusCallback callback) {
  job_coordinator_->AbortAll();

  observer_list_->Notify(
      FROM_HERE, &ServiceWorkerContextCoreObserver::OnDeleteAndStartOver);
  for (const auto& live_version_itr : live_versions_) {
    ServiceWorkerVersion* live_version = live_version_itr.second;
    for (auto& observer : sync_observer_list_->observers) {
      const std::optional<ServiceWorkerRunningInfo> running_info =
          wrapper_->GetRunningServiceWorkerInfo(live_version->version_id());
      if (running_info.has_value()) {
        observer.OnStopped(live_version->version_id(),
                           /*worker_info=*/running_info.value());
      }
    }
  }

  registry()->DeleteAndStartOver(std::move(callback));
}

void ServiceWorkerContextCore::ClearAllServiceWorkersForTest(
    base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
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
    const blink::StorageKey& key,
    ServiceWorkerContext::CheckHasServiceWorkerCallback callback) {
  registry()->FindRegistrationForClientUrl(
      ServiceWorkerRegistry::Purpose::kNotForNavigation, url, key,
      base::BindOnce(&ServiceWorkerContextCore::
                         DidFindRegistrationForCheckHasServiceWorker,
                     AsWeakPtr(), std::move(callback)));
}

void ServiceWorkerContextCore::UpdateVersionFailureCount(
    int64_t version_id,
    blink::ServiceWorkerStatusCode status) {
  // Don't count these, they aren't start worker failures.
  if (status == blink::ServiceWorkerStatusCode::kErrorDisallowed)
    return;

  auto it = failure_counts_.find(version_id);
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

void ServiceWorkerContextCore::NotifyRegistrationStored(
    int64_t registration_id,
    const GURL& scope,
    const blink::StorageKey& key) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  registered_storage_keys_.insert(key);
  observer_list_->Notify(
      FROM_HERE, &ServiceWorkerContextCoreObserver::OnRegistrationStored,
      registration_id, scope, key);
}

void ServiceWorkerContextCore::NotifyAllRegistrationsDeletedForStorageKey(
    const blink::StorageKey& key) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  registered_storage_keys_.erase(key);
  observer_list_->Notify(
      FROM_HERE,
      &ServiceWorkerContextCoreObserver::OnAllRegistrationsDeletedForStorageKey,
      key);
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

void ServiceWorkerContextCore::OnWindowOpened(const GURL& script_url,
                                              const GURL& url) {
  observer_list_->Notify(FROM_HERE,
                         &ServiceWorkerContextCoreObserver::OnWindowOpened,
                         script_url, url);
}

void ServiceWorkerContextCore::OnClientNavigated(const GURL& script_url,
                                                 const GURL& url) {
  observer_list_->Notify(FROM_HERE,
                         &ServiceWorkerContextCoreObserver::OnClientNavigated,
                         script_url, url);
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

  scoped_refptr<ServiceWorkerRegistration> registration =
      GetLiveRegistration(version->registration_id());
  if (registration)
    registration->OnNoControllees(version);

  observer_list_->Notify(
      FROM_HERE, &ServiceWorkerContextCoreObserver::OnNoControllees,
      version->version_id(), version->scope(), version->key());
}

void ServiceWorkerContextCore::OnControlleeNavigationCommitted(
    ServiceWorkerVersion* version,
    const std::string& client_uuid,
    GlobalRenderFrameHostId render_frame_host_id) {
  DCHECK_EQ(this, version->context().get());

  observer_list_->Notify(
      FROM_HERE,
      &ServiceWorkerContextCoreObserver::OnControlleeNavigationCommitted,
      version->version_id(), client_uuid, render_frame_host_id);
}

void ServiceWorkerContextCore::OnRunningStateChanged(
    ServiceWorkerVersion* version) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(this, version->context().get());
  switch (version->running_status()) {
    case blink::EmbeddedWorkerStatus::kStopped:
      observer_list_->Notify(FROM_HERE,
                             &ServiceWorkerContextCoreObserver::OnStopped,
                             version->version_id());
      for (auto& observer : sync_observer_list_->observers) {
        const std::optional<ServiceWorkerRunningInfo> running_info =
            wrapper_->GetRunningServiceWorkerInfo(version->version_id());
        if (running_info.has_value()) {
          observer.OnStopped(version->version_id(),
                             /*worker_info=*/running_info.value());
        }
      }
      break;
    case blink::EmbeddedWorkerStatus::kStarting:
      observer_list_->Notify(FROM_HERE,
                             &ServiceWorkerContextCoreObserver::OnStarting,
                             version->version_id());
      break;
    case blink::EmbeddedWorkerStatus::kRunning:
      observer_list_->Notify(
          FROM_HERE, &ServiceWorkerContextCoreObserver::OnStarted,
          version->version_id(), version->scope(),
          version->embedded_worker()->process_id(), version->script_url(),
          version->worker_host()->token(), version->key());
      break;
    case blink::EmbeddedWorkerStatus::kStopping:
      observer_list_->Notify(FROM_HERE,
                             &ServiceWorkerContextCoreObserver::OnStopping,
                             version->version_id());
      break;
  }
}

void ServiceWorkerContextCore::OnVersionStateChanged(
    ServiceWorkerVersion* version) {
  DCHECK_EQ(this, version->context().get());
  if (version->status() == ServiceWorkerVersion::INSTALLED &&
      version->router_evaluator()) {
    observer_list_->Notify(
        FROM_HERE,
        &ServiceWorkerContextCoreObserver::OnVersionRouterRulesChanged,
        version->version_id(), version->router_evaluator()->ToString());
  }
  observer_list_->Notify(
      FROM_HERE, &ServiceWorkerContextCoreObserver::OnVersionStateChanged,
      version->version_id(), version->scope(), version->key(),
      version->status());
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
    const std::u16string& error_message,
    int line_number,
    int column_number,
    const GURL& source_url) {
  DCHECK_EQ(this, version->context().get());
  observer_list_->Notify(
      FROM_HERE, &ServiceWorkerContextCoreObserver::OnErrorReported,
      version->version_id(), version->scope(), version->key(),
      ServiceWorkerContextObserver::ErrorInfo(error_message, line_number,
                                              column_number, source_url));
}

void ServiceWorkerContextCore::OnReportConsoleMessage(
    ServiceWorkerVersion* version,
    blink::mojom::ConsoleMessageSource source,
    blink::mojom::ConsoleMessageLevel message_level,
    const std::u16string& message,
    int line_number,
    const GURL& source_url) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserContext* browser_context = wrapper_->browser_context();
  DCHECK(browser_context);
  DCHECK_EQ(this, version->context().get());
  const bool is_builtin_component =
      HasWebUIScheme(source_url) ||
      GetContentClient()->browser()->IsBuiltinComponent(
          browser_context, url::Origin::Create(source_url));

  LogConsoleMessage(message_level, message, line_number, is_builtin_component,
                    wrapper_->is_incognito(),
                    base::UTF8ToUTF16(source_url.spec()));

  observer_list_->Notify(
      FROM_HERE, &ServiceWorkerContextCoreObserver::OnReportConsoleMessage,
      version->version_id(), version->scope(), version->key(),
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

void ServiceWorkerContextCore::DidGetRegisteredStorageKeys(
    base::TimeTicks start_time,
    const std::vector<blink::StorageKey>& storage_keys) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  for (const blink::StorageKey& storage_key : storage_keys)
    registered_storage_keys_.insert(storage_key);

  DCHECK(!registrations_initialized_);
  registrations_initialized_ = true;

  if (on_registrations_initialized_for_test_)
    std::move(on_registrations_initialized_for_test_).Run();

  if (!start_time.is_null()) {
    base::UmaHistogramMediumTimes(
        "ServiceWorker.Storage.RegisteredStorageKeyCacheInitialization.Time",
        base::TimeTicks::Now() - start_time);
  }
}

ScopedServiceWorkerClient::ScopedServiceWorkerClient(
    base::WeakPtr<ServiceWorkerClient> service_worker_client)
    : service_worker_client_(std::move(service_worker_client)) {}

ScopedServiceWorkerClient::~ScopedServiceWorkerClient() {
  if (!service_worker_client_) {
    return;
  }

  // Don't destroy the client if committed, because it means this is already
  // `Release()`d.
  if (service_worker_client_->is_response_committed()) {
    return;
  }

  service_worker_client_->owner().DestroyServiceWorkerClient(
      std::move(service_worker_client_));
}

ScopedServiceWorkerClient::ScopedServiceWorkerClient(
    ScopedServiceWorkerClient&& other) = default;

std::tuple<blink::mojom::ServiceWorkerContainerInfoForClientPtr,
           blink::mojom::ControllerServiceWorkerInfoPtr>
ScopedServiceWorkerClient::CommitResponseAndRelease(
    std::optional<GlobalRenderFrameHostId> rfh_id,
    const PolicyContainerPolicies& policy_container_policies,
    mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
        coep_reporter,
    ukm::SourceId ukm_source_id) {
  if (!service_worker_client_) {
    return {};
  }

  blink::mojom::ServiceWorkerContainerInfoForClientPtr container_info =
      service_worker_client_->CommitResponse(
          base::PassKey<ScopedServiceWorkerClient>(), std::move(rfh_id),
          policy_container_policies, std::move(coep_reporter),
          std::move(ukm_source_id));

  blink::mojom::ControllerServiceWorkerInfoPtr controller;
  if (service_worker_client_->controller()) {
    controller = service_worker_client_->container_host()
                     ->CreateControllerServiceWorkerInfo();
  }
  return std::make_tuple(std::move(container_info), std::move(controller));
}

#if !BUILDFLAG(IS_ANDROID)
ServiceWorkerHidDelegateObserver*
ServiceWorkerContextCore::hid_delegate_observer() {
  if (!hid_delegate_observer_) {
    hid_delegate_observer_ =
        std::make_unique<ServiceWorkerHidDelegateObserver>(this);
  }
  return hid_delegate_observer_.get();
}

void ServiceWorkerContextCore::SetServiceWorkerHidDelegateObserverForTesting(
    std::unique_ptr<ServiceWorkerHidDelegateObserver> hid_delegate_observer) {
  hid_delegate_observer_ = std::move(hid_delegate_observer);
}

ServiceWorkerUsbDelegateObserver*
ServiceWorkerContextCore::usb_delegate_observer() {
  if (!usb_delegate_observer_) {
    usb_delegate_observer_ =
        std::make_unique<ServiceWorkerUsbDelegateObserver>(this);
  }
  return usb_delegate_observer_.get();
}

void ServiceWorkerContextCore::SetServiceWorkerUsbDelegateObserverForTesting(
    std::unique_ptr<ServiceWorkerUsbDelegateObserver> usb_delegate_observer) {
  usb_delegate_observer_ = std::move(usb_delegate_observer);
}
#endif  // !BUILDFLAG(IS_ANDROID)
}  // namespace content
