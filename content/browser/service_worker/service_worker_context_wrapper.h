// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTEXT_WRAPPER_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTEXT_WRAPPER_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/observer_list_threadsafe.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_core_observer.h"
#include "content/browser/service_worker/service_worker_identifiability_metrics.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/service_worker_running_info.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/mojom/client_security_state.mojom-forward.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace base {
class FilePath;
}  // namespace base

namespace storage {
class QuotaManagerProxy;
class ServiceWorkerStorageControlImpl;
class SpecialStoragePolicy;
}  // namespace storage

namespace url {
class Origin;
}  // namespace url

namespace content {

class BrowserContext;
class ChromeBlobStorageContext;
class ServiceWorkerContextObserver;
class StoragePartitionImpl;

// A ref-counted wrapper struct around an ObserverList. This is needed because
// the ObserverList is shared implicitly between the ServiceWorkerContextCore
// and the ServiceWorkerContextWrapper.
struct ServiceWorkerContextSynchronousObserverList
    : public base::RefCounted<ServiceWorkerContextSynchronousObserverList> {
 public:
  REQUIRE_ADOPTION_FOR_REFCOUNTED_TYPE();

  ServiceWorkerContextSynchronousObserverList();

  base::ObserverList<ServiceWorkerContextObserverSynchronous> observers;

 private:
  friend class base::RefCounted<ServiceWorkerContextSynchronousObserverList>;
  ~ServiceWorkerContextSynchronousObserverList();
};

// A refcounted wrapper class for ServiceWorkerContextCore. Higher level content
// lib classes keep references to this class on multiple threads. The inner core
// instance is strictly single threaded (on the UI thread) and is not
// refcounted. The core object is what is used internally by service worker
// classes.
//
// All the methods called on the UI thread.
// TODO(crbug.com/40738640): Require all references to be on the UI
// thread and remove RefCountedThreadSafe.
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
      ServiceWorkerRegistry::FindRegistrationCallback;
  using GetRegistrationsCallback =
      ServiceWorkerRegistry::GetRegistrationsCallback;
  using GetRegistrationsInfosCallback =
      ServiceWorkerRegistry::GetRegistrationsInfosCallback;
  using GetUserDataCallback = ServiceWorkerRegistry::GetUserDataCallback;
  using GetUserKeysAndDataCallback =
      ServiceWorkerRegistry::GetUserKeysAndDataCallback;
  using GetUserDataForAllRegistrationsCallback =
      ServiceWorkerRegistry::GetUserDataForAllRegistrationsCallback;
  using GetInstalledRegistrationOriginsCallback =
      base::OnceCallback<void(const std::vector<url::Origin>& origins)>;

  explicit ServiceWorkerContextWrapper(BrowserContext* browser_context);

  ServiceWorkerContextWrapper(const ServiceWorkerContextWrapper&) = delete;
  ServiceWorkerContextWrapper& operator=(const ServiceWorkerContextWrapper&) =
      delete;

  // Init and Shutdown called when the StoragePartition is being setup and torn
  // down.
  void Init(const base::FilePath& user_data_directory,
            storage::QuotaManagerProxy* quota_manager_proxy,
            storage::SpecialStoragePolicy* special_storage_policy,
            ChromeBlobStorageContext* blob_context);
  void Shutdown();

  // Deletes all files on disk and restarts the system asynchronously. This
  // leaves the system in a disabled state until it's done.
  void DeleteAndStartOver();

  // Can be null before/during init and during/after shutdown (and in tests).
  StoragePartitionImpl* storage_partition() const;

  void set_storage_partition(StoragePartitionImpl* storage_partition);

  BrowserContext* browser_context();

  ServiceWorkerProcessManager* process_manager() {
    return process_manager_.get();
  }

