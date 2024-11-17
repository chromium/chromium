// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTEXT_CORE_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTEXT_CORE_H_

#include <stdint.h>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/id_map.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list_threadsafe.h"
#include "base/observer_list_types.h"
#include "components/services/storage/public/mojom/quota_client.mojom.h"
#include "components/services/storage/public/mojom/service_worker_storage_control.mojom.h"
#include "content/browser/service_worker/service_worker_info.h"
#include "content/browser/service_worker/service_worker_process_manager.h"
#include "content/browser/service_worker/service_worker_registration_status.h"
#include "content/browser/service_worker/service_worker_registry.h"
#include "content/common/content_export.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/service_worker_context.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom-forward.h"

class GURL;

namespace storage {
class QuotaClientCallbackWrapper;
class QuotaManagerProxy;
class SpecialStoragePolicy;
}  // namespace storage

namespace content {
class ServiceWorkerContainerHostForClient;
class ServiceWorkerContextCoreObserver;
class ServiceWorkerContextWrapper;
class ServiceWorkerJobCoordinator;
class ServiceWorkerQuotaClient;
class ServiceWorkerRegistration;
struct ServiceWorkerContextSynchronousObserverList;

#if !BUILDFLAG(IS_ANDROID)
class ServiceWorkerHidDelegateObserver;
class ServiceWorkerUsbDelegateObserver;
#endif  // !BUILDFLAG(IS_ANDROID)

// A smart pointer of `ServiceWorkerClient`.
//
// - If `CommitResponseAndRelease()` is not called, this works as a
//   semi-strong reference:
//   - Keeps the underlying `ServiceWorkerClient` alive unless its
//     `ServiceWorkerClientOwner` is destroyed.
//   - Destroys the `ServiceWorkerClient` synchronously in the
//     `ScopedServiceWorkerClient` destructor.
//   - Actually the `ServiceWorkerClient` is owned by `ServiceWorkerClientOwner`
//     and `ServiceWorkerClientOwner::OnContainerHostReceiverDisconnected()` is
//     never called until `ServiceWorkerClientOwner::BindHost()` is called (i.e.
//     `ScopedServiceWorkerClient::CommitResponseAndRelease` is called),
//     and thus there is nothing explicitly to do to keep-alive it by
//     `ScopedServiceWorkerClient`, and the destructor of
//     `ScopedServiceWorkerClient` explicitly destroys the client when
//     `CommitResponseAndRelease()` hasn't been called.
// - After `CommitResponseAndRelease()` is called, this works as a weak
//   reference:
//   - No longer keeps alive nor destroys the `ServiceWorkerClient`. Instead,
//     the returned object from `CommitResponseAndRelease()` keeps it alive
//     (i.e. until
//     `ServiceWorkerClientOwner::OnContainerHostReceiverDisconnected()` is
//     called)
//   - `service_worker_client_` is NOT cleared and still can be used.
class CONTENT_EXPORT ScopedServiceWorkerClient final {
 public:
  explicit ScopedServiceWorkerClient(
      base::WeakPtr<ServiceWorkerClient> service_worker_client);
  ~ScopedServiceWorkerClient();

  ScopedServiceWorkerClient(const ScopedServiceWorkerClient& other) = delete;
  ScopedServiceWorkerClient& operator=(const ScopedServiceWorkerClient& other) =
      delete;

  ScopedServiceWorkerClient(ScopedServiceWorkerClient&& other);
  ScopedServiceWorkerClient& operator=(ScopedServiceWorkerClient&& other) =
      delete;

  // Calls `ServiceWorkerClient::CommitResponse()` and performs related
  // initialization/transitions, and Releases the keep-aliveness from `this`.
  // The caller should keep alive `ServiceWorkerClient` by keeping the returned
  // `ServiceWorkerContainerInfoForClientPtr`'s `host_remote`.
  [[nodiscard]] std::tuple<blink::mojom::ServiceWorkerContainerInfoForClientPtr,
                           blink::mojom::ControllerServiceWorkerInfoPtr>
  CommitResponseAndRelease(
      std::optional<GlobalRenderFrameHostId> rfh_id,
      const PolicyContainerPolicies& policy_container_policies,
      mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
          coep_reporter,
      ukm::SourceId ukm_source_id);

