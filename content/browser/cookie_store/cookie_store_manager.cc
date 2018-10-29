// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cookie_store/cookie_store_manager.h"

#include <utility>

#include "base/optional.h"
#include "content/browser/cookie_store/cookie_change_subscriptions.pb.h"
#include "content/browser/service_worker/embedded_worker_status.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_metrics.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/public/browser/browser_context.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_event_status.mojom.h"
#include "url/gurl.h"

namespace content {

namespace {

// ServiceWorkerStorage user data key for cookie change subscriptions.
const char kSubscriptionsUserKey[] = "cookie_store_subscriptions";

// Handles the result of ServiceWorkerContextWrapper::StoreRegistrationUserData.
void HandleStoreRegistrationUserDataStatus(
    blink::ServiceWorkerStatusCode status) {
  // The current implementation does not have a good way to handle errors in
  // StoreRegistrationUserData. Cookie change subscriptions have been added to
  // the registration during the install event, so it's too late to surface the
  // error to the renderer. The registration has already been persisted, and the
  // Service Worker is likely active by now.
  DLOG_IF(ERROR, status != blink::ServiceWorkerStatusCode::kOk)
      << "StoreRegistrationUserData failed";
}

}  // namespace

CookieStoreManager::CookieStoreManager(
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context)
    : service_worker_context_(std::move(service_worker_context)),
      cookie_change_listener_binding_(this),
      registration_user_data_key_(kSubscriptionsUserKey),
      weak_factory_(this) {
  service_worker_context_->AddObserver(this);
}

CookieStoreManager::~CookieStoreManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  service_worker_context_->RemoveObserver(this);
}

void CookieStoreManager::CreateService(blink::mojom::CookieStoreRequest request,
                                       const url::Origin& origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  bindings_.AddBinding(std::make_unique<CookieStoreHost>(this, origin),
                       std::move(request));
}

