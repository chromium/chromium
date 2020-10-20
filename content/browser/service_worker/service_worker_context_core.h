// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTEXT_CORE_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTEXT_CORE_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/containers/id_map.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list_threadsafe.h"
#include "components/services/storage/public/mojom/service_worker_storage_control.mojom.h"
#include "content/browser/service_worker/service_worker_info.h"
#include "content/browser/service_worker/service_worker_process_manager.h"
#include "content/browser/service_worker/service_worker_registration_status.h"
#include "content/browser/service_worker/service_worker_registry.h"
#include "content/browser/service_worker/service_worker_storage.h"
#include "content/common/content_export.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/service_worker_context.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"

class GURL;

namespace base {
class FilePath;
}

namespace storage {
class QuotaManagerProxy;
class SpecialStoragePolicy;
}  // namespace storage

namespace content {

class ServiceWorkerContextCoreObserver;
class ServiceWorkerContextWrapper;
class ServiceWorkerJobCoordinator;
class ServiceWorkerRegistration;
class URLLoaderFactoryGetter;

// This class manages data associated with service workers.
// The class is single threaded and should only be used on the IO thread.
// In chromium, there is one instance per storagepartition. This class
// is the root of the containment hierarchy for service worker data
// associated with a particular partition.
class CONTENT_EXPORT ServiceWorkerContextCore
    : public ServiceWorkerVersion::Observer {
 public:
  using BoolCallback = base::OnceCallback<void(bool)>;
  using StatusCallback =
      base::OnceCallback<void(blink::ServiceWorkerStatusCode status)>;
  using RegistrationCallback =
      base::OnceCallback<void(blink::ServiceWorkerStatusCode status,
                              const std::string& status_message,
                              int64_t registration_id)>;
  using UpdateCallback =
      base::OnceCallback<void(blink::ServiceWorkerStatusCode status,
                              const std::string& status_message,
                              int64_t registration_id)>;
  using UnregistrationCallback =
      base::OnceCallback<void(blink::ServiceWorkerStatusCode status)>;
  using ContainerHostByClientUUIDMap =
      std::map<std::string, std::unique_ptr<ServiceWorkerContainerHost>>;

  // Iterates over ServiceWorkerContainerHost objects in the
  // ContainerHostByClientUUIDMap.
  // Note: As ContainerHostIterator is operating on a member of
  // ServiceWorkerContextCore, users must ensure the ServiceWorkerContextCore
  // instance always outlives the ContainerHostIterator one.
  class CONTENT_EXPORT ContainerHostIterator {
   public:
    ~ContainerHostIterator();
    ServiceWorkerContainerHost* GetContainerHost();
    void Advance();
    bool IsAtEnd();

   private:
    friend class ServiceWorkerContextCore;
    using ContainerHostPredicate =
        base::RepeatingCallback<bool(ServiceWorkerContainerHost*)>;
    ContainerHostIterator(ContainerHostByClientUUIDMap* map,
                          ContainerHostPredicate predicate);
    void ForwardUntilMatchingContainerHost();

    ContainerHostByClientUUIDMap* const map_;
    ContainerHostPredicate predicate_;
    ContainerHostByClientUUIDMap::iterator container_host_iterator_;

    DISALLOW_COPY_AND_ASSIGN(ContainerHostIterator);
  };

  // This is owned by the StoragePartition, which will supply it with
  // the local path on disk. Given an empty |user_data_directory|,
  // nothing will be stored on disk. |observer_list| is created in
  // ServiceWorkerContextWrapper. When Notify() of |observer_list| is called in
  // ServiceWorkerContextCore, the methods of ServiceWorkerContextCoreObserver
  // will be called on the thread which called AddObserver() of |observer_list|.
  ServiceWorkerContextCore(
      const base::FilePath& user_data_directory,
      scoped_refptr<base::SequencedTaskRunner> database_task_runner,
      storage::QuotaManagerProxy* quota_manager_proxy,
      storage::SpecialStoragePolicy* special_storage_policy,
      URLLoaderFactoryGetter* url_loader_factory_getter,
      std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
          non_network_pending_loader_factory_bundle_for_update_check,
      base::ObserverListThreadSafe<ServiceWorkerContextCoreObserver>*
          observer_list,
      ServiceWorkerContextWrapper* wrapper);
  // TODO(https://crbug.com/877356): Remove this copy mechanism.
  ServiceWorkerContextCore(ServiceWorkerContextCore* old_context,
                           ServiceWorkerContextWrapper* wrapper);
  ~ServiceWorkerContextCore() override;

  void OnStorageWiped();

  void OnMainScriptResponseSet(
      int64_t version_id,
      const ServiceWorkerVersion::MainScriptResponse& response);

  // OnControlleeAdded/Removed are called asynchronously. It is possible the
  // container host identified by |client_uuid| was already destroyed when they
  // are called.
  // Note regarding BackForwardCache integration:
  // OnControlleeRemoved is called when a controllee enters back-forward
  // cache, and OnControlleeAdded is called when a controllee is restored from
  // back-forward cache.
  void OnControlleeAdded(ServiceWorkerVersion* version,
                         const std::string& client_uuid,
                         const ServiceWorkerClientInfo& client_info);
  void OnControlleeRemoved(ServiceWorkerVersion* version,
                           const std::string& client_uuid);

  // Called when the navigation for a window client commits to a render frame
  // host. Also called asynchronously to preserve the ordering with
  // OnControlleeAdded and OnControlleeRemoved.
  void OnControlleeNavigationCommitted(
      ServiceWorkerVersion* version,
      const std::string& client_uuid,
      GlobalFrameRoutingId render_frame_host_id);

  // Called when all controllees are removed.
  // Note regarding BackForwardCache integration:
  // Clients in back-forward cache don't count as controllees.
  void OnNoControllees(ServiceWorkerVersion* version);

  // ServiceWorkerVersion::Observer overrides.
  void OnRunningStateChanged(ServiceWorkerVersion* version) override;
  void OnVersionStateChanged(ServiceWorkerVersion* version) override;
  void OnDevToolsRoutingIdChanged(ServiceWorkerVersion* version) override;
  void OnErrorReported(ServiceWorkerVersion* version,
                       const base::string16& error_message,
                       int line_number,
                       int column_number,
                       const GURL& source_url) override;
  void OnReportConsoleMessage(ServiceWorkerVersion* version,
                              blink::mojom::ConsoleMessageSource source,
                              blink::mojom::ConsoleMessageLevel message_level,
                              const base::string16& message,
                              int line_number,
                              const GURL& source_url) override;

  ServiceWorkerContextWrapper* wrapper() const { return wrapper_; }
  ServiceWorkerRegistry* registry() const { return registry_.get(); }
  mojo::Remote<storage::mojom::ServiceWorkerStorageControl>&
  GetStorageControl();
  ServiceWorkerProcessManager* process_manager();
  ServiceWorkerJobCoordinator* job_coordinator() {
    return job_coordinator_.get();
  }

  // Returns a ContainerHost iterator for all service worker clients for the
  // |origin|. If |include_reserved_clients| is true, this includes clients that
  // are not execution ready (i.e., for windows, the document has not yet been
  // created and for workers, the final response after redirects has not yet
  // been delivered). If |include_back_forward_cached_clients| is true, this
  // includes the clients whose documents are stored in BackForward Cache.
  std::unique_ptr<ContainerHostIterator> GetClientContainerHostIterator(
      const GURL& origin,
      bool include_reserved_clients,
      bool include_back_forward_cached_clients);

  // Returns a ContainerHost iterator for service worker window clients for the
  // |origin|. If |include_reserved_clients| is false, this only returns clients
  // that are execution ready.
  std::unique_ptr<ContainerHostIterator> GetWindowClientContainerHostIterator(
      const GURL& origin,
      bool include_reserved_clients);

  // Runs the callback with true if there is a ContainerHost for |origin| of
  // type blink::mojom::ServiceWorkerContainerType::kForWindow which is a main
  // (top-level) frame. Reserved clients are ignored.
  // TODO(crbug.com/824858): Make this synchronously return bool when the core
  // thread is UI.
  void HasMainFrameWindowClient(const GURL& origin, BoolCallback callback);

  // Used to create a ServiceWorkerContainerHost for a window during a
  // navigation. |are_ancestors_secure| should be true for main frames.
  // Otherwise it is true iff all ancestor frames of this frame have a secure
  // origin. |frame_tree_node_id| is FrameTreeNode id.
  base::WeakPtr<ServiceWorkerContainerHost> CreateContainerHostForWindow(
      mojo::PendingAssociatedReceiver<blink::mojom::ServiceWorkerContainerHost>
          host_receiver,
      bool are_ancestors_secure,
      mojo::PendingAssociatedRemote<blink::mojom::ServiceWorkerContainer>
          container_remote,
      int frame_tree_node_id);

  // Used for starting a web worker (dedicated worker or shared worker). Returns
  // a container host for the worker.
  base::WeakPtr<ServiceWorkerContainerHost> CreateContainerHostForWorker(
      mojo::PendingAssociatedReceiver<blink::mojom::ServiceWorkerContainerHost>
          host_receiver,
      int process_id,
      mojo::PendingAssociatedRemote<blink::mojom::ServiceWorkerContainer>
          container_remote,
      ServiceWorkerClientInfo client_info);

  // Updates the client UUID of an existing container host.
  void UpdateContainerHostClientID(const std::string& current_client_uuid,
                                   const std::string& new_client_uuid);

  // Retrieves a container host given its client UUID.
  ServiceWorkerContainerHost* GetContainerHostByClientID(
      const std::string& client_uuid);

  void OnContainerHostReceiverDisconnected();

  void RegisterServiceWorker(
      const GURL& script_url,
      const blink::mojom::ServiceWorkerRegistrationOptions& options,
      blink::mojom::FetchClientSettingsObjectPtr
          outside_fetch_client_settings_object,
      RegistrationCallback callback);

  // If |is_immediate| is true, unregister clears the active worker from the
  // registration without waiting for the controlled clients to unload.
  void UnregisterServiceWorker(const GURL& scope,
                               bool is_immediate,
                               UnregistrationCallback callback);

  // Callback is called after all deletions occurred. The status code is
  // blink::ServiceWorkerStatusCode::kOk if all succeed, or
  // SERVICE_WORKER_FAILED if any did not succeed.
  void DeleteForOrigin(const url::Origin& origin, StatusCallback callback);

  // Performs internal storage cleanup. Operations to the storage in the past
  // (e.g. deletion) are usually recorded in disk for a certain period until
  // compaction happens. This method wipes them out to ensure that the deleted
  // entries and other traces like log files are removed.
  void PerformStorageCleanup(base::OnceClosure callback);

  // Updates the service worker. If |force_bypass_cache| is true or 24 hours
  // have passed since the last update, bypasses the browser cache.
  void UpdateServiceWorker(ServiceWorkerRegistration* registration,
                           bool force_bypass_cache);
  // |callback| is called when the promise for
  // ServiceWorkerRegistration.update() would be resolved.
  void UpdateServiceWorker(ServiceWorkerRegistration* registration,
                           bool force_bypass_cache,
                           bool skip_script_comparison,
                           blink::mojom::FetchClientSettingsObjectPtr
                               outside_fetch_client_settings_object,
                           UpdateCallback callback);

  // Used in DevTools to update the service worker registrations without
  // consulting the browser cache while loading the controlled page. The
  // loading is delayed until the update completes and the new worker is
  // activated. The new worker skips the waiting state and immediately
  // becomes active after installed.
  bool force_update_on_page_load() { return force_update_on_page_load_; }
  void set_force_update_on_page_load(bool force_update_on_page_load) {
    force_update_on_page_load_ = force_update_on_page_load;
  }

  // This class maintains collections of live instances, this class
  // does not own these object or influence their lifetime.
  ServiceWorkerRegistration* GetLiveRegistration(int64_t registration_id);
  void AddLiveRegistration(ServiceWorkerRegistration* registration);
  // RemoveLiveRegistration removes registration from |live_registrations_|
  // and notifies all observers of the id of the registration removed.
  void RemoveLiveRegistration(int64_t registration_id);
  const std::map<int64_t, ServiceWorkerRegistration*>& GetLiveRegistrations()
      const {
    return live_registrations_;
  }
  ServiceWorkerVersion* GetLiveVersion(int64_t version_id);
  void AddLiveVersion(ServiceWorkerVersion* version);
  void RemoveLiveVersion(int64_t registration_id);
  const std::map<int64_t, ServiceWorkerVersion*>& GetLiveVersions() const {
    return live_versions_;
  }

  std::vector<ServiceWorkerRegistrationInfo> GetAllLiveRegistrationInfo();
  std::vector<ServiceWorkerVersionInfo> GetAllLiveVersionInfo();

  // ProtectVersion holds a reference to |version| until UnprotectVersion is
  // called.
  void ProtectVersion(const scoped_refptr<ServiceWorkerVersion>& version);
  void UnprotectVersion(int64_t version_id);

  void ScheduleDeleteAndStartOver() const;

  // Deletes all files on disk and restarts the system. This leaves the system
  // in a disabled state until it's done.
  void DeleteAndStartOver(StatusCallback callback);

  void ClearAllServiceWorkersForTest(base::OnceClosure callback);

  // Determines if there is a ServiceWorker registration that matches
  // |url|. See ServiceWorkerContext::CheckHasServiceWorker for more
  // details.
  void CheckHasServiceWorker(
      const GURL& url,
      const ServiceWorkerContext::CheckHasServiceWorkerCallback callback);

  // Returns OfflineCapability of the service worker matching |url|.
  // See ServiceWorkerContext::CheckOfflineCapability for more
  // details.
  void CheckOfflineCapability(
      const GURL& url,
      const ServiceWorkerContext::CheckOfflineCapabilityCallback callback);

  void UpdateVersionFailureCount(int64_t version_id,
                                 blink::ServiceWorkerStatusCode status);
  // Returns the count of consecutive start worker failures for the given
  // version. The count resets to zero when the worker successfully starts.
  int GetVersionFailureCount(int64_t version_id);

  // Called by ServiceWorkerStorage when StoreRegistration() succeeds.
  void NotifyRegistrationStored(int64_t registration_id, const GURL& scope);
  // Called on the core thread and notifies observers that all registrations
  // have been deleted for a particular origin.
  void NotifyAllRegistrationsDeletedForOrigin(const url::Origin& origin);

  URLLoaderFactoryGetter* loader_factory_getter() {
    return loader_factory_getter_.get();
  }

  const scoped_refptr<blink::URLLoaderFactoryBundle>&
  loader_factory_bundle_for_update_check() {
    return loader_factory_bundle_for_update_check_;
  }

  base::WeakPtr<ServiceWorkerContextCore> AsWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  int GetNextEmbeddedWorkerId();

 private:
  friend class ServiceWorkerContextCoreTest;
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerContextCoreTest, FailureInfo);