  const base::WeakPtr<ServiceWorkerClient>& AsWeakPtr() const {
    return service_worker_client_;
  }
  ServiceWorkerClient* get() const { return service_worker_client_.get(); }
  ServiceWorkerClient* operator->() const {
    return service_worker_client_.get();
  }

 private:
  base::WeakPtr<ServiceWorkerClient> service_worker_client_;
};

// A class responsible for `ServiceWorkerClient` management, including its
// ownership, lifetime, and client ID updates.
// This is always owned by and associated with a `ServiceWorkerContextCore`.
// This is split from `ServiceWorkerContextCore` to allow `ServiceWorkerClient`
// to access `ServiceWorkerClientOwner` throughout the lifetime of
// `ServiceWorkerClient` while disallow access to other parts of
// `ServiceWorkerContextCore` after `DeleteAndStartOver()`.
// Callers other than `ServiceWorkerClient` /
// `ServiceWorkerContainerHostForClient` should access this through
// `ServiceWorkerContextCore::service_worker_client_owner()`.
class CONTENT_EXPORT ServiceWorkerClientOwner final {
 public:
  using BoolCallback = base::OnceCallback<void(bool)>;
  using ServiceWorkerClientByClientUUIDMap =
      std::map<std::string, std::unique_ptr<ServiceWorkerClient>>;

  // Iterates over ServiceWorkerClient objects in the
  // ServiceWorkerClientByClientUUIDMap.
  // Note: As ServiceWorkerClientIterator is operating on a member of
  // ServiceWorkerClientOwner, users must ensure the ServiceWorkerClientOwner
  // instance always outlives the ServiceWorkerClientIterator one.
  class CONTENT_EXPORT ServiceWorkerClientIterator final {
   public:
    ServiceWorkerClientIterator(const ServiceWorkerClientIterator&) = delete;
    ServiceWorkerClientIterator& operator=(const ServiceWorkerClientIterator&) =
        delete;

    ~ServiceWorkerClientIterator();

    ServiceWorkerClientIterator& operator++();
    bool IsAtEnd() const;

    ServiceWorkerClient& operator*() const;
    ServiceWorkerClient* operator->() const;

   private:
    friend class ServiceWorkerClientOwner;
    using ServiceWorkerClientPredicate =
        base::RepeatingCallback<bool(ServiceWorkerClient&)>;
    ServiceWorkerClientIterator(ServiceWorkerClientByClientUUIDMap* map,
                                ServiceWorkerClientPredicate predicate);
    void ForwardUntilMatchingServiceWorkerClient();

    const raw_ptr<ServiceWorkerClientByClientUUIDMap, DanglingUntriaged> map_;
    ServiceWorkerClientPredicate predicate_;
    ServiceWorkerClientByClientUUIDMap::iterator iterator_;
  };

  explicit ServiceWorkerClientOwner(ServiceWorkerContextCore& context);
  ServiceWorkerClientOwner(const ServiceWorkerClientOwner& other) = delete;
  ServiceWorkerClientOwner& operator=(const ServiceWorkerClientOwner& other) =
      delete;
  ~ServiceWorkerClientOwner();

  // Should be only called when the old `context_` is about to be destroyed and
  // the ownership of `this` is being moved to `new_context`.
  void ResetContext(ServiceWorkerContextCore& new_context);

  // Returns an iterator for all service worker clients for the
  // `key`. If `include_reserved_clients` is true, this includes clients that
  // are not execution ready (i.e., for windows, the document has not yet been
  // created and for workers, the final response after redirects has not yet
  // been delivered). If `include_back_forward_cached_clients` is true, this
  // includes the clients whose documents are stored in BackForward Cache.
  ServiceWorkerClientIterator GetServiceWorkerClients(
      const blink::StorageKey& key,
      bool include_reserved_clients,
      bool include_back_forward_cached_clients);

  // Returns an iterator for service worker window clients for the
  // `key`. If `include_reserved_clients` is false, this only returns clients
  // that are execution ready.
  ServiceWorkerClientIterator GetWindowServiceWorkerClients(
      const blink::StorageKey& key,
      bool include_reserved_clients);

  // Runs the callback with true if there is a service worker client for `key`
  // of type blink::mojom::ServiceWorkerContainerType::kForWindow which is a
  // main (top-level) frame. Reserved clients are ignored.
  // TODO(crbug.com/40568315): Make this synchronously return bool when the core
  // thread is UI.
  void HasMainFrameWindowClient(const blink::StorageKey& key,
                                BoolCallback callback);