  // ServiceWorkerContextCoreObserver implementation:
  void OnRegistrationCompleted(int64_t registration_id,
                               const GURL& scope,
                               const blink::StorageKey& key) override;
  void OnRegistrationStored(int64_t registration_id,
                            const GURL& scope,
                            const blink::StorageKey& key) override;
  void OnAllRegistrationsDeletedForStorageKey(
      const blink::StorageKey& key) override;
  void OnErrorReported(
      int64_t version_id,
      const GURL& scope,
      const blink::StorageKey& key,
      const ServiceWorkerContextObserver::ErrorInfo& info) override;
  void OnReportConsoleMessage(int64_t version_id,
                              const GURL& scope,
                              const blink::StorageKey& key,
                              const ConsoleMessage& message) override;
  void OnControlleeAdded(int64_t version_id,
                         const std::string& uuid,
                         const ServiceWorkerClientInfo& info) override;
  void OnControlleeRemoved(int64_t version_id,
                           const std::string& uuid) override;
  void OnNoControllees(int64_t version_id,
                       const GURL& scope,
                       const blink::StorageKey& key) override;
  void OnControlleeNavigationCommitted(
      int64_t version_id,
      const std::string& uuid,
      GlobalRenderFrameHostId render_frame_host_id) override;
  void OnStarting(int64_t version_id) override;
  void OnStarted(int64_t version_id,
                 const GURL& scope,
                 int process_id,
                 const GURL& script_url,
                 const blink::ServiceWorkerToken& token,
                 const blink::StorageKey& key) override;
  void OnStopping(int64_t version_id) override;
  void OnStopped(int64_t version_id) override;
  void OnDeleteAndStartOver() override;
  void OnVersionStateChanged(int64_t version_id,
                             const GURL& scope,
                             const blink::StorageKey& key,
                             ServiceWorkerVersion::Status status) override;
  void OnWindowOpened(const GURL& script_url, const GURL& url) override;
  void OnClientNavigated(const GURL& script_url, const GURL& url) override;

  // ServiceWorkerContext implementation:
  void AddObserver(ServiceWorkerContextObserver* observer) override;
  void RemoveObserver(ServiceWorkerContextObserver* observer) override;
  void AddSyncObserver(
      ServiceWorkerContextObserverSynchronous* observer) override;
  void RemoveSyncObserver(
      ServiceWorkerContextObserverSynchronous* observer) override;
  // TODO (crbug.com/1335059) RegisterServiceWorker passes an invalid frame id.
  // Currently it's okay because it is used only by PaymentAppInstaller and
  // Extensions, but ideally we should add some guard to avoid the method is
  // called from other places.
  void RegisterServiceWorker(
      const GURL& script_url,
      const blink::StorageKey& key,
      const blink::mojom::ServiceWorkerRegistrationOptions& options,
      StatusCodeCallback callback) override;
  void UnregisterServiceWorker(const GURL& scope,
                               const blink::StorageKey& key,
                               StatusCodeCallback callback) override;
  void UnregisterServiceWorkerImmediately(const GURL& scope,
                                          const blink::StorageKey& key,
                                          StatusCodeCallback callback) override;
  ServiceWorkerExternalRequestResult StartingExternalRequest(
      int64_t service_worker_version_id,
      ServiceWorkerExternalRequestTimeoutType timeout_type,
      const base::Uuid& request_uuid) override;
  ServiceWorkerExternalRequestResult FinishedExternalRequest(
      int64_t service_worker_version_id,
      const base::Uuid& request_uuid) override;
  size_t CountExternalRequestsForTest(const blink::StorageKey& key) override;
  bool ExecuteScriptForTest(
      const std::string& script,
      int64_t service_worker_version_id,
      ServiceWorkerScriptExecutionCallback callback) override;
  bool MaybeHasRegistrationForStorageKey(const blink::StorageKey& key) override;
  void GetAllStorageKeysInfo(GetUsageInfoCallback callback) override;
  void DeleteForStorageKey(const blink::StorageKey& key,
                           ResultCallback callback) override;
  void CheckHasServiceWorker(const GURL& url,
                             const blink::StorageKey& key,
                             CheckHasServiceWorkerCallback callback) override;

