// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COOKIE_STORE_COOKIE_STORE_MANAGER_H_
#define CONTENT_BROWSER_COOKIE_STORE_COOKIE_STORE_MANAGER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/containers/linked_list.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "content/browser/cookie_store/cookie_change_subscription.h"
#include "content/browser/cookie_store/cookie_store_host.h"
#include "content/browser/service_worker/service_worker_context_core_observer.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "third_party/blink/public/mojom/cookie_store/cookie_store.mojom.h"
#include "url/origin.h"

class GURL;

namespace content {

class ServiceWorkerContextWrapper;
class ServiceWorkerRegistration;

// Manages cookie change subscriptions for a StoragePartition's service workers.
//
// Subscriptions are stored along with their associated service worker
// registrations in ServiceWorkerStorage, as user data. When a service worker is
// unregistered, its cookie change subscriptions are removed. The storage method
// (user data) is an implementation detail. Callers should not rely on it, as
// the storage method may change in the future.
//
// Instances of this class must be accessed exclusively on the service worker
// core thread, because they call into ServiceWorkerContextWrapper methods that
// are restricted to that thread.
class CookieStoreManager : public ServiceWorkerContextCoreObserver,
                           public ::network::mojom::CookieChangeListener {
 public:
  // Creates a CookieStoreManager with an empty in-memory subscription database.
  //
  // The in-memory subscription database must be populated with data from disk,
  // by calling ReadAllSubscriptions().
  CookieStoreManager(
      scoped_refptr<ServiceWorkerContextWrapper> service_worker_context);

  ~CookieStoreManager() override;

  // Creates a mojo connection to a service worker.
  //
  // This is called when service workers use the Cookie Store API to subscribe
  // to cookie changes or obtain the list of cookie changes.
  void CreateService(mojo::PendingReceiver<blink::mojom::CookieStore> receiver,
                     const url::Origin& origin);

  // Starts loading the on-disk subscription data.
  //
  // Returns after scheduling the work. The callback is called with a boolean
  // that indicates if the load operation succeeded.
  //
  // It is safe to call all the other CookieStoreManager methods during the
  // loading operation. The CookieStoreManager has well-defined semantics if
  // loading fails, so it is not necessary to handle loading errors.
  void LoadAllSubscriptions(base::OnceCallback<void(bool)> callback);

  // Processes cookie changes from a network service instance.
  void ListenToCookieChanges(
      mojo::PendingRemote<::network::mojom::CookieManager> cookie_manager,
      base::OnceCallback<void(bool)> callback);

  // content::mojom::CookieStore implementation
  void AppendSubscriptions(
      int64_t service_worker_registration_id,
      const url::Origin& origin,
      std::vector<blink::mojom::CookieChangeSubscriptionPtr> mojo_subscriptions,
      blink::mojom::CookieStore::AppendSubscriptionsCallback callback);
  void GetSubscriptions(
      int64_t service_worker_registration_id,
      const url::Origin& origin,
      blink::mojom::CookieStore::GetSubscriptionsCallback callback);

  // ServiceWorkerContextCoreObserver
  void OnRegistrationStored(int64_t service_worker_registration_id,
                            const GURL& pattern) override;
  void OnRegistrationDeleted(int64_t service_worker_registration_id,
                             const GURL& pattern) override;
  void OnNewLiveRegistration(int64_t service_worker_registration_id,
                             const GURL& pattern) override;
  void OnStorageWiped() override;

  // ::network::mojom::CookieChangeListener
  void OnCookieChange(const net::CookieChangeInfo& change) override;

 private:
  // Updates internal state with the result of loading disk subscription data.
  //
  // Called exactly once.
  void ProcessOnDiskSubscriptions(
      base::OnceCallback<void(bool)> load_callback,
      const std::vector<std::pair<int64_t, std::string>>& user_data,
      blink::ServiceWorkerStatusCode status);

  // Runs all the callbacks waiting for on-disk subscription data.
  //
  // Called exactly once, after on-disk subcriptions have been loaded.
  void DidLoadAllSubscriptions(bool succeeded,
                               base::OnceCallback<void(bool)> load_callback);

  // Starts sending cookie change events to a service worker.
  //
  // All subscriptions must belong to the same service worker registration. This
  // method is not idempotent.
  void ActivateSubscriptions(
      std::vector<CookieChangeSubscription>* subscriptions);

  // Stops sending cookie change events to a service worker.
  //
  // All subscriptions must belong to the same service worker registration. This
  // method is not idempotent.
  void DeactivateSubscriptions(
      std::vector<CookieChangeSubscription>* subscriptions);

  // Sends a cookie change to interested service workers.
  //
  // Must only be called after the on-disk subscription data is successfully
  // loaded.
  void DispatchCookieChange(const net::CookieChangeInfo& change);

  // Sends a cookie change event to one service worker.
  void DispatchChangeEvent(
      scoped_refptr<ServiceWorkerRegistration> registration,
      const net::CookieChangeInfo& change);

  // Called after a service worker was started so it can get a cookie change.
  void DidStartWorkerForChangeEvent(
      scoped_refptr<ServiceWorkerRegistration> registration,
      const net::CookieChangeInfo& change,
      blink::ServiceWorkerStatusCode start_worker_status);

  // Used to efficiently implement OnRegistrationDeleted().
  //
  // When a service worker registration is removed from the system, the
  // CookieStoreManager needs to remove all the cookie change subscriptions
  // associated with the registration. Looking up the registration ID in the
  // |subscriptions_by_registration_| map is done in O(1) time, and then each
  // subscription is removed from a LinkedList in |subscription_by_url_key_| in
  // O(1) time.
  std::unordered_map<int64_t, std::vector<CookieChangeSubscription>>
      subscriptions_by_registration_;

  // Used to efficiently implement DispatchCookieChange().
  //
  // When a cookie change notification comes from the network service, the
  // CookieStoreManager needs to dispatch events to the workers with relevant
  // subscriptions. |subscriptions_by_url_key_| indexes change subscriptions
  // according to the eTLD+1 of the subscription's scope URL, so each cookie
  // change only needs to be checked against the subscriptions of the service
  // workers in the same eTLD+1. The reduction in work is signficant, given that
  // checking whether a subscription matches a cookie isn't very cheap.
  //
  // The current implementation's performance profile could have been achieved
  // with a map from eTLD+1 to registration IDs, which would not have required
  // linked lists. However, the current approach is more amenable to future
  // optimizations, such as partitioning by (eTLD+1, cookie name).
  std::map<std::string, base::LinkedList<CookieChangeSubscription>>
      subscriptions_by_url_key_;

  // Used to look up and modify service worker registration data.
  scoped_refptr<ServiceWorkerContextWrapper> service_worker_context_;

  // Tracks the open mojo pipes created by CreateService().
  //
  // Each pipe is associated with the CookieStoreHost instance that it is
  // connected to. When the pipe is closed, the UniqueReceiverSet automatically
  // deletes the CookieStoreHost.
  mojo::UniqueReceiverSet<blink::mojom::CookieStore> receivers_;

  // Used to receive cookie changes from the network service.
  mojo::Remote<::network::mojom::CookieManager> cookie_manager_;
  mojo::Receiver<::network::mojom::CookieChangeListener>
      cookie_change_listener_receiver_{this};

  // The service worker registration user data key for subscription data.
  //
  // All the subscriptions associated with a registration are stored in a single
  // user data entry whose key is |registration_user_data_key_|, and whose value
  // is a serialized CookieChangeSubscriptionsProto.
  const std::string registration_user_data_key_;

  // Called after all subscriptions have been loaded.
  //
  // Callbacks can assume that |done_loading_subscriptions_| is true
  // and |succeeded_loading_subscriptions_| is set. If the latter is true,
  // |subscriptions_by_registration_| and |subscriptions_by_url_key_| will also
  // be populated.
  std::vector<base::OnceClosure> subscriptions_loaded_callbacks_;

  // Set to true once all subscriptions have been loaded.
  bool done_loading_subscriptions_ = false;

  // Only defined when |done_loading_subscriptions_| is true.
  bool succeeded_loading_subscriptions_ = false;

  // Instances of this class are currently bound to the service worker core
  // thread, because they call ServiceWorkerContextWrapper methods that are
  // restricted to that thread. However, the class implementation itself is
  // thread-friendly, so it only checks that methods are called on the same
  // sequence.
  SEQUENCE_CHECKER(sequence_checker_);

  // Supports having the manager destroyed while waiting for disk I/O.
  base::WeakPtrFactory<CookieStoreManager> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CookieStoreManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_COOKIE_STORE_COOKIE_STORE_MANAGER_H_