  // Used to create a ServiceWorkerClient for a window during a
  // navigation. |are_ancestors_secure| should be true for main frames.
  // Otherwise it is true iff all ancestor frames of this frame have a secure
  // origin. |frame_tree_node_id| is FrameTreeNode id.
  ScopedServiceWorkerClient CreateServiceWorkerClientForWindow(
      bool are_ancestors_secure,
      FrameTreeNodeId frame_tree_node_id);

  // Used for starting a web worker (dedicated worker or shared worker). Returns
  // a service worker client for the worker.
  ScopedServiceWorkerClient CreateServiceWorkerClientForWorker(
      int process_id,
      ServiceWorkerClientInfo client_info);

  // Binds the ServiceWorkerContainerHost mojo receiver for `container_host`.
  // After this point, `container_host` and its `ServiceWorkerClient` will be
  // destroyed on the mojo pipe close.
  void BindHost(
      ServiceWorkerContainerHostForClient& container_host,
      mojo::PendingAssociatedReceiver<blink::mojom::ServiceWorkerContainerHost>
          host_receiver);

  void DestroyServiceWorkerClient(
      base::WeakPtr<ServiceWorkerClient> service_worker_client);

  // Updates the client UUID of an existing service worker client.
  void UpdateServiceWorkerClientClientID(const std::string& current_client_uuid,
                                         const std::string& new_client_uuid);

  // Retrieves a service worker client given its client UUID.
  ServiceWorkerClient* GetServiceWorkerClientByClientID(
      const std::string& client_uuid);

  // Retrieves a service worker client given its window ID.
  ServiceWorkerClient* GetServiceWorkerClientByWindowId(
      const base::UnguessableToken& window_id);

  void OnContainerHostReceiverDisconnected();

 private:
  // The `ServiceWorkerContextCore` that owns `this`. This can change due to
  // `DeleteAndStartOver` but is still always valid and non-null.
  raw_ref<ServiceWorkerContextCore> context_;

  // Owns `ServiceWorkerContainerForClient` (via `ServiceWorkerClient`).
  // `ServiceWorkerContainerForServiceWorker`s are owned by `ServiceWorkerHost`.
  ServiceWorkerClientByClientUUIDMap service_worker_clients_by_uuid_;

  std::unique_ptr<
      mojo::AssociatedReceiverSet<blink::mojom::ServiceWorkerContainerHost,
                                  ServiceWorkerContainerHostForClient*>>
      container_host_receivers_;
};

// This class manages data associated with service workers.
// The class is single threaded and should only be used on the UI thread.
// In chromium, there is one instance per storagepartition. This class
// is the root of the containment hierarchy for service worker data
// associated with a particular partition.
class CONTENT_EXPORT ServiceWorkerContextCore
    : public ServiceWorkerVersion::Observer {
 public:
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
  using WarmUpRequest =
      std::tuple<GURL,
                 blink::StorageKey,
                 ServiceWorkerContext::WarmUpServiceWorkerCallback>;

  class TestVersionObserver : public base::CheckedObserver {
   public:
    TestVersionObserver() = default;

    // Called when a new `ServiceWorkerVersion` is added to this context.
    virtual void OnServiceWorkerVersionCreated(
        ServiceWorkerVersion* service_worker_version) {}
  };

  // This is owned by ServiceWorkerContextWrapper. `observer_list` is created in
  // ServiceWorkerContextWrapper. When Notify() of `observer_list` is called in
  // ServiceWorkerContextCore, the methods of ServiceWorkerContextCoreObserver
  // will be called on the thread which called AddObserver() of `observer_list`.
  // `sync_observer_list` is a synchronously notified subset of
  // ServiceWorkerContextObserver.
  ServiceWorkerContextCore(
      storage::QuotaManagerProxy* quota_manager_proxy,
      storage::SpecialStoragePolicy* special_storage_policy,
      std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
          non_network_pending_loader_factory_bundle_for_update_check,
      base::ObserverListThreadSafe<ServiceWorkerContextCoreObserver>*
          observer_list,
      ServiceWorkerContextSynchronousObserverList* sync_observer_list,
      ServiceWorkerContextWrapper* wrapper);
  // TODO(crbug.com/41409843): Remove this copy mechanism.
  ServiceWorkerContextCore(
      std::unique_ptr<ServiceWorkerContextCore> old_context,
      ServiceWorkerContextWrapper* wrapper);

  ServiceWorkerContextCore(const ServiceWorkerContextCore&) = delete;
  ServiceWorkerContextCore& operator=(const ServiceWorkerContextCore&) = delete;

  ~ServiceWorkerContextCore() override;

  ServiceWorkerClientOwner& service_worker_client_owner() {
    return *service_worker_client_owner_;
  }

  void OnClientDestroyed(ServiceWorkerClient& service_worker_client);

  void OnStorageWiped();

  void OnMainScriptResponseSet(
      int64_t version_id,
      const ServiceWorkerVersion::MainScriptResponse& response);

  // Called when a Service Worker opens a window.
  void OnWindowOpened(const GURL& script_url, const GURL& url);

  // Called when a Service Worker navigates an existing tab.
  void OnClientNavigated(const GURL& script_url, const GURL& url);

  // OnControlleeAdded/Removed are called asynchronously. It is possible the
  // service worker client identified by |client_uuid| was already destroyed
  // when they are called. Note regarding BackForwardCache integration:
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
      GlobalRenderFrameHostId render_frame_host_id);