  void ClearAllServiceWorkersForTest(base::OnceClosure callback) override;
  void StartWorkerForScope(const GURL& scope,
                           const blink::StorageKey& key,
                           StartWorkerCallback info_callback,
                           StatusCodeCallback failure_callback) override;
  void StartServiceWorkerAndDispatchMessage(
      const GURL& scope,
      const blink::StorageKey& key,
      blink::TransferableMessage message,
      ResultCallback result_callback) override;
  void StartServiceWorkerForNavigationHint(
      const GURL& document_url,
      const blink::StorageKey& key,
      StartServiceWorkerForNavigationHintCallback callback) override;
  void WarmUpServiceWorker(const GURL& document_url,
                           const blink::StorageKey& key,
                           WarmUpServiceWorkerCallback callback) override;
  void StopAllServiceWorkersForStorageKey(
      const blink::StorageKey& key) override;
  void StopAllServiceWorkers(base::OnceClosure callback) override;
  const base::flat_map<int64_t, ServiceWorkerRunningInfo>&
  GetRunningServiceWorkerInfos() override;
  bool IsLiveStartingServiceWorker(int64_t service_worker_version_id) override;
  bool IsLiveRunningServiceWorker(int64_t service_worker_version_id) override;
  service_manager::InterfaceProvider& GetRemoteInterfaces(
      int64_t service_worker_version_id) override;
  blink::AssociatedInterfaceProvider& GetRemoteAssociatedInterfaces(
      int64_t service_worker_version_id) override;

  // Returns the running info for a worker with `version_id`, if found.
  std::optional<ServiceWorkerRunningInfo> GetRunningServiceWorkerInfo(
      int64_t version_id);

  scoped_refptr<ServiceWorkerRegistration> GetLiveRegistration(
      int64_t registration_id);
  ServiceWorkerVersion* GetLiveVersion(int64_t version_id);
  std::vector<ServiceWorkerRegistrationInfo> GetAllLiveRegistrationInfo();
  std::vector<ServiceWorkerVersionInfo> GetAllLiveVersionInfo();

  void HasMainFrameWindowClient(const blink::StorageKey& key,
                                BoolCallback callback) const;

  // Returns all frame routing ids for the given `key`.
  std::unique_ptr<std::vector<GlobalRenderFrameHostId>>
  GetWindowClientFrameRoutingIds(const blink::StorageKey& key) const;

  // Returns the registration whose scope longest matches `client_url` with the
  // associated `key`. It is guaranteed that the returned registration has the
  // activated worker.
  //
  //  - If the registration is not found, returns ERROR_NOT_FOUND.
  //  - If the registration has neither the waiting version nor the active
  //    version, returns ERROR_NOT_FOUND.
  //  - If the registration does not have the active version but has the waiting
  //    version, activates the waiting version and runs `callback` when it is
  //    activated.
  //
  // There is no guarantee for whether the callback is called synchronously or
  // asynchronously.
  void FindReadyRegistrationForClientUrl(const GURL& client_url,
                                         const blink::StorageKey& key,
                                         FindRegistrationCallback callback);

  // Returns the registration for `scope` with the associated `key`. It is
  // guaranteed that the returned registration has the activated worker.
  //
  //  - If the registration is not found, returns ERROR_NOT_FOUND.
  //  - If the registration has neither the waiting version nor the active
  //    version, returns ERROR_NOT_FOUND.
  //  - If the registration does not have the active version but has the waiting
  //    version, activates the waiting version and runs `callback` when it is
  //    activated.
  //
  // There is no guarantee for whether the callback is called synchronously or
  // asynchronously.
  void FindReadyRegistrationForScope(const GURL& scope,
                                     const blink::StorageKey& key,
                                     FindRegistrationCallback callback);