void CookieStoreManager::LoadAllSubscriptions(
    base::OnceCallback<void(bool)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK(!done_loading_subscriptions_) << __func__ << " already called";

  service_worker_context_->GetUserDataForAllRegistrations(
      registration_user_data_key_,
      base::BindOnce(&CookieStoreManager::ProcessOnDiskSubscriptions,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void CookieStoreManager::ListenToCookieChanges(
    ::network::mojom::CookieManagerPtr cookie_manager,
    base::OnceCallback<void(bool)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK(!cookie_manager_) << __func__ << " already called";
  cookie_manager_ = std::move(cookie_manager);

  DCHECK(!cookie_change_listener_binding_.is_bound());
  ::network::mojom::CookieChangeListenerPtr cookie_change_listener;
  cookie_change_listener_binding_.Bind(
      mojo::MakeRequest(&cookie_change_listener));

  // TODO(pwnall): Switch to an API with subscription confirmation.
  cookie_manager_->AddGlobalChangeListener(std::move(cookie_change_listener));
  std::move(callback).Run(true);
}

void CookieStoreManager::ProcessOnDiskSubscriptions(
    base::OnceCallback<void(bool)> load_callback,
    const std::vector<std::pair<int64_t, std::string>>& user_data,
    blink::ServiceWorkerStatusCode status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK(!done_loading_subscriptions_) << __func__ << " already called";
  done_loading_subscriptions_ = true;

  if (status != blink::ServiceWorkerStatusCode::kOk) {
    DidLoadAllSubscriptions(false, std::move(load_callback));
    return;
  }

  DCHECK(subscriptions_by_registration_.empty());
  subscriptions_by_registration_.reserve(user_data.size());
  bool load_success = true;
  for (const auto& pair : user_data) {
    int64_t service_worker_registration_id = pair.first;
    const std::string& proto_string = pair.second;

    base::Optional<std::vector<CookieChangeSubscription>> subscriptions_opt =
        CookieChangeSubscription::DeserializeVector(
            proto_string, service_worker_registration_id);
    if (!subscriptions_opt.has_value()) {
      load_success = false;
      continue;
    }

    ActivateSubscriptions(&subscriptions_opt.value());
    DCHECK(
        !subscriptions_by_registration_.count(service_worker_registration_id));
    subscriptions_by_registration_.emplace(
        std::move(service_worker_registration_id),
        std::move(subscriptions_opt).value());
  }

  DidLoadAllSubscriptions(load_success, std::move(load_callback));
}

void CookieStoreManager::DidLoadAllSubscriptions(
    bool succeeded,
    base::OnceCallback<void(bool)> load_callback) {
  DCHECK(done_loading_subscriptions_);
  succeeded_loading_subscriptions_ = succeeded;

  for (auto& callback : subscriptions_loaded_callbacks_)
    std::move(callback).Run();
  subscriptions_loaded_callbacks_.clear();

  std::move(load_callback).Run(succeeded);
}

void CookieStoreManager::AppendSubscriptions(
    int64_t service_worker_registration_id,
    const url::Origin& origin,
    std::vector<blink::mojom::CookieChangeSubscriptionPtr> mojo_subscriptions,
    blink::mojom::CookieStore::AppendSubscriptionsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!done_loading_subscriptions_) {
    subscriptions_loaded_callbacks_.emplace_back(base::BindOnce(
        &CookieStoreManager::AppendSubscriptions, weak_factory_.GetWeakPtr(),
        service_worker_registration_id, origin, std::move(mojo_subscriptions),
        std::move(callback)));
    return;
  }

  if (!succeeded_loading_subscriptions_) {
    std::move(callback).Run(false);
    return;
  }

  // GetLiveRegistration() is sufficient here (as opposed to a flavor of
  // FindRegistration()) because AppendSubscriptions is only called from the
  // implementation of the Cookie Store API, which is exposed to
  // ServiceWorkerGlobalScope. ServiceWorkerGlobalScope references the
  // service worker's registration via a ServiceWorkerRegistration JavaScript
  // object, so the registration is guaranteed to be live while the service
  // worker is executing.
  //
  // It is possible for the service worker to get killed while this API call is
  // in progress, for example, if the service worker code exceeds an event
  // handling time limit. In that case, the return value will not be observed,
  // so a false negative is acceptable.
  ServiceWorkerRegistration* service_worker_registration =
      service_worker_context_->GetLiveRegistration(
          service_worker_registration_id);
  if (!service_worker_registration ||
      !origin.IsSameOriginWith(
          url::Origin::Create(service_worker_registration->scope()))) {
    // This error case is a good fit for mojo::ReportBadMessage(), because the
    // renderer has passed an invalid registration ID. However, the code here
    // might run without a mojo call context, if the original call was delayed
    // while loading on-disk subscription data.
    //
    // While it would be possible to have two code paths for the two situations,
    // the extra complexity doesn't seem warranted for the limited debuggig
    // benefits provided by mojo::ReportBadMessage.
    std::move(callback).Run(false);
    return;
  }

  // TODO(crbug.com/843079): This check incorrectly allows an active service
  //                         worker version to call the API, if another version
  //                         is installing at the same time.
  if (!service_worker_registration->installing_version()) {
    // A service worker's cookie change subscriptions can only be modified while
    // the service worker's install event is handled.
    std::move(callback).Run(false);
    return;
  }

  if (mojo_subscriptions.empty()) {
    // Empty subscriptions are special-cased so we never have to serialize an
    // empty array of subscriptions. This is advantageous because the protobuf
    // serialization of an empty array is the empty string, which is also used
    // by the convenience protobuf serialization API to signal serialization
    // failure. So, supporting serializing an empty array would mean we can't
    // use the convenience serialization API.
    std::move(callback).Run(true);
    return;
  }

  std::vector<CookieChangeSubscription> new_subscriptions =
      CookieChangeSubscription::FromMojoVector(
          std::move(mojo_subscriptions), service_worker_registration->id());
  DCHECK(!new_subscriptions.empty());

  auto old_subscriptions_it =
      subscriptions_by_registration_.find(service_worker_registration_id);
  if (old_subscriptions_it == subscriptions_by_registration_.end()) {
    subscriptions_by_registration_.emplace(service_worker_registration_id,
                                           std::move(new_subscriptions));
    std::move(callback).Run(true);
    return;
  }

  std::vector<CookieChangeSubscription>& old_subscriptions =
      old_subscriptions_it->second;
  old_subscriptions.reserve(old_subscriptions.size() +
                            new_subscriptions.size());
  for (auto& new_subscription : new_subscriptions)
    old_subscriptions.emplace_back(std::move(new_subscription));

  std::move(callback).Run(true);
}

void CookieStoreManager::GetSubscriptions(
    int64_t service_worker_registration_id,
    const url::Origin& origin,
    blink::mojom::CookieStore::GetSubscriptionsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!done_loading_subscriptions_) {
    subscriptions_loaded_callbacks_.emplace_back(base::BindOnce(
        &CookieStoreManager::GetSubscriptions, weak_factory_.GetWeakPtr(),
        service_worker_registration_id, origin, std::move(callback)));
    return;
  }

  if (!succeeded_loading_subscriptions_) {
    std::move(callback).Run(
        std::vector<blink::mojom::CookieChangeSubscriptionPtr>(), false);
    return;
  }

  auto it = subscriptions_by_registration_.find(service_worker_registration_id);
  if (it == subscriptions_by_registration_.end() || it->second.empty()) {
    std::move(callback).Run(
        std::vector<blink::mojom::CookieChangeSubscriptionPtr>(), true);
    return;
  }

  const url::Origin& first_origin = url::Origin::Create(it->second[0].url());
#if DCHECK_IS_ON()
  for (const auto& subscription : it->second) {
    DCHECK(
        first_origin.IsSameOriginWith(url::Origin::Create(subscription.url())))
        << "Service worker's change subscriptions don't have the same origin";
  }
#endif  // DCHECK_IS_ON()

  if (!origin.IsSameOriginWith(first_origin)) {
    // This error case is a good fit for mojo::ReportBadMessage(), because the
    // renderer has passed an invalid registration ID. However, the code here
    // might run without a mojo call context, if the original call was delayed
    // while loading on-disk subscription data.
    //
    // While it would be possible to have two code paths for the two situations,
    // the extra complexity doesn't seem warranted for the limited debuggig
    // benefits provided by mojo::ReportBadMessage.
    std::move(callback).Run(
        std::vector<blink::mojom::CookieChangeSubscriptionPtr>(), false);
    return;
  }

  std::move(callback).Run(CookieChangeSubscription::ToMojoVector(it->second),
                          true);
}

void CookieStoreManager::OnNewLiveRegistration(
    int64_t service_worker_registration_id,
    const GURL& pattern) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void CookieStoreManager::OnRegistrationStored(
    int64_t service_worker_registration_id,
    const GURL& pattern) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Waiting for the on-disk subscriptions to be loaded ensures that the
  // registration's subscriptions aren't activated twice. Without waiting,
  // there's a risk that LoadAllSubscriptions() sees the result of the
  // StoreRegistrationUserData() call below.
  if (!done_loading_subscriptions_) {
    subscriptions_loaded_callbacks_.emplace_back(base::BindOnce(
        &CookieStoreManager::OnRegistrationStored, weak_factory_.GetWeakPtr(),
        service_worker_registration_id, pattern));
    return;
  }

  auto it = subscriptions_by_registration_.find(service_worker_registration_id);
  if (it == subscriptions_by_registration_.end())
    return;

  ActivateSubscriptions(&it->second);

  std::string subscriptions_data =
      CookieChangeSubscription::SerializeVector(it->second);
  DCHECK(!subscriptions_data.empty())
      << "Failed to create cookie change subscriptions protobuf";

  service_worker_context_->StoreRegistrationUserData(
      service_worker_registration_id, pattern.GetOrigin(),
      std::vector<std::pair<std::string, std::string>>(
          {{registration_user_data_key_, subscriptions_data}}),
      base::BindOnce(&HandleStoreRegistrationUserDataStatus));
}