  typedef std::map<int64_t, ServiceWorkerRegistration*> RegistrationsMap;
  typedef std::map<int64_t, ServiceWorkerVersion*> VersionMap;

  struct FailureInfo {
    int count;
    blink::ServiceWorkerStatusCode last_failure;
  };

  void RegistrationComplete(const GURL& scope,
                            RegistrationCallback callback,
                            blink::ServiceWorkerStatusCode status,
                            const std::string& status_message,
                            ServiceWorkerRegistration* registration);

  void UpdateComplete(UpdateCallback callback,
                      blink::ServiceWorkerStatusCode status,
                      const std::string& status_message,
                      ServiceWorkerRegistration* registration);

  void UnregistrationComplete(const GURL& scope,
                              UnregistrationCallback callback,
                              int64_t registration_id,
                              blink::ServiceWorkerStatusCode status);

  bool IsValidRegisterRequest(const GURL& script_url,
                              const GURL& scope_url,
                              std::string* out_error) const;

  void DidGetRegistrationsForDeleteForOrigin(
      const url::Origin& origin,
      base::OnceCallback<void(blink::ServiceWorkerStatusCode)> callback,
      blink::ServiceWorkerStatusCode status,
      const std::vector<scoped_refptr<ServiceWorkerRegistration>>&
          registrations);