  // Similar to FindReadyRegistrationForScope, but in the case no waiting or
  // active worker is found (i.e., there is only an installing worker),
  // `callback` is called without waiting for the worker to reach active.
  //
  // Not guaranteed to call the callback asynchronously.
  // TODO(falken): Should it?
  void FindRegistrationForScope(const GURL& scope,
                                const blink::StorageKey& key,
                                FindRegistrationCallback callback);

  // Returns the registration for `registration_id`. It is guaranteed that the
  // returned registration has the activated worker.
  //
  //  - If the registration is not found, returns ERROR_NOT_FOUND.
  //  - If the registration has neither the waiting version nor the active
  //    version, returns ERROR_NOT_FOUND.
  //  - If the registration does not have the active version but has the waiting
  //    version, activates the waiting version and runs `callback` when it is
  //    activated.
  //
  // There is no guarantee about whether the callback is called asynchronously
  // or synchronously.
  void FindReadyRegistrationForId(int64_t registration_id,
                                  const blink::StorageKey& key,
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
  // There is no guarantee about whether the callback is called synchronously or
  // asynchronously.
  void FindReadyRegistrationForIdOnly(int64_t registration_id,
                                      FindRegistrationCallback callback);

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
      const blink::StorageKey& key,
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

  // Returns a list of ServiceWorkerRegistration for `key`. The list includes
  // stored registrations and installing (not stored yet) registrations.
  void GetRegistrationsForStorageKey(const blink::StorageKey& key,
                                     GetRegistrationsCallback callback);

  // Fails with kErrorNotFound if there is no active registration for the given
  // `scope` and `key`. It means that there is no registration at all or that
  // the registration doesn't have an active version yet (which is the case for
  // installing service workers).
  void StartActiveServiceWorker(const GURL& scope,
                                const blink::StorageKey& key,
                                StatusCallback callback);

  void SkipWaitingWorker(const GURL& scope, const blink::StorageKey& key);
  void UpdateRegistration(const GURL& scope, const blink::StorageKey& key);
  void SetForceUpdateOnPageLoad(bool force_update_on_page_load);

  // Different from AddObserver/RemoveObserver(ServiceWorkerContextObserver*).
  // But we must keep the same name, or else base::ScopedObservation breaks.
  void AddObserver(ServiceWorkerContextCoreObserver* observer);
  void RemoveObserver(ServiceWorkerContextCoreObserver* observer);

  // Notifies only synchronous observer
  // `ServiceWorkerContextObserverSynchronous` of all running workers stopped.
  void NotifyRunningServiceWorkerStoppedToSynchronousObserver();

  bool is_incognito() const { return is_incognito_; }

  // Can be null before/during init, during/after shutdown, and after
  // DeleteAndStartOver fails.
  ServiceWorkerContextCore* context();

  void SetLoaderFactoryForUpdateCheckForTest(
      scoped_refptr<network::SharedURLLoaderFactory> loader_factory);
  // Returns nullptr on failure.
  scoped_refptr<network::SharedURLLoaderFactory> GetLoaderFactoryForUpdateCheck(
      const GURL& scope,
      network::mojom::ClientSecurityStatePtr client_security_state);

  // Returns nullptr on failure.
  // Note: This is currently only used for plzServiceWorker.
  scoped_refptr<network::SharedURLLoaderFactory>
  GetLoaderFactoryForMainScriptFetch(
      const GURL& scope,
      int64_t version_id,
      network::mojom::ClientSecurityStatePtr client_security_state);

  // Binds a ServiceWorkerStorageControl.
  void BindStorageControl(
      mojo::PendingReceiver<storage::mojom::ServiceWorkerStorageControl>
          receiver);

  using StorageControlBinder = base::RepeatingCallback<void(
      mojo::PendingReceiver<storage::mojom::ServiceWorkerStorageControl>)>;
  // Sets a callback to bind ServiceWorkerStorageControl for testing.
  void SetStorageControlBinderForTest(StorageControlBinder binder);

  ServiceWorkerContextCore* GetContextCoreForTest() {
    return context_core_.get();
  }

  void SetForceUpdateOnPageLoadForTesting(
      bool force_update_on_page_load) override;

 private:
  friend class BackgroundSyncManagerTest;
  friend class base::DeleteHelper<ServiceWorkerContextWrapper>;
  friend class EmbeddedWorkerBrowserTest;
  friend class EmbeddedWorkerTestHelper;
  friend class FakeServiceWorkerContextWrapper;
  friend class ServiceWorkerClientsApiBrowserTest;
  friend class ServiceWorkerInternalsUI;
  friend class ServiceWorkerMainResourceHandle;
  friend class ServiceWorkerProcessManager;
  friend class ServiceWorkerVersionBrowserTest;
  friend struct BrowserThread::DeleteOnThread<BrowserThread::UI>;

  ~ServiceWorkerContextWrapper() override;

  // Init() with a custom database task runner and BrowserContext. Explicitly
  // called from EmbeddedWorkerTestHelper.
  void InitInternal(
      storage::QuotaManagerProxy* quota_manager_proxy,
      storage::SpecialStoragePolicy* special_storage_policy,
      ChromeBlobStorageContext* blob_context,
      BrowserContext* browser_context);

  // If `include_installing_version` is true, `callback` is called if there is
  // an installing version with no waiting or active version.
  //
  // Not guaranteed to call the callback asynchronously.
  // TODO(falken): Should it?
  void FindRegistrationForScopeImpl(const GURL& scope,
                                    const blink::StorageKey& key,
                                    bool include_installing_version,
                                    FindRegistrationCallback callback);

  // Helper methods for `UnregisterServiceWorker()` and
  // `UnregisterServiceWorkerImmediately()`. `callback` provides the status that
  // was encountered. `blink::ServiceWorkerStatusCode::kOk` means the request to
  // unregister was sent. It does not mean the worker has been fully
  // unregistered though.
  void UnregisterServiceWorkerImpl(const GURL& scope,
                                   const blink::StorageKey& key,
                                   StatusCodeCallback callback);
  void UnregisterServiceWorkerImmediatelyImpl(const GURL& scope,
                                              const blink::StorageKey& key,
                                              StatusCodeCallback callback);

  void MaybeProcessPendingWarmUpRequest();

  void DidFindRegistrationForFindImpl(
      bool include_installing_version,
      FindRegistrationCallback callback,
      blink::ServiceWorkerStatusCode status,
      scoped_refptr<ServiceWorkerRegistration> registration);
  void OnStatusChangedForFindReadyRegistration(
      FindRegistrationCallback callback,
      scoped_refptr<ServiceWorkerRegistration> registration);

  void DidDeleteAndStartOver(blink::ServiceWorkerStatusCode status);

  void DidGetAllRegistrationsForGetAllStorageKeys(
      GetUsageInfoCallback callback,
      blink::ServiceWorkerStatusCode status,
      const std::vector<ServiceWorkerRegistrationInfo>& registrations);

  void DidFindRegistrationForUpdate(
      blink::ServiceWorkerStatusCode status,
      scoped_refptr<content::ServiceWorkerRegistration> registration);

  void DidFindRegistrationForNavigationHint(
      StartServiceWorkerForNavigationHintCallback callback,
      blink::ServiceWorkerStatusCode status,
      scoped_refptr<ServiceWorkerRegistration> registration);

  void DidFindRegistrationForWarmUp(
      WarmUpServiceWorkerCallback callback,
      blink::ServiceWorkerStatusCode status,
      scoped_refptr<ServiceWorkerRegistration> registration);

  void DidStartServiceWorkerForNavigationHint(
      const GURL& scope,
      StartServiceWorkerForNavigationHintCallback callback,
      blink::ServiceWorkerStatusCode code);

  void DidWarmUpServiceWorker(const GURL& scope,
                              WarmUpServiceWorkerCallback callback,
                              blink::ServiceWorkerStatusCode code);

  void DidFindRegistrationForMessageDispatch(
      blink::TransferableMessage message,
      const GURL& source_origin,
      ResultCallback result_callback,
      blink::ServiceWorkerStatusCode service_worker_status,
      scoped_refptr<ServiceWorkerRegistration> registration);

  void DidStartServiceWorkerForMessageDispatch(
      blink::TransferableMessage message,
      const GURL& source_origin,
      scoped_refptr<ServiceWorkerRegistration> registration,
      ServiceWorkerContext::ResultCallback result_callback,
      blink::ServiceWorkerStatusCode service_worker_status);

  std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
  CreateNonNetworkPendingURLLoaderFactoryBundleForUpdateCheck(
      BrowserContext* browser_context);

  // TODO(crbug.com/40820909): Remove. Temporary workaround.
  void StartServiceWorkerAndDispatchMessageOnUIThread(
      const GURL& scope,
      const blink::StorageKey& key,
      blink::TransferableMessage message,
      ResultCallback callback);

  // Clears running workers and notifies `ServiceWorkerContextObservers` of
  // worker stop.
  void ClearRunningServiceWorkers();

  scoped_refptr<network::SharedURLLoaderFactory>
  GetLoaderFactoryForBrowserInitiatedRequest(
      const GURL& scope,
      std::optional<int64_t> version_id,
      network::mojom::ClientSecurityStatePtr client_security_state);

  // Observers of `context_core_` which live within content's implementation
  // boundary. Shared with `context_core_`.
  using ServiceWorkerContextObserverList =
      base::ObserverListThreadSafe<ServiceWorkerContextCoreObserver>;
  const scoped_refptr<ServiceWorkerContextObserverList> core_observer_list_;
  // Observers of `context_core_`, but actually a subset of
  // `ServiceWorkerContextObserver`. Shared with `context_core_`.
  const scoped_refptr<ServiceWorkerContextSynchronousObserverList>
      core_sync_observer_list_;

  // Observers which live outside content's implementation boundary.
  base::ObserverList<ServiceWorkerContextObserver, true>::Unchecked
      observer_list_;

  // `browser_context_` is maintained to be valid within the lifetime of the
  // browser context.
  raw_ptr<BrowserContext, DanglingUntriaged> browser_context_;

  const std::unique_ptr<ServiceWorkerProcessManager> process_manager_;
  std::unique_ptr<ServiceWorkerContextCore> context_core_;

  // Initialized in Init(); true if the user data directory is empty.
  bool is_incognito_ = false;

  // Indicates if we are in the middle of deleting the `context_core_` in
  // order to start over.
  bool is_deleting_and_starting_over_ = false;

  // Raw pointer to the StoragePartitionImpl owning |this|.
  raw_ptr<StoragePartitionImpl, DanglingUntriaged> storage_partition_ = nullptr;

  // Map that contains all service workers that are considered "running". Used
  // to dispatch OnVersionStartedRunning()/OnVersionStoppedRunning() events.
  base::flat_map<int64_t /* version_id */, ServiceWorkerRunningInfo>
      running_service_workers_;

  std::unique_ptr<ServiceWorkerIdentifiabilityMetrics> identifiability_metrics_;

  // TODO(crbug.com/40120038): Remove `storage_control_` when
  // ServiceWorkerStorage is sandboxed. An instance of this impl should live in
  // the storage service, not here.
  std::unique_ptr<storage::ServiceWorkerStorageControlImpl> storage_control_;
  // These fields are used to (re)create `storage_control_`.
  base::FilePath user_data_directory_;
  scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy_;

  // A callback to bind ServiceWorkerStorageControl. Used for tests.
  StorageControlBinder storage_control_binder_for_test_;

  // A loader factory used to register a service worker. Used for tests.
  scoped_refptr<network::SharedURLLoaderFactory> loader_factory_for_test_;

 private:
  // Returns a version if the worker is live, otherwise nullptr.
  ServiceWorkerVersion* GetLiveServiceWorker(int64_t service_worker_version_id);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTEXT_WRAPPER_H_