void CookieStoreManager::OnRegistrationDeleted(
    int64_t service_worker_registration_id,
    const GURL& pattern) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Waiting for the on-disk subscriptions to be loaded ensures that the
  // registration's subscriptions are removed. Without waiting, there's a risk
  // that a registration's subscriptions will finish loading (and thus remain
  // active) right after this function runs.
  if (!done_loading_subscriptions_) {
    subscriptions_loaded_callbacks_.emplace_back(base::BindOnce(
        &CookieStoreManager::OnRegistrationDeleted, weak_factory_.GetWeakPtr(),
        service_worker_registration_id, pattern));
    return;
  }

  auto it = subscriptions_by_registration_.find(service_worker_registration_id);
  if (it == subscriptions_by_registration_.end())
    return;

  DeactivateSubscriptions(&it->second);
  subscriptions_by_registration_.erase(it);
}

void CookieStoreManager::ActivateSubscriptions(
    std::vector<CookieChangeSubscription>* subscriptions) {
  if (subscriptions->empty())
    return;

  // Service workers can only observe changes to cookies for URLs under their
  // scope. This means all the URLs that the worker is observing must map to the
  // same domain key (eTLD+1).
  //
  // TODO(pwnall): This is the same as implementation as
  //               net::CookieMonsterChangeDispatcher::DomainKey. Extract that
  //               implementation into net/cookies.cookie_util.h and call it.
  std::string url_key = net::registry_controlled_domains::GetDomainAndRegistry(

      (*subscriptions)[0].url(),
      net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  base::LinkedList<CookieChangeSubscription>& url_key_subscriptions_list =
      subscriptions_by_url_key_[url_key];

  for (auto& subscription : *subscriptions) {
    DCHECK(!subscription.next() && !subscription.previous())
        << "Subscription passed to " << __func__ << " already activated";
    DCHECK_EQ(url_key,
              net::registry_controlled_domains::GetDomainAndRegistry(
                  subscription.url(),
                  net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES))
        << __func__ << " subscriptions belong to different registrations";
    url_key_subscriptions_list.Append(&subscription);
  }
}

void CookieStoreManager::DeactivateSubscriptions(
    std::vector<CookieChangeSubscription>* subscriptions) {
  if (subscriptions->empty())
    return;

  // Service workers can only observe changes to cookies for URLs under their
  // scope. This means all the URLs that the worker is observing must map to the
  // same domain key (eTLD+1).
  //
  // TODO(pwnall): This has the same implementation as
  //               net::CookieMonsterChangeDispatcher::DomainKey. Extract that
  //               implementation into net/cookies.cookie_util.h and call it.
  std::string url_key = net::registry_controlled_domains::GetDomainAndRegistry(
      (*subscriptions)[0].url(),
      net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  for (auto& subscription : *subscriptions) {
    DCHECK(subscription.next() && subscription.previous())
        << "Subscription passed to " << __func__ << " not previously activated";
    DCHECK_EQ(url_key,
              net::registry_controlled_domains::GetDomainAndRegistry(
                  subscription.url(),
                  net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES))
        << __func__ << " subscriptions belong to different registrations";
    subscription.RemoveFromList();
  }
  auto it = subscriptions_by_url_key_.find(url_key);
  DCHECK(it != subscriptions_by_url_key_.end());
  if (it->second.empty())
    subscriptions_by_url_key_.erase(it);
}

void CookieStoreManager::OnStorageWiped() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Waiting for the on-disk subscriptions to be loaded ensures that all
  // subscriptions are removed. Without waiting, there's a risk that some
  // subscriptions will finish loading (and thus remain active) after this
  // function runs.
  if (!done_loading_subscriptions_) {
    subscriptions_loaded_callbacks_.emplace_back(base::BindOnce(
        &CookieStoreManager::OnStorageWiped, weak_factory_.GetWeakPtr()));
    return;
  }

  subscriptions_by_url_key_.clear();
  subscriptions_by_registration_.clear();
}

void CookieStoreManager::OnCookieChange(
    const net::CanonicalCookie& cookie,
    ::network::mojom::CookieChangeCause cause) {
  // Waiting for on-disk subscriptions to be loaded ensures that changes are
  // delivered to all service workers that subscribed to them in previous
  // browser sessions. Without waiting, workers might miss cookie changes.
  if (!done_loading_subscriptions_) {
    subscriptions_loaded_callbacks_.emplace_back(
        base::BindOnce(&CookieStoreManager::OnCookieChange,
                       weak_factory_.GetWeakPtr(), cookie, cause));
    return;
  }

  // Compute the list of service workers interested in this change. A worker
  // might have multiple subscriptions that cover this change, but should still
  // receive a single change event.
  // TODO(pwnall): This has same as implementation as
  //               net::CookieMonsterChangeDispatcher::DomainKey. Extract that
  //               implementation into net/cookies.cookie_util.h and call it.
  std::string url_key = net::registry_controlled_domains::GetDomainAndRegistry(
      cookie.Domain(),
      net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  auto it = subscriptions_by_url_key_.find(url_key);
  if (it == subscriptions_by_url_key_.end())
    return;
  std::set<int64_t> interested_registration_ids;
  const base::LinkedList<CookieChangeSubscription>& subscriptions = it->second;
  for (const base::LinkNode<CookieChangeSubscription>* node =
           subscriptions.head();
       node != subscriptions.end(); node = node->next()) {
    const CookieChangeSubscription* subscription = node->value();
    if (subscription->ShouldObserveChangeTo(cookie)) {
      interested_registration_ids.insert(
          subscription->service_worker_registration_id());
    }
  }

  // Dispatch the change to interested workers.
  for (int64_t registration_id : interested_registration_ids) {
    service_worker_context_->FindReadyRegistrationForIdOnly(
        registration_id,
        base::BindOnce(
            [](base::WeakPtr<CookieStoreManager> manager,
               const net::CanonicalCookie& cookie,
               ::network::mojom::CookieChangeCause cause,
               blink::ServiceWorkerStatusCode find_status,
               scoped_refptr<ServiceWorkerRegistration> registration) {
              if (find_status != blink::ServiceWorkerStatusCode::kOk)
                return;

              DCHECK(registration);
              if (!manager)
                return;
              manager->DispatchChangeEvent(std::move(registration), cookie,
                                           cause);
            },
            weak_factory_.GetWeakPtr(), cookie, cause));
  }
}

void CookieStoreManager::DispatchChangeEvent(
    scoped_refptr<ServiceWorkerRegistration> registration,
    const net::CanonicalCookie& cookie,
    ::network::mojom::CookieChangeCause cause) {
  scoped_refptr<ServiceWorkerVersion> active_version =
      registration->active_version();
  if (active_version->running_status() != EmbeddedWorkerStatus::RUNNING) {
    active_version->RunAfterStartWorker(
        ServiceWorkerMetrics::EventType::COOKIE_CHANGE,
        base::BindOnce(&CookieStoreManager::DidStartWorkerForChangeEvent,
                       weak_factory_.GetWeakPtr(), std::move(registration),
                       cookie, cause));
    return;
  }

  int request_id = active_version->StartRequest(
      ServiceWorkerMetrics::EventType::COOKIE_CHANGE, base::DoNothing());

  active_version->endpoint()->DispatchCookieChangeEvent(
      cookie, cause, active_version->CreateSimpleEventCallback(request_id));
}

void CookieStoreManager::DidStartWorkerForChangeEvent(
    scoped_refptr<ServiceWorkerRegistration> registration,
    const net::CanonicalCookie& cookie,
    ::network::mojom::CookieChangeCause cause,
    blink::ServiceWorkerStatusCode start_worker_status) {
  if (start_worker_status != blink::ServiceWorkerStatusCode::kOk)
    return;
  DispatchChangeEvent(std::move(registration), cookie, cause);
}

}  // namespace content
