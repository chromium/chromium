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

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/observer_list_threadsafe.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_core_observer.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/service_worker_running_info.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

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
class ServiceWorkerContextWatcher;
class StoragePartitionImpl;
class URLLoaderFactoryGetter;

// A refcounted wrapper class for ServiceWorkerContextCore. Higher level content
// lib classes keep references to this class on multiple threads. The inner core
// instance is strictly single threaded (this is called the "core thread") and
// is not refcounted. The core object is what is used internally by service
// worker classes.
class CONTENT_EXPORT ServiceWorkerContextWrapper
    : public ServiceWorkerContext,
      public ServiceWorkerContextCoreObserver,
      public base::RefCountedThreadSafe<ServiceWorkerContextWrapper,
                                        BrowserThread::DeleteOnUIThread> {
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

  static void RunOrPostTaskOnCoreThread(const base::Location& location,
                                        base::OnceClosure task);

  // Init and Shutdown are for use on the UI thread when the profile,
  // storagepartition is being setup and torn down.
  void Init(const base::FilePath& user_data_directory,
            storage::QuotaManagerProxy* quota_manager_proxy,
            storage::SpecialStoragePolicy* special_storage_policy,
            ChromeBlobStorageContext* blob_context,
            URLLoaderFactoryGetter* url_loader_factory_getter);
  void Shutdown();

  // Non-ServiceWorkerOnUI only. Must be called on the IO thread.
  void InitializeResourceContext(ResourceContext* resource_context);

  // Deletes all files on disk and restarts the system asynchronously. This
  // leaves the system in a disabled state until it's done. This should be
  // called on the core thread.
  void DeleteAndStartOver();

  // The StoragePartition should only be used on the UI thread.
  // Can be null before/during init and during/after shutdown (and in tests).
  StoragePartitionImpl* storage_partition() const;

  void set_storage_partition(StoragePartitionImpl* storage_partition);

  // UI thread.
  BrowserContext* browser_context();

  // Non-ServiceWorkerOnUI only. The ResourceContext for the associated
  // BrowserContext. This should only be accessed on the IO thread, and can be
  // null during initialization and shutdown.
  ResourceContext* resource_context();

  // The process manager can be used on either UI or IO.
  ServiceWorkerProcessManager* process_manager() {
    return process_manager_.get();
  }

  // ServiceWorkerContextCoreObserver implementation:
  void OnRegistrationCompleted(int64_t registration_id,
                               const GURL& scope) override;
  void OnRegistrationStored(int64_t registration_id,
                            const GURL& scope) override;
  void OnReportConsoleMessage(int64_t version_id,
                              const ConsoleMessage& message) override;
  void OnNoControllees(int64_t version_id, const GURL& scope) override;
  void OnStarted(int64_t version_id,
                 const GURL& scope,
                 int process_id,
                 const GURL& script_url) override;
  void OnStopped(int64_t version_id) override;
  void OnDeleteAndStartOver() override;
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
  ServiceWorkerExternalRequestResult StartingExternalRequest(
      int64_t service_worker_version_id,
      const std::string& request_uuid) override;
  ServiceWorkerExternalRequestResult FinishedExternalRequest(
      int64_t service_worker_version_id,
      const std::string& request_uuid) override;
  void CountExternalRequestsForTest(
      const GURL& url,
      CountExternalRequestsCallback callback) override;
  void GetAllOriginsInfo(GetUsageInfoCallback callback) override;
  void DeleteForOrigin(const GURL& origin, ResultCallback callback) override;
  void PerformStorageCleanup(base::OnceClosure callback) override;
  void CheckHasServiceWorker(const GURL& url,
                             CheckHasServiceWorkerCallback callback) override;
  void ClearAllServiceWorkersForTest(base::OnceClosure callback) override;
  void StartWorkerForScope(const GURL& scope,
                           StartWorkerCallback info_callback,
                           base::OnceClosure failure_callback) override;
  void StartServiceWorkerAndDispatchMessage(
      const GURL& scope,
      blink::TransferableMessage message,
      ResultCallback result_callback) override;
  void StartServiceWorkerForNavigationHint(
      const GURL& document_url,
      StartServiceWorkerForNavigationHintCallback callback) override;
  void StopAllServiceWorkersForOrigin(const GURL& origin) override;
  void StopAllServiceWorkers(base::OnceClosure callback) override;
  const base::flat_map<int64_t, ServiceWorkerRunningInfo>&
  GetRunningServiceWorkerInfos() override;

  // These methods must only be called from the core thread.
  ServiceWorkerRegistration* GetLiveRegistration(int64_t registration_id);
  ServiceWorkerVersion* GetLiveVersion(int64_t version_id);
  std::vector<ServiceWorkerRegistrationInfo> GetAllLiveRegistrationInfo();
  std::vector<ServiceWorkerVersionInfo> GetAllLiveVersionInfo();

  // May be called from any thread, and the callback is called on that thread.
  void HasMainFrameProviderHost(const GURL& origin,
                                BoolCallback callback) const;

  // Returns all frame ids for the given |origin|. Must be called on the core
  // thread.
  std::unique_ptr<std::vector<GlobalFrameRoutingId>> GetProviderHostIds(
      const GURL& origin) const;

  // Returns the registration whose scope longest matches |client_url|. It is
  // guaranteed that the returned registration has the activated worker.
  //
  //  - If the registration is not found, returns ERROR_NOT_FOUND.
  //  - If the registration has neither the waiting version nor the active
  //    version, returns ERROR_NOT_FOUND.
  //  - If the registration does not have the active version but has the waiting
  //    version, activates the waiting version and runs |callback| when it is
  //    activated.
  //
  // Can be called on any thread, and the callback is called on that thread.
  void FindReadyRegistrationForClientUrl(const GURL& client_url,
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
  // Can be called from any thread, and the callback is called on that thread.
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
  // Can be called from any thread, and the callback is called on that thread.
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
  // Can be called from any thread, and the callback is called on that thread.
  void FindReadyRegistrationForIdOnly(int64_t registration_id,
                                      FindRegistrationCallback callback);

  // These can be called from any thread, and the callback is called on that
  // thread.
  void GetAllRegistrations(GetRegistrationsInfosCallback callback);
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
  void ClearUserDataForAllRegistrationsByKeyPrefix(
      const std::string& key_prefix,
      StatusCallback callback);

  // Must be called on the core thread, and the callback is called on that
  // thread. This restriction is because the callback gets pointers to live
  // registrations, which live on the core thread.
  void GetRegistrationsForOrigin(const url::Origin& origin,
                                 GetRegistrationsCallback callback);

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

  // The core context is only for use on the IO thread.
  // Can be null before/during init, during/after shutdown, and after
  // DeleteAndStartOver fails.
  ServiceWorkerContextCore* context();

  // Whether |origin| has any registrations. Must be called on UI thread.
  bool HasRegistrationForOrigin(const GURL& origin) const;
  void WaitForRegistrationsInitializedForTest();

  // This must be called on the core thread, and the |callback| also runs on
  // the core thread which can be called with nullptr on failure.
  void GetLoaderFactoryForUpdateCheck(
      const GURL& scope,
      base::OnceCallback<void(scoped_refptr<network::SharedURLLoaderFactory>)>
          callback);

 private:
  friend class BackgroundSyncManagerTest;
  friend class base::DeleteHelper<ServiceWorkerContextWrapper>;
  friend class EmbeddedWorkerBrowserTest;
  friend class EmbeddedWorkerTestHelper;
  friend class FakeServiceWorkerContextWrapper;
  friend class ServiceWorkerClientsApiBrowserTest;
  friend class ServiceWorkerInternalsUI;
  friend class ServiceWorkerNavigationHandleCore;
  friend class ServiceWorkerProcessManager;
  friend class ServiceWorkerRequestHandler;
  friend class ServiceWorkerVersionBrowserTest;
  friend struct BrowserThread::DeleteOnThread<BrowserThread::UI>;

  ~ServiceWorkerContextWrapper() override;

  void InitOnCoreThread(
      const base::FilePath& user_data_directory,
      scoped_refptr<base::SequencedTaskRunner> database_task_runner,
      storage::QuotaManagerProxy* quota_manager_proxy,
      storage::SpecialStoragePolicy* special_storage_policy,
      ChromeBlobStorageContext* blob_context,
      URLLoaderFactoryGetter* url_loader_factory_getter,
      std::unique_ptr<blink::URLLoaderFactoryBundleInfo>
          non_network_loader_factory_bundle_info_for_update_check);
  void ShutdownOnCoreThread();

  // If |include_installing_version| is true, |callback| is called if there is
  // an installing version with no waiting or active version.
  void FindRegistrationForScopeImpl(const GURL& scope,
                                    bool include_installing_version,
                                    FindRegistrationCallback callback);

  void DidFindRegistrationForFindReady(
      FindRegistrationCallback callback,
      scoped_refptr<base::TaskRunner> callback_runner,
      blink::ServiceWorkerStatusCode status,
      scoped_refptr<ServiceWorkerRegistration> registration);
  void DidFindRegistrationForFindImpl(
      bool include_installing_version,
      FindRegistrationCallback callback,
      scoped_refptr<base::TaskRunner> callback_runner,
      blink::ServiceWorkerStatusCode status,
      scoped_refptr<ServiceWorkerRegistration> registration);
  void OnStatusChangedForFindReadyRegistration(
      FindRegistrationCallback callback,
      scoped_refptr<base::TaskRunner> callback_runner,
      scoped_refptr<ServiceWorkerRegistration> registration);

  void DidDeleteAndStartOver(blink::ServiceWorkerStatusCode status);

  void DidGetAllRegistrationsForGetAllOrigins(
      GetUsageInfoCallback callback,
      scoped_refptr<base::TaskRunner> callback_runner,
      blink::ServiceWorkerStatusCode status,
      const std::vector<ServiceWorkerRegistrationInfo>& registrations);

  void DidCheckHasServiceWorker(CheckHasServiceWorkerCallback callback,
                                content::ServiceWorkerCapability status);

  void DidFindRegistrationForUpdate(
      blink::ServiceWorkerStatusCode status,
      scoped_refptr<content::ServiceWorkerRegistration> registration);

  void CountExternalRequests(const GURL& url,
                             CountExternalRequestsCallback callback);

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

  void DidFindRegistrationForMessageDispatch(
      blink::TransferableMessage message,
      const GURL& source_origin,
      ResultCallback result_callback,
      scoped_refptr<base::TaskRunner> callback_runner,
      blink::ServiceWorkerStatusCode service_worker_status,
      scoped_refptr<ServiceWorkerRegistration> registration);

  void DidStartServiceWorkerForMessageDispatch(
      blink::TransferableMessage message,
      const GURL& source_origin,
      scoped_refptr<ServiceWorkerRegistration> registration,
      ServiceWorkerContext::ResultCallback result_callback,
      scoped_refptr<base::TaskRunner> callback_runner,
      blink::ServiceWorkerStatusCode service_worker_status);

  // Called when ServiceWorkerImportedScriptUpdateCheck is enabled.
  std::unique_ptr<blink::URLLoaderFactoryBundleInfo>
  CreateNonNetworkURLLoaderFactoryBundleInfoForUpdateCheck(
      BrowserContext* browser_context);

  void SetUpLoaderFactoryForUpdateCheckOnUI(
      const GURL& scope,
      base::OnceCallback<void(scoped_refptr<network::SharedURLLoaderFactory>)>
          callback);

  // This method completes the remaining work of
  // SetUpLoaderFactoryForUpdateCheckOnUI() on Core thread: Binds the pending
  // network factory receiver and creates the loader factory bundle for update
  // check.
  void DidSetUpLoaderFactoryForUpdateCheck(
      mojo::PendingRemote<network::mojom::URLLoaderFactory> remote,
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> pending_receiver,
      bool bypass_redirect_checks,
      base::OnceCallback<void(scoped_refptr<network::SharedURLLoaderFactory>)>
          callback);

  // Called when the stored registrations are loaded, and each time a new
  // service worker is registered.
  void OnRegistrationUpdated(
      const std::vector<ServiceWorkerRegistrationInfo>& registrations);

  // Temporary for crbug.com/824858.
  void GetAllOriginsInfoOnCoreThread(
      GetUsageInfoCallback callback,
      scoped_refptr<base::TaskRunner> callback_runner);
  void PerformStorageCleanupOnCoreThread(
      base::OnceClosure callback,
      scoped_refptr<base::TaskRunner> callback_runner);
  void StartServiceWorkerAndDispatchMessageOnCoreThread(
      const GURL& scope,
      blink::TransferableMessage message,
      ResultCallback result_callback,
      scoped_refptr<base::TaskRunner> callback_runner);
  void FindReadyRegistrationForIdOnCoreThread(
      int64_t registration_id,
      const GURL& origin,
      FindRegistrationCallback callback,
      scoped_refptr<base::TaskRunner> callback_runner);
  void FindReadyRegistrationForIdOnlyOnCoreThread(
      int64_t registration_id,
      FindRegistrationCallback callback,
      scoped_refptr<base::TaskRunner> callback_runner);
  void DeleteForOriginOnCoreThread(
      const GURL& origin,
      ResultCallback callback,
      scoped_refptr<base::TaskRunner> callback_runner);
  void FindRegistrationForScopeOnCoreThread(
      const GURL& scope,
      bool include_installing_version,
      FindRegistrationCallback callback,
      scoped_refptr<base::TaskRunner> callback_runner);
  void HasMainFrameProviderHostOnCoreThread(
      const GURL& origin,
      BoolCallback callback,
      scoped_refptr<base::TaskRunner> callback_runner) const;
  void FindReadyRegistrationForClientUrlOnCoreThread(
      const GURL& client_url,
      FindRegistrationCallback callback,
      scoped_refptr<base::TaskRunner> callback_runner);
  void GetAllRegistrationsOnCoreThread(GetRegistrationsInfosCallback callback);
  void GetRegistrationUserDataOnCoreThread(int64_t registration_id,
                                           const std::vector<std::string>& keys,
                                           GetUserDataCallback callback);
  void GetRegistrationUserDataByKeyPrefixOnCoreThread(
      int64_t registration_id,
      const std::string& key_prefix,
      GetUserDataCallback callback);
  void GetRegistrationUserKeysAndDataByKeyPrefixOnCoreThread(
      int64_t registration_id,
      const std::string& key_prefix,
      GetUserKeysAndDataCallback callback);
  void StoreRegistrationUserDataOnCoreThread(
      int64_t registration_id,
      const GURL& origin,
      const std::vector<std::pair<std::string, std::string>>& key_value_pairs,
      StatusCallback callback);
  void ClearRegistrationUserDataOnCoreThread(
      int64_t registration_id,
      const std::vector<std::string>& keys,
      StatusCallback callback);
  void ClearRegistrationUserDataByKeyPrefixesOnCoreThread(
      int64_t registration_id,
      const std::vector<std::string>& key_prefixes,
      StatusCallback callback);
  void GetUserDataForAllRegistrationsOnCoreThread(
      const std::string& key,
      GetUserDataForAllRegistrationsCallback callback);
  void GetUserDataForAllRegistrationsByKeyPrefixOnCoreThread(
      const std::string& key_prefix,
      GetUserDataForAllRegistrationsCallback callback);
  void ClearUserDataForAllRegistrationsByKeyPrefixOnCoreThread(
      const std::string& key_prefix,
      StatusCallback callback);
  void StartServiceWorkerForNavigationHintOnCoreThread(
      const GURL& document_url,
      StartServiceWorkerForNavigationHintCallback callback);
  void StopAllServiceWorkersOnCoreThread(
      base::OnceClosure callback,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner_for_callback);

  // Observers of |context_core_| which live within content's implementation
  // boundary. Shared with |context_core_|.
  using ServiceWorkerContextObserverList =
      base::ObserverListThreadSafe<ServiceWorkerContextCoreObserver>;
  const scoped_refptr<ServiceWorkerContextObserverList> core_observer_list_;

  // Observers which live outside content's implementation boundary. Observer
  // methods will always be dispatched on the UI thread.
  base::ObserverList<ServiceWorkerContextObserver, true>::Unchecked
      observer_list_;

  const std::unique_ptr<ServiceWorkerProcessManager> process_manager_;
  // Cleared in ShutdownOnCoreThread():
  std::unique_ptr<ServiceWorkerContextCore> context_core_;

  // Initialized in Init(); true if the user data directory is empty.
  bool is_incognito_ = false;

  // Raw pointer to the StoragePartitionImpl owning |this|.
  StoragePartitionImpl* storage_partition_ = nullptr;

  // Non-ServiceWorkerOnUI only. The ResourceContext associated with this
  // context.
  ResourceContext* resource_context_ = nullptr;

  // Map that contains all service workers that are considered "running". Used
  // to dispatch OnVersionStartedRunning()/OnVersionStoppedRunning() events.
  base::flat_map<int64_t /* version_id */, ServiceWorkerRunningInfo>
      running_service_workers_;

  // Maps the origin to a set of registration ids for that origin. Must be
  // accessed on UI thread.
  // TODO(http://crbug.com/824858): This can be removed when service workers are
  // fully converted to running on the UI thread.
  base::flat_map<GURL, base::flat_set<int64_t>> registrations_for_origin_;
  bool registrations_initialized_ = false;
  base::OnceClosure on_registrations_initialized_;

  scoped_refptr<ServiceWorkerContextWatcher> watcher_;

  // Temporary for moving context core to the UI thread.
  scoped_refptr<base::TaskRunner> core_thread_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerContextWrapper);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTEXT_WRAPPER_H_