  void DidFindRegistrationForCheckHasServiceWorker(
      ServiceWorkerContext::CheckHasServiceWorkerCallback callback,
      blink::ServiceWorkerStatusCode status,
      scoped_refptr<ServiceWorkerRegistration> registration);
  void OnRegistrationFinishedForCheckHasServiceWorker(
      ServiceWorkerContext::CheckHasServiceWorkerCallback callback,
      scoped_refptr<ServiceWorkerRegistration> registration);

  // It's safe to store a raw pointer instead of a scoped_refptr to |wrapper_|
  // because the Wrapper::Shutdown call that hops threads to destroy |this| uses
  // Bind() to hold a reference to |wrapper_| until |this| is fully destroyed.
  ServiceWorkerContextWrapper* wrapper_;

  // |container_host_by_uuid_| owns container hosts for service worker clients.
  // Container hosts for service worker execution contexts are owned by
  // ServiceWorkerHost.
  ContainerHostByClientUUIDMap container_host_by_uuid_;

  std::unique_ptr<
      mojo::AssociatedReceiverSet<blink::mojom::ServiceWorkerContainerHost,
                                  ServiceWorkerContainerHost*>>
      container_host_receivers_;

  std::unique_ptr<ServiceWorkerRegistry> registry_;
  std::unique_ptr<ServiceWorkerJobCoordinator> job_coordinator_;
  // TODO(bashi): Move |live_registrations_| to ServiceWorkerRegistry as
  // ServiceWorkerRegistry is a better place to manage in-memory representation
  // of registrations.
  std::map<int64_t, ServiceWorkerRegistration*> live_registrations_;
  std::map<int64_t, ServiceWorkerVersion*> live_versions_;
  std::map<int64_t, scoped_refptr<ServiceWorkerVersion>> protected_versions_;

  std::map<int64_t /* version_id */, FailureInfo> failure_counts_;

  scoped_refptr<URLLoaderFactoryGetter> loader_factory_getter_;

  scoped_refptr<blink::URLLoaderFactoryBundle>
      loader_factory_bundle_for_update_check_;

  bool force_update_on_page_load_;
  // Set in RegisterServiceWorker(), cleared in ClearAllServiceWorkersForTest().
  // This is used to avoid unnecessary disk read operation in tests. This value
  // is false if Chrome was relaunched after service workers were registered.
  bool was_service_worker_registered_;
  using ServiceWorkerContextObserverList =
      base::ObserverListThreadSafe<ServiceWorkerContextCoreObserver>;
  const scoped_refptr<ServiceWorkerContextObserverList> observer_list_;

  int next_embedded_worker_id_ = 0;

  base::WeakPtrFactory<ServiceWorkerContextCore> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerContextCore);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTEXT_CORE_H_
