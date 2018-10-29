// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTEXT_WRAPPER_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTEXT_WRAPPER_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/observer_list_threadsafe.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_core_observer.h"
#include "content/common/content_export.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/service_worker_context.h"

namespace base {
class FilePath;
}

namespace storage {
class QuotaManagerProxy;
class SpecialStoragePolicy;
}

namespace url {
class Origin;
}  // namespace url

namespace content {

class BrowserContext;
class ChromeBlobStorageContext;
class ResourceContext;
class ServiceWorkerContextObserver;
class StoragePartitionImpl;
class URLLoaderFactoryGetter;

// A refcounted wrapper class for ServiceWorkerContextCore. Higher level content
// lib classes keep references to this class on multiple threads. The inner core
// instance is strictly single threaded and is not refcounted. The core object
// is what is used internally by service worker classes.
class CONTENT_EXPORT ServiceWorkerContextWrapper
    : public ServiceWorkerContext,
      public ServiceWorkerContextCoreObserver,
      public base::RefCountedThreadSafe<ServiceWorkerContextWrapper> {
 public:
  using StatusCallback =
      base::OnceCallback<void(blink::ServiceWorkerStatusCode)>;
  using BoolCallback = base::OnceCallback<void(bool)>;
  using FindRegistrationCallback =
      ServiceWorkerStorage::FindRegistrationCallback;
  using GetRegistrationsCallback =
      ServiceWorkerStorage::GetRegistrationsCallback;
  using GetRegistrationsInfosCallback =
      ServiceWorkerStorage::GetRegistrationsInfosCallback;
  using GetUserDataCallback = ServiceWorkerStorage::GetUserDataCallback;
  using GetUserKeysAndDataCallback =
      ServiceWorkerStorage::GetUserKeysAndDataCallback;
  using GetUserDataForAllRegistrationsCallback =
      ServiceWorkerStorage::GetUserDataForAllRegistrationsCallback;

  explicit ServiceWorkerContextWrapper(BrowserContext* browser_context);

  // Init and Shutdown are for use on the UI thread when the profile,
  // storagepartition is being setup and torn down.
  // |blob_context| and |url_loader_factory_getter| are used only
  // when IsServicificationEnabled is true.
  void Init(const base::FilePath& user_data_directory,
            storage::QuotaManagerProxy* quota_manager_proxy,
            storage::SpecialStoragePolicy* special_storage_policy,
            ChromeBlobStorageContext* blob_context,
            URLLoaderFactoryGetter* url_loader_factory_getter);
  void Shutdown();

  // Must be called on the IO thread.
  void InitializeResourceContext(ResourceContext* resource_context);

  // Deletes all files on disk and restarts the system asynchronously. This
  // leaves the system in a disabled state until it's done. This should be
  // called on the IO thread.
  void DeleteAndStartOver();

  // The StoragePartition should only be used on the UI thread.
  // Can be null before/during init and during/after shutdown (and in tests).
  StoragePartitionImpl* storage_partition() const;

  void set_storage_partition(StoragePartitionImpl* storage_partition);

  // The ResourceContext for the associated BrowserContext. This should only
  // be accessed on the IO thread, and can be null during initialization and
  // shutdown.
  ResourceContext* resource_context();

  // The process manager can be used on either UI or IO.
  ServiceWorkerProcessManager* process_manager() {
    return process_manager_.get();
  }

  // ServiceWorkerContextCoreObserver implementation:
  void OnRegistrationCompleted(int64_t registration_id,
                               const GURL& scope) override;
  void OnNoControllees(int64_t version_id, const GURL& scope) override;
  void OnVersionStateChanged(int64_t version_id,
                             const GURL& scope,
                             ServiceWorkerVersion::Status status) override;

  // ServiceWorkerContext implementation:
  void AddObserver(ServiceWorkerContextObserver* observer) override;
  void RemoveObserver(ServiceWorkerContextObserver* observer) override;
  void RegisterServiceWorker(
      const GURL& script_url,
      const blink::mojom::ServiceWorkerRegistrationOptions& options,
      ResultCallback callback) override;
  void UnregisterServiceWorker(const GURL& scope,
                               ResultCallback callback) override;
  bool StartingExternalRequest(int64_t service_worker_version_id,
                               const std::string& request_uuid) override;
  bool FinishedExternalRequest(int64_t service_worker_version_id,
                               const std::string& request_uuid) override;
  void CountExternalRequestsForTest(
      const GURL& url,
      CountExternalRequestsCallback callback) override;
  void GetAllOriginsInfo(GetUsageInfoCallback callback) override;
  void DeleteForOrigin(const GURL& origin, ResultCallback callback) override;
  void CheckHasServiceWorker(const GURL& url,
                             const GURL& other_url,
                             CheckHasServiceWorkerCallback callback) override;
  void ClearAllServiceWorkersForTest(base::OnceClosure callback) override;
  void StartWorkerForScope(const GURL& scope,
                           StartWorkerCallback info_callback,
                           base::OnceClosure failure_callback) override;
  void StartServiceWorkerAndDispatchLongRunningMessage(
      const GURL& scope,
      blink::TransferableMessage message,
      ResultCallback result_callback) override;
  void StartServiceWorkerForNavigationHint(
      const GURL& document_url,
      StartServiceWorkerForNavigationHintCallback callback) override;
  void StopAllServiceWorkersForOrigin(const GURL& origin) override;
  void StopAllServiceWorkers(base::OnceClosure callback) override;

  // These methods must only be called from the IO thread.
  ServiceWorkerRegistration* GetLiveRegistration(int64_t registration_id);
  ServiceWorkerVersion* GetLiveVersion(int64_t version_id);
  std::vector<ServiceWorkerRegistrationInfo> GetAllLiveRegistrationInfo();
  std::vector<ServiceWorkerVersionInfo> GetAllLiveVersionInfo();

  // Must be called from the IO thread.
  void HasMainFrameProviderHost(const GURL& origin,
                                BoolCallback callback) const;

  // Returns all frame ids for the given |origin|.
  std::unique_ptr<std::vector<GlobalFrameRoutingId>> GetProviderHostIds(
      const GURL& origin) const;

  // Returns the registration whose scope longest matches |document_url|. It is
  // guaranteed that the returned registration has the activated worker.
  //
  //  - If the registration is not found, returns ERROR_NOT_FOUND.
  //  - If the registration has neither the waiting version nor the active
  //    version, returns ERROR_NOT_FOUND.
  //  - If the registration does not have the active version but has the waiting
  //    version, activates the waiting version and runs |callback| when it is
  //    activated.
  //
  // Must be called from the IO thread.
  void FindReadyRegistrationForDocument(const GURL& document_url,
                                        FindRegistrationCallback callback);

  // Returns the registration for |scope|. It is guaranteed that the returned
  // registration has the activated worker.
  //
  //  - If the registration is not found, returns ERROR_NOT_FOUND.
  //  - If the registration has neither the waiting version nor the active
  //    version, returns ERROR_NOT_FOUND.
  //  - If the registration does not have the active version but has the waiting
  //    version, activates the waiting version and runs |callback| when it is
  //    activated.
  //
  // Must be called from the IO thread.
  void FindReadyRegistrationForScope(const GURL& scope,
                                     FindRegistrationCallback callback);

  // Similar to FindReadyRegistrationForScope, but in the case no waiting or
  // active worker is found (i.e., there is only an installing worker),
  // |callback| is called without waiting for the worker to reach active.
  void FindRegistrationForScope(const GURL& scope,
                                FindRegistrationCallback callback);

  // Returns the registration for |registration_id|. It is guaranteed that the
  // returned registration has the activated worker.
  //
  //  - If the registration is not found, returns ERROR_NOT_FOUND.
  //  - If the registration has neither the waiting version nor the active
  //    version, returns ERROR_NOT_FOUND.
  //  - If the registration does not have the active version but has the waiting
  //    version, activates the waiting version and runs |callback| when it is
  //    activated.
  //
  // Must be called from the IO thread.
  void FindReadyRegistrationForId(int64_t registration_id,
                                  const GURL& origin,
                                  FindRegistrationCallback callback);

  // Returns the registration for |registration_id|. It is guaranteed that the
  // returned registration has the activated worker.
  //
  // Generally |FindReadyRegistrationForId| should be used to look up a
  // registration by |registration_id| since it's more efficient. But if a
  // |registration_id| is all that is available this method can be used instead.
  //
  //  - If the registration is not found, returns ERROR_NOT_FOUND.
  //  - If the registration has neither the waiting version nor the active
  //    version, returns ERROR_NOT_FOUND.
  //  - If the registration does not have the active version but has the waiting
  //    version, activates the waiting version and runs |callback| when it is
  //    activated.
  //
  // Must be called from the IO thread.
  void FindReadyRegistrationForIdOnly(int64_t registration_id,
                                      FindRegistrationCallback callback);

  // All these methods must be called from the IO thread.
  void GetAllRegistrations(GetRegistrationsInfosCallback callback);
  void GetRegistrationsForOrigin(const url::Origin& origin,
                                 GetRegistrationsCallback callback);
  void GetRegistrationUserData(int64_t registration_id,
                               const std::vector<std::string>& keys,
                               GetUserDataCallback callback);
  void GetRegistrationUserDataByKeyPrefix(int64_t registration_id,
                                          const std::string& key_prefix,
                                          GetUserDataCallback callback);
  void GetRegistrationUserKeysAndDataByKeyPrefix(
      int64_t registration_id,
      const std::string& key_prefix,
      GetUserKeysAndDataCallback callback);
  void StoreRegistrationUserData(
      int64_t registration_id,
      const GURL& origin,
      const std::vector<std::pair<std::string, std::string>>& key_value_pairs,
      StatusCallback callback);
  void ClearRegistrationUserData(int64_t registration_id,
                                 const std::vector<std::string>& keys,
                                 StatusCallback callback);
  void ClearRegistrationUserDataByKeyPrefixes(
      int64_t registration_id,
      const std::vector<std::string>& key_prefixes,
      StatusCallback callback);
  void GetUserDataForAllRegistrations(
      const std::string& key,
      GetUserDataForAllRegistrationsCallback callback);
  void GetUserDataForAllRegistrationsByKeyPrefix(
      const std::string& key_prefix,
      GetUserDataForAllRegistrationsCallback callback);

  // This function can be called from any thread, but the callback will always
  // be called on the UI thread.
  void StartServiceWorker(const GURL& scope, StatusCallback callback);

  // These methods can be called from any thread.
  void SkipWaitingWorker(const GURL& scope);
  void UpdateRegistration(const GURL& scope);
  void SetForceUpdateOnPageLoad(bool force_update_on_page_load);
  // Different from AddObserver/RemoveObserver(ServiceWorkerContextObserver*).
  // But we must keep the same name, or else base::ScopedObserver breaks.
  void AddObserver(ServiceWorkerContextCoreObserver* observer);
  void RemoveObserver(ServiceWorkerContextCoreObserver* observer);

  bool is_incognito() const { return is_incognito_; }

  // S13nServiceWorker:
  // Used for starting a shared worker. Returns a provider host for the shared
  // worker and fills |out_provider_info| with info to send to the renderer to
  // connect to the host. The host stays alive as long as this info stays alive
  // (namely, as long as |out_provider_info->host_ptr_info| stays alive).
  //
  // Must be called on the IO thread.
  base::WeakPtr<ServiceWorkerProviderHost> PreCreateHostForSharedWorker(
      int process_id,
      mojom::ServiceWorkerProviderInfoForSharedWorkerPtr* out_provider_info);

 private:
  friend class BackgroundSyncManagerTest;
  friend class base::RefCountedThreadSafe<ServiceWorkerContextWrapper>;
  friend class EmbeddedWorkerTestHelper;
  friend class EmbeddedWorkerBrowserTest;
  friend class FakeServiceWorkerContextWrapper;
  friend class ServiceWorkerDispatcherHost;
  friend class ServiceWorkerInternalsUI;
  friend class ServiceWorkerNavigationHandleCore;
  friend class ServiceWorkerProcessManager;
  friend class ServiceWorkerRequestHandler;
  friend class ServiceWorkerVersionBrowserTest;

  ~ServiceWorkerContextWrapper() override;

  void InitInternal(
      const base::FilePath& user_data_directory,
      scoped_refptr<base::SequencedTaskRunner> database_task_runner,
      storage::QuotaManagerProxy* quota_manager_proxy,
      storage::SpecialStoragePolicy* special_storage_policy,
      ChromeBlobStorageContext* blob_context,
      URLLoaderFactoryGetter* url_loader_factory_getter);
  void ShutdownOnIO();

  // If |include_installing_version| is true, |callback| is called if there is
  // an installing version with no waiting or active version.
  void FindRegistrationForScopeImpl(const GURL& scope,
                                    bool include_installing_version,
                                    FindRegistrationCallback callback);

  void DidFindRegistrationForFindReady(
      FindRegistrationCallback callback,
      blink::ServiceWorkerStatusCode status,
      scoped_refptr<ServiceWorkerRegistration> registration);
  void DidFindRegistrationForFindImpl(
      bool include_installing_version,
      FindRegistrationCallback callback,
      blink::ServiceWorkerStatusCode status,
      scoped_refptr<ServiceWorkerRegistration> registration);
  void OnStatusChangedForFindReadyRegistration(
      FindRegistrationCallback callback,
      scoped_refptr<ServiceWorkerRegistration> registration);

  void DidDeleteAndStartOver(blink::ServiceWorkerStatusCode status);

  void DidGetAllRegistrationsForGetAllOrigins(
      GetUsageInfoCallback callback,
      blink::ServiceWorkerStatusCode status,
      const std::vector<ServiceWorkerRegistrationInfo>& registrations);

  void DidCheckHasServiceWorker(CheckHasServiceWorkerCallback callback,
                                content::ServiceWorkerCapability status);

  void DidFindRegistrationForUpdate(
      blink::ServiceWorkerStatusCode status,
      scoped_refptr<content::ServiceWorkerRegistration> registration);

  void CountExternalRequests(const GURL& url,
                             CountExternalRequestsCallback callback);

  void StartServiceWorkerForNavigationHintOnIO(
      const GURL& document_url,
      StartServiceWorkerForNavigationHintCallback callback);

  void DidFindRegistrationForNavigationHint(
      StartServiceWorkerForNavigationHintCallback callback,
      blink::ServiceWorkerStatusCode status,
      scoped_refptr<ServiceWorkerRegistration> registration);

  void DidStartServiceWorkerForNavigationHint(
      const GURL& scope,
      StartServiceWorkerForNavigationHintCallback callback,
      blink::ServiceWorkerStatusCode code);

  void RecordStartServiceWorkerForNavigationHintResult(
      StartServiceWorkerForNavigationHintCallback callback,
      StartServiceWorkerForNavigationHintResult result);

  void StopAllServiceWorkersOnIO(
      base::OnceClosure callback,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner_for_callback);

  void DidFindRegistrationForLongRunningMessage(
      blink::TransferableMessage message,
      const GURL& source_origin,
      ResultCallback result_callback,
      blink::ServiceWorkerStatusCode service_worker_status,
      scoped_refptr<ServiceWorkerRegistration> registration);

  void DidStartServiceWorkerForLongRunningMessage(
      blink::TransferableMessage message,
      const GURL& source_origin,
      scoped_refptr<ServiceWorkerRegistration> registration,
      ServiceWorkerContext::ResultCallback result_callback,
      blink::ServiceWorkerStatusCode service_worker_status);

  void SendActiveWorkerMessage(
      blink::TransferableMessage message,
      const GURL& source_origin,
      ServiceWorkerContext::ResultCallback result_callback,
      blink::ServiceWorkerStatusCode status,
      scoped_refptr<ServiceWorkerRegistration> registration);

  // The core context is only for use on the IO thread.
  // Can be null before/during init, during/after shutdown, and after
  // DeleteAndStartOver fails.
  ServiceWorkerContextCore* context();

  // Observers of |context_core_| which live within content's implementation
  // boundary. Shared with |context_core_|.
  using ServiceWorkerContextObserverList =
      base::ObserverListThreadSafe<ServiceWorkerContextCoreObserver>;
  const scoped_refptr<ServiceWorkerContextObserverList> core_observer_list_;

  // Observers which live outside content's implementation boundary. Observer
  // methods will always be dispatched on the UI thread.
  base::ObserverList<ServiceWorkerContextObserver>::Unchecked observer_list_;

  const std::unique_ptr<ServiceWorkerProcessManager> process_manager_;
  // Cleared in ShutdownOnIO():
  std::unique_ptr<ServiceWorkerContextCore> context_core_;

  // Initialized in Init(); true if the user data directory is empty.
  bool is_incognito_ = false;

  // Raw pointer to the StoragePartitionImpl owning |this|.
  StoragePartitionImpl* storage_partition_ = nullptr;

  // The ResourceContext associated with this context.
  ResourceContext* resource_context_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerContextWrapper);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTEXT_WRAPPER_H_