  // Called when all controllees are removed.
  // Note regarding BackForwardCache integration:
  // Clients in back-forward cache don't count as controllees.
  void OnNoControllees(ServiceWorkerVersion* version);

  // ServiceWorkerVersion::Observer overrides.
  void OnRunningStateChanged(ServiceWorkerVersion* version) override;
  void OnVersionStateChanged(ServiceWorkerVersion* version) override;
  void OnDevToolsRoutingIdChanged(ServiceWorkerVersion* version) override;
  void OnErrorReported(ServiceWorkerVersion* version,
                       const std::u16string& error_message,
                       int line_number,
                       int column_number,
                       const GURL& source_url) override;
  void OnReportConsoleMessage(ServiceWorkerVersion* version,
                              blink::mojom::ConsoleMessageSource source,
                              blink::mojom::ConsoleMessageLevel message_level,
                              const std::u16string& message,
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

  void RegisterServiceWorker(
      const GURL& script_url,
      const blink::StorageKey& key,
      const blink::mojom::ServiceWorkerRegistrationOptions& options,
      blink::mojom::FetchClientSettingsObjectPtr
          outside_fetch_client_settings_object,
      RegistrationCallback callback,
      const GlobalRenderFrameHostId& requesting_frame_id,
      const PolicyContainerPolicies& policy_container_policies);

  // If `is_immediate` is true, unregister clears the active worker from the
  // registration without waiting for the controlled clients to unload.
  void UnregisterServiceWorker(const GURL& scope,
                               const blink::StorageKey& key,
                               bool is_immediate,
                               UnregistrationCallback callback);

  // Callback is called after all deletions occurred. The status code is
  // blink::ServiceWorkerStatusCode::kOk if all succeed, or
  // SERVICE_WORKER_FAILED if any did not succeed.
  void DeleteForStorageKey(const blink::StorageKey& key,
                           StatusCallback callback);

  // Updates the service worker. If |force_bypass_cache| is true or 24 hours
  // have passed since the last update, bypasses the browser cache.
  // This is used for update requests where there is no associated execution
  // context.
  void UpdateServiceWorkerWithoutExecutionContext(
      ServiceWorkerRegistration* registration,
      bool force_bypass_cache);
  // As above, but for sites with an associated execution context, which leads
  // to the specification of `outside_fetch_client_settings_object`.
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
  // does not own these object or influence their lifetime.  It returns
  // a scoped_refptr<>, however, as the caller must keep the registration
  // alive while operating on it.
  scoped_refptr<ServiceWorkerRegistration> GetLiveRegistration(
      int64_t registration_id);
  void AddLiveRegistration(ServiceWorkerRegistration* registration);
  // Erases the live registration for `registration_id`, if found.
  void RemoveLiveRegistration(int64_t registration_id);
  const std::map<int64_t, raw_ptr<ServiceWorkerRegistration, CtnExperimental>>&
  GetLiveRegistrations() const {
    return live_registrations_;
  }
  ServiceWorkerVersion* GetLiveVersion(int64_t version_id);
  void AddLiveVersion(ServiceWorkerVersion* version);
  void RemoveLiveVersion(int64_t registration_id);
  const std::map<int64_t, raw_ptr<ServiceWorkerVersion, CtnExperimental>>&
  GetLiveVersions() const {
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
  // `url` and `key`. See ServiceWorkerContext::CheckHasServiceWorker for more
  // details.
  void CheckHasServiceWorker(
      const GURL& url,
      const blink::StorageKey& key,
      const ServiceWorkerContext::CheckHasServiceWorkerCallback callback);

  void UpdateVersionFailureCount(int64_t version_id,
                                 blink::ServiceWorkerStatusCode status);
  // Returns the count of consecutive start worker failures for the given
  // version. The count resets to zero when the worker successfully starts.
  int GetVersionFailureCount(int64_t version_id);

  // Called by ServiceWorkerStorage when StoreRegistration() succeeds.
  void NotifyRegistrationStored(int64_t registration_id,
                                const GURL& scope,
                                const blink::StorageKey& key);
  // Notifies observers that all registrations have been deleted for a
  // particular `key`.
  void NotifyAllRegistrationsDeletedForStorageKey(const blink::StorageKey& key);

  const scoped_refptr<blink::URLLoaderFactoryBundle>&
  loader_factory_bundle_for_update_check() {
    return loader_factory_bundle_for_update_check_;
  }

  base::WeakPtr<ServiceWorkerContextCore> AsWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  int GetNextEmbeddedWorkerId();

  void NotifyClientIsExecutionReady(
      const ServiceWorkerClient& service_worker_client);

  bool MaybeHasRegistrationForStorageKey(const blink::StorageKey& key);

  // This method waits for service worker registrations to be initialized, and
  // depends on |on_registrations_initialized_| and |registrations_initialized_|
  // which are called in InitializeRegisteredOrigins().
  void WaitForRegistrationsInitializedForTest();

  // Enqueue a warm-up request that consists of a tuple of (document_url, key,
  // callback). The added request will be consumed in LIFO order. If the
  // `warm_up_requests_` queue size exceeds the limit, then the older entries
  // will be removed from the queue, and the removed entry's callbacks will be
  // triggered.
  void AddWarmUpRequest(
      const GURL& document_url,
      const blink::StorageKey& key,
      ServiceWorkerContext::WarmUpServiceWorkerCallback callback);

  std::optional<WarmUpRequest> PopNextWarmUpRequest();
  bool IsWaitingForWarmUp(const blink::StorageKey& key) const;

  bool IsProcessingWarmingUp() const { return is_processing_warming_up_; }
  void BeginProcessingWarmingUp() { is_processing_warming_up_ = true; }
  void EndProcessingWarmingUp() { is_processing_warming_up_ = false; }

  void AddVersionObserverForTest(TestVersionObserver* observer) {
    test_version_observers_.AddObserver(observer);
  }

  void RemoveVersionObserverForTest(TestVersionObserver* observer) {
    test_version_observers_.RemoveObserver(observer);
  }

#if !BUILDFLAG(IS_ANDROID)
  ServiceWorkerHidDelegateObserver* hid_delegate_observer();

  void SetServiceWorkerHidDelegateObserverForTesting(
      std::unique_ptr<ServiceWorkerHidDelegateObserver> hid_delegate_observer);

  // In the service worker case, WebUSB is only available in extension service
  // workers. Since extension isn't available in ANDROID, guard
  // ServiceWorkerUsbDelegateObserver within non-android platforms.
  ServiceWorkerUsbDelegateObserver* usb_delegate_observer();

  void SetServiceWorkerUsbDelegateObserverForTesting(
      std::unique_ptr<ServiceWorkerUsbDelegateObserver> usb_delegate_observer);
#endif  // !BUILDFLAG(IS_ANDROID)

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
                            const blink::StorageKey& key,
                            RegistrationCallback callback,
                            blink::ServiceWorkerStatusCode status,
                            const std::string& status_message,
                            ServiceWorkerRegistration* registration);

  void UpdateServiceWorkerImpl(ServiceWorkerRegistration* registration,
                               bool force_bypass_cache,
                               bool skip_script_comparison,
                               blink::mojom::FetchClientSettingsObjectPtr
                                   outside_fetch_client_settings_object,
                               UpdateCallback callback);

  void UpdateComplete(UpdateCallback callback,
                      blink::ServiceWorkerStatusCode status,
                      const std::string& status_message,
                      ServiceWorkerRegistration* registration);
  void UnregistrationComplete(const GURL& scope,
                              const blink::StorageKey& key,
                              UnregistrationCallback callback,
                              int64_t registration_id,
                              blink::ServiceWorkerStatusCode status);
  bool IsValidRegisterRequest(const GURL& script_url,
                              const GURL& scope_url,
                              const blink::StorageKey& key,
                              std::string* out_error) const;

  void DidGetRegistrationsForDeleteForStorageKey(
      const blink::StorageKey& key,
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

  // This is used as a callback of GetRegisteredStorageKeys when initialising to
  // store a list of storage keys that have registered service workers.
  void DidGetRegisteredStorageKeys(
      base::TimeTicks start_time,
      const std::vector<blink::StorageKey>& storage_keys);

  // It's safe to store a raw pointer instead of a scoped_refptr to |wrapper_|
  // because the Wrapper::Shutdown call that hops threads to destroy |this| uses
  // Bind() to hold a reference to |wrapper_| until |this| is fully destroyed.
  raw_ptr<ServiceWorkerContextWrapper> wrapper_;

  std::unique_ptr<ServiceWorkerClientOwner> service_worker_client_owner_;

  std::unique_ptr<ServiceWorkerRegistry> registry_;
  std::unique_ptr<ServiceWorkerJobCoordinator> job_coordinator_;
  // TODO(bashi): Move |live_registrations_| to ServiceWorkerRegistry as
  // ServiceWorkerRegistry is a better place to manage in-memory representation
  // of registrations.
  std::map<int64_t, raw_ptr<ServiceWorkerRegistration, CtnExperimental>>
      live_registrations_;
  std::map<int64_t, raw_ptr<ServiceWorkerVersion, CtnExperimental>>
      live_versions_;
  std::map<int64_t, scoped_refptr<ServiceWorkerVersion>> protected_versions_;

  std::map<int64_t /* version_id */, FailureInfo> failure_counts_;

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
  const scoped_refptr<ServiceWorkerContextSynchronousObserverList>
      sync_observer_list_;

  int next_embedded_worker_id_ = 0;

  // ServiceWorkerQuotaClient assumes that this context always has an associated
  // ServiceWorkerRegistry, so `quota_client_` must be declared after
  // `registry_`.
  //
  // ServiceWorkerQuotaClient is held via a std::unique_ptr so it can be
  // transferred (along with any state it may hold) to a different
  // ServiceWorkerContextCore by the logic kicked off from
  // ServiceWorkerRegistry::ScheduleDeleteAndStartOver().
  std::unique_ptr<ServiceWorkerQuotaClient> quota_client_;
  std::unique_ptr<storage::QuotaClientCallbackWrapper> quota_client_wrapper_;

  // ServiceWorkerQuotaClient's mojo pipe to QuotaManager is disconnected when
  // the mojo::Receiver is destroyed.
  //
  // This receiver is held via a std::unique_ptr so it can be transferred (along
  // with its mojo pipe) to a different ServiceWorkerContextCore by the logic
  // kicked off from ServiceWorkerRegistry::ScheduleDeleteAndStartOver().
  std::unique_ptr<mojo::Receiver<storage::mojom::QuotaClient>>
      quota_client_receiver_;

  // A set of StorageKeys that have at least one registration.
  // TODO(http://crbug.com/824858): This can be removed when service workers are
  // fully converted to running on the UI thread.
  std::set<blink::StorageKey> registered_storage_keys_;
  bool registrations_initialized_ = false;
  base::OnceClosure on_registrations_initialized_for_test_;

  std::deque<WarmUpRequest> warm_up_requests_;

  bool is_processing_warming_up_ = false;

#if !BUILDFLAG(IS_ANDROID)
  std::unique_ptr<ServiceWorkerHidDelegateObserver> hid_delegate_observer_;
  std::unique_ptr<ServiceWorkerUsbDelegateObserver> usb_delegate_observer_;
#endif  // !BUILDFLAG(IS_ANDROID)

  base::ObserverList<TestVersionObserver> test_version_observers_;

  base::WeakPtrFactory<ServiceWorkerContextCore> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTEXT_CORE_H_
