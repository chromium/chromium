// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cookie_store/cookie_store_manager.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/not_fatal_until.h"
#include "base/ranges/algorithm.h"
#include "base/sequence_checker.h"
#include "content/browser/cookie_store/cookie_change_subscriptions.pb.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_metrics.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/cookies/cookie_partition_key.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "third_party/blink/public/common/service_worker/embedded_worker_status.h"
#include "third_party/blink/public/common/service_worker/service_worker_scope_match.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_event_status.mojom.h"
#include "url/gurl.h"

namespace content {

namespace {

// ServiceWorkerStorage user data key for cookie change subscriptions.
const char kSubscriptionsUserKey[] = "cookie_store_subscriptions";

}  // namespace

CookieStoreManager::CookieStoreManager(
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context)
    : service_worker_context_(std::move(service_worker_context)),
      registration_user_data_key_(kSubscriptionsUserKey) {
  service_worker_context_->AddObserver(this);
}

CookieStoreManager::~CookieStoreManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  service_worker_context_->RemoveObserver(this);
}

void CookieStoreManager::BindReceiver(
    mojo::PendingReceiver<blink::mojom::CookieStore> receiver,
    const blink::StorageKey& storage_key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!network::IsOriginPotentiallyTrustworthy(storage_key.origin())) {
    mojo::ReportBadMessage("Cookie Store access from an insecure origin");
    return;
  }

  receivers_.Add(std::make_unique<CookieStoreHost>(this, storage_key),
                 std::move(receiver));
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
    network::mojom::NetworkContext* network_context,
    base::OnceCallback<void(bool)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!cookie_manager_) << __func__ << " already called";
  DCHECK(!cookie_change_listener_receiver_.is_bound())
      << __func__ << " already called";

  mojo::PendingRemote<::network::mojom::CookieManager> cookie_manager_remote;
  network_context->GetCookieManager(
      cookie_manager_remote.InitWithNewPipeAndPassReceiver());
  cookie_manager_.Bind(std::move(cookie_manager_remote));

  // TODO(pwnall): Switch to an API with subscription confirmation.
  cookie_manager_->AddGlobalChangeListener(
      cookie_change_listener_receiver_.BindNewPipeAndPassRemote());
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

    std::vector<std::unique_ptr<CookieChangeSubscription>> subscriptions =
        CookieChangeSubscription::DeserializeVector(
            proto_string, service_worker_registration_id);
    if (subscriptions.empty()) {
      load_success = false;
      continue;
    }

    ActivateSubscriptions(subscriptions);
    DCHECK(
        !subscriptions_by_registration_.count(service_worker_registration_id));
    subscriptions_by_registration_.emplace(
        std::move(service_worker_registration_id), std::move(subscriptions));
  }

  DidLoadAllSubscriptions(load_success, std::move(load_callback));
}

void CookieStoreManager::DidLoadAllSubscriptions(
    bool succeeded,
    base::OnceCallback<void(bool)> load_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(done_loading_subscriptions_);
  succeeded_loading_subscriptions_ = succeeded;

  for (auto& callback : subscriptions_loaded_callbacks_)
    std::move(callback).Run();
  subscriptions_loaded_callbacks_.clear();

  std::move(load_callback).Run(succeeded);
}

void CookieStoreManager::AddSubscriptions(
    int64_t service_worker_registration_id,
    const blink::StorageKey& storage_key,
    std::vector<blink::mojom::CookieChangeSubscriptionPtr> mojo_subscriptions,
    mojo::ReportBadMessageCallback bad_message_callback,
    blink::mojom::CookieStore::AddSubscriptionsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!done_loading_subscriptions_) {
    subscriptions_loaded_callbacks_.push_back(base::BindOnce(
        &CookieStoreManager::AddSubscriptions, weak_factory_.GetWeakPtr(),
        service_worker_registration_id, storage_key,
        std::move(mojo_subscriptions), std::move(bad_message_callback),
        std::move(callback)));
    return;
  }

  if (!succeeded_loading_subscriptions_) {
    std::move(callback).Run(false);
    return;
  }

  // GetLiveRegistration() is sufficient here, as opposed to a flavor of
  // FindRegistration(), because we know the registration must be alive.
  //
  // blink::CookieStoreManager calls AddSubscription() and stays alive until the
  // async call completes. blink::CookieStoreManager hangs onto the Blink side
  // of the Service Worker's registration. So, the registration will be live if
  // the call's result is received.
  scoped_refptr<ServiceWorkerRegistration> service_worker_registration =
      service_worker_context_->GetLiveRegistration(
          service_worker_registration_id);
  // If the calling blink::CookieStoreManager instance goes away (for example,
  // it had to wait for the database load to complete, and that took too long),
  // the result of this call won't be received, so it's acceptable to fail it.
  if (!service_worker_registration ||
      !service_worker_registration->active_version()) {
    std::move(callback).Run(false);
    return;
  }

  if (storage_key != service_worker_registration->key()) {
    std::move(bad_message_callback).Run("Invalid service worker");
    std::move(callback).Run(false);
    return;
  }

  // The empty set is special-cased because the code below assumes that the
  // registration's list of subscriptions will end up non-empty.
  if (mojo_subscriptions.empty()) {
    std::move(callback).Run(true);
    return;
  }

  for (const auto& mojo_subscription : mojo_subscriptions) {
    if (!blink::ServiceWorkerScopeMatches(service_worker_registration->scope(),
                                          mojo_subscription->url)) {
      // Blink should have validated subscription URLs against the service
      // worker registration scope. A mismatch here means that the renderer was
      // compromised.
      std::move(bad_message_callback).Run("Invalid subscription URL");
      std::move(callback).Run(false);
      return;
    }
  }

  // If the registration does not exist in the map, the default std::vector()
  // constructor is used to create a new entry. The constructor produces an
  // empty vector, which is exactly what is needed here.
  std::vector<std::unique_ptr<CookieChangeSubscription>>& subscriptions =
      subscriptions_by_registration_[service_worker_registration_id];

  // New subscriptions will be appended past the current vector end.
  size_t old_subscriptions_size = subscriptions.size();

  // The loop consumes the mojo subscriptions, so it can build
  // CookieChangeSubscriptions more efficiently.
  for (auto& mojo_subscription : mojo_subscriptions) {
    auto new_subscription = std::make_unique<CookieChangeSubscription>(
        std::move(mojo_subscription), service_worker_registration->id());

    auto existing_subscription_it = base::ranges::find(
        subscriptions, *new_subscription,
        &std::unique_ptr<CookieChangeSubscription>::operator*);
    if (existing_subscription_it == subscriptions.end())
      subscriptions.push_back(std::move(new_subscription));
  }

  ActivateSubscriptions(
      base::make_span(subscriptions).subspan(old_subscriptions_size));
  StoreSubscriptions(service_worker_registration_id, storage_key, subscriptions,
                     std::move(callback));
}

void CookieStoreManager::RemoveSubscriptions(
    int64_t service_worker_registration_id,
    const blink::StorageKey& storage_key,
    std::vector<blink::mojom::CookieChangeSubscriptionPtr> mojo_subscriptions,
    mojo::ReportBadMessageCallback bad_message_callback,
    blink::mojom::CookieStore::RemoveSubscriptionsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!done_loading_subscriptions_) {
    subscriptions_loaded_callbacks_.push_back(base::BindOnce(
        &CookieStoreManager::RemoveSubscriptions, weak_factory_.GetWeakPtr(),
        service_worker_registration_id, storage_key,
        std::move(mojo_subscriptions), std::move(bad_message_callback),
        std::move(callback)));
    return;
  }

  if (!succeeded_loading_subscriptions_) {
    std::move(callback).Run(false);
    return;
  }

  // GetLiveRegistration() is sufficient here, as opposed to a flavor of
  // FindRegistration(), because we know the registration must be alive.
  //
  // blink::CookieStoreManager calls AddSubscription() and stays alive until the
  // async call completes. blink::CookieStoreManager hangs onto the Blink side
  // of the Service Worker's registration. So, the registration will be live if
  // the call's result is received.
  scoped_refptr<ServiceWorkerRegistration> service_worker_registration =
      service_worker_context_->GetLiveRegistration(
          service_worker_registration_id);
  // If the calling blink::CookieStoreManager instance goes away (for example,
  // it had to wait for the database load to complete, and that took too long),
  // the result of this call won't be received, so it's acceptable to fail it.
  if (!service_worker_registration ||
      !service_worker_registration->active_version()) {
    std::move(callback).Run(false);
    return;
  }

  if (!(storage_key == service_worker_registration->key())) {
    std::move(bad_message_callback).Run("Invalid service worker");
    std::move(callback).Run(false);
    return;
  }

  std::vector<std::unique_ptr<CookieChangeSubscription>> target_subscriptions;
  target_subscriptions.reserve(mojo_subscriptions.size());
  // The loop consumes the mojo subscriptions, so it can build
  // CookieChangeSubscriptions more efficiently.
  for (auto& mojo_subscription : mojo_subscriptions) {
    // This method does not need to check the subscription's URL against the
    // service worker registration's scope. AddSubscription() checks
    // subscription URLs, so the registration does not have any subscriptions
    // with invalid URLs. If a compromised renderer attempts to remove a
    // subscription with an invalid URL, no such subscription will be found, and
    // this method will be a noop.
    target_subscriptions.push_back(std::make_unique<CookieChangeSubscription>(
        std::move(mojo_subscription), service_worker_registration->id()));
  }

  auto all_subscriptions_it =
      subscriptions_by_registration_.find(service_worker_registration_id);
  if (all_subscriptions_it == subscriptions_by_registration_.end()) {
    // Nothing to remove.
    std::move(callback).Run(true);
    return;
  }
  std::vector<std::unique_ptr<CookieChangeSubscription>>& all_subscriptions =
      all_subscriptions_it->second;

  std::vector<std::unique_ptr<CookieChangeSubscription>> removed_subscriptions;
  removed_subscriptions.reserve(target_subscriptions.size());

  std::vector<std::unique_ptr<CookieChangeSubscription>> live_subscriptions;
  // Assume that the application is tracking its subscriptions carefully and
  // each removal will succeed. If the assumption holds, no vector reallocation
  // will be needed.
  if (all_subscriptions.size() > target_subscriptions.size()) {
    live_subscriptions.reserve(all_subscriptions.size() -
                               target_subscriptions.size());
  }

  for (auto& subscription : all_subscriptions) {
    auto target_subscription_it = base::ranges::find(
        target_subscriptions, *subscription,
        &std::unique_ptr<CookieChangeSubscription>::operator*);
    if (target_subscription_it == target_subscriptions.end()) {
      // The subscription is not marked for deletion.
      live_subscriptions.push_back(std::move(subscription));
    } else {
      DCHECK(**target_subscription_it == *subscription);
      removed_subscriptions.push_back(std::move(subscription));
    }
  }
  DeactivateSubscriptions(removed_subscriptions);

  // StoreSubscriptions() needs to be called before updating
  // |subscriptions_by_registration_|, because the update may delete the vector
  // holding the subscriptions.
  StoreSubscriptions(service_worker_registration_id,
                     service_worker_registration->key(), live_subscriptions,
                     std::move(callback));
  if (live_subscriptions.empty()) {
    subscriptions_by_registration_.erase(all_subscriptions_it);
  } else {
    all_subscriptions = std::move(live_subscriptions);
  }
}

void CookieStoreManager::GetSubscriptions(
    int64_t service_worker_registration_id,
    const blink::StorageKey& storage_key,
    mojo::ReportBadMessageCallback bad_message_callback,
    blink::mojom::CookieStore::GetSubscriptionsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!done_loading_subscriptions_) {
    subscriptions_loaded_callbacks_.push_back(base::BindOnce(
        &CookieStoreManager::GetSubscriptions, weak_factory_.GetWeakPtr(),
        service_worker_registration_id, storage_key,
        std::move(bad_message_callback), std::move(callback)));
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

  const GURL& first_url = it->second[0]->url();
#if DCHECK_IS_ON()
  for (const auto& subscription : it->second) {
    DCHECK(url::IsSameOriginWith(first_url, subscription->url()))
        << "Service worker's change subscriptions don't have the same origin";
  }
#endif  // DCHECK_IS_ON()

  if (!storage_key.origin().IsSameOriginWith(first_url)) {
    std::move(bad_message_callback).Run("Invalid service worker");
    std::move(callback).Run(
        std::vector<blink::mojom::CookieChangeSubscriptionPtr>(), false);
    return;
  }

  std::move(callback).Run(CookieChangeSubscription::ToMojoVector(it->second),
                          true);
}

void CookieStoreManager::StoreSubscriptions(
    int64_t service_worker_registration_id,
    const blink::StorageKey& storage_key,
    const std::vector<std::unique_ptr<CookieChangeSubscription>>& subscriptions,
    base::OnceCallback<void(bool)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (subscriptions.empty()) {
    service_worker_context_->ClearRegistrationUserData(
        service_worker_registration_id, {registration_user_data_key_},
        base::BindOnce(
            [](base::OnceCallback<void(bool)> callback,
               blink::ServiceWorkerStatusCode status) {
              std::move(callback).Run(status ==
                                      blink::ServiceWorkerStatusCode::kOk);
            },
            std::move(callback)));
    return;
  }

  std::string subscriptions_data =
      CookieChangeSubscription::SerializeVector(subscriptions);
  DCHECK(!subscriptions_data.empty())
      << "Failed to create cookie change subscriptions protobuf";

  service_worker_context_->StoreRegistrationUserData(
      service_worker_registration_id, storage_key,
      std::vector<std::pair<std::string, std::string>>(
          {{registration_user_data_key_, subscriptions_data}}),
      base::BindOnce(
          [](base::OnceCallback<void(bool)> callback,
             blink::ServiceWorkerStatusCode status) {
            std::move(callback).Run(status ==
                                    blink::ServiceWorkerStatusCode::kOk);
          },
          std::move(callback)));
}

void CookieStoreManager::OnRegistrationDeleted(
    int64_t service_worker_registration_id,
    const GURL& pattern,
    const blink::StorageKey& key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Waiting for the on-disk subscriptions to be loaded ensures that the
  // registration's subscriptions are removed. Without waiting, there's a risk
  // that a registration's subscriptions will finish loading (and thus remain
  // active) right after this function runs.
  if (!done_loading_subscriptions_) {
    subscriptions_loaded_callbacks_.push_back(base::BindOnce(
        &CookieStoreManager::OnRegistrationDeleted, weak_factory_.GetWeakPtr(),
        service_worker_registration_id, pattern, key));
    return;
  }

  auto it = subscriptions_by_registration_.find(service_worker_registration_id);
  if (it == subscriptions_by_registration_.end())
    return;

  DeactivateSubscriptions(it->second);
  subscriptions_by_registration_.erase(it);
}

void CookieStoreManager::ActivateSubscriptions(
    base::span<const std::unique_ptr<CookieChangeSubscription>> subscriptions) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (subscriptions.empty())
    return;

  // Service workers can only observe changes to cookies for URLs under their
  // scope. This means all the URLs that the worker is observing must map to the
  // same domain key (eTLD+1).
  //
  // TODO(pwnall): This is the same as implementation as
  //               net::CookieMonsterChangeDispatcher::DomainKey. Extract that
  //               implementation into net/cookies.cookie_util.h and call it.
  std::string url_key = net::registry_controlled_domains::GetDomainAndRegistry(
      subscriptions[0]->url(),
      net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  base::LinkedList<CookieChangeSubscription>& url_key_subscriptions_list =
      subscriptions_by_url_key_[url_key];

  for (auto& subscription : subscriptions) {
    DCHECK(!subscription->next() && !subscription->previous())
        << "Subscription passed to " << __func__ << " already activated";
    DCHECK_EQ(url_key,
              net::registry_controlled_domains::GetDomainAndRegistry(
                  subscription->url(),
                  net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES))
        << __func__ << " subscriptions belong to different registrations";
    url_key_subscriptions_list.Append(subscription.get());
  }
}

void CookieStoreManager::DeactivateSubscriptions(
    base::span<const std::unique_ptr<CookieChangeSubscription>> subscriptions) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (subscriptions.empty())
    return;

  // Service workers can only observe changes to cookies for URLs under their
  // scope. This means all the URLs that the worker is observing must map to the
  // same domain key (eTLD+1).
  //
  // TODO(pwnall): This has the same implementation as
  //               net::CookieMonsterChangeDispatcher::DomainKey. Extract that
  //               implementation into net/cookies.cookie_util.h and call it.
  std::string url_key = net::registry_controlled_domains::GetDomainAndRegistry(
      subscriptions[0]->url(),
      net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  for (auto& subscription : subscriptions) {
    DCHECK(subscription->next() && subscription->previous())
        << "Subscription passed to " << __func__ << " not previously activated";
    DCHECK_EQ(url_key,
              net::registry_controlled_domains::GetDomainAndRegistry(
                  subscription->url(),
                  net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES))
        << __func__ << " subscriptions belong to different registrations";
    subscription->RemoveFromList();
  }
  auto it = subscriptions_by_url_key_.find(url_key);
  CHECK(it != subscriptions_by_url_key_.end(), base::NotFatalUntil::M130);
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
    subscriptions_loaded_callbacks_.push_back(base::BindOnce(
        &CookieStoreManager::OnStorageWiped, weak_factory_.GetWeakPtr()));
    return;
  }

  subscriptions_by_url_key_.clear();
  subscriptions_by_registration_.clear();
}

void CookieStoreManager::OnCookieChange(const net::CookieChangeInfo& change) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Waiting for on-disk subscriptions to be loaded ensures that changes are
  // delivered to all service workers that subscribed to them in previous
  // browser sessions. Without waiting, workers might miss cookie changes.
  if (!done_loading_subscriptions_) {
    subscriptions_loaded_callbacks_.push_back(
        base::BindOnce(&CookieStoreManager::OnCookieChange,
                       weak_factory_.GetWeakPtr(), change));
    return;
  }

  if (change.cause == net::CookieChangeCause::OVERWRITE) {
    // Cookie overwrites generate an OVERWRITE event with the old cookie data
    // and an INSERTED event with the new cookie data. The Cookie Store API
    // only reports new cookie information, so OVERWRITE events doesn't need to
    // be dispatched to service workers.
    return;
  }

  // Compute the list of service workers interested in this change. A worker
  // might have multiple subscriptions that cover this change, but should still
  // receive a single change event.
  // TODO(pwnall): This has same as implementation as
  //               net::CookieMonsterChangeDispatcher::DomainKey. Extract that
  //               implementation into net/cookies.cookie_util.h and call it.
  std::string url_key = net::registry_controlled_domains::GetDomainAndRegistry(
      change.cookie.Domain(),
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
    if (subscription->ShouldObserveChangeTo(
            change.cookie, change.access_result.access_semantics)) {
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
               BrowserContext* browser_context,
               ContentBrowserClient* content_browser_client,
               const net::CookieChangeInfo& change,
               blink::ServiceWorkerStatusCode find_status,
               scoped_refptr<ServiceWorkerRegistration> registration) {
              if (find_status != blink::ServiceWorkerStatusCode::kOk)
                return;

              DCHECK(registration);
              if (!manager)
                return;

              if (content_browser_client && !change.cookie.IsPartitioned() &&
                  !content_browser_client->IsFullCookieAccessAllowed(
                      browser_context, /*web_contents=*/nullptr,
                      registration->scope(), registration->key())) {
                return;
              }

              // If the change is for a partition cookie, we check that its
              // partition key matches the StorageKey's top-level site.
              if (auto cookie_partition_key =
                      registration->key().ToCookiePartitionKey()) {
                if (change.cookie.IsPartitioned() &&
                    change.cookie.PartitionKey() != cookie_partition_key) {
                  return;
                }
                // If the cookie partition key for the worker has a nonce, then
                // only partitioned cookies should be visible.
                if (net::CookiePartitionKey::HasNonce(cookie_partition_key) &&
                    !change.cookie.IsPartitioned()) {
                  return;
                }
              }

              if (registration->key().IsThirdPartyContext() &&
                  !change.cookie.IsEffectivelySameSiteNone()) {
                return;
              }

              // TODO(crbug.com/40063772): Third-party partitioned workers
              // should not have access to unpartitioned state when third-party
              // cookie blocking is on.
              // TODO(crbug.com/40063772): Should RSA grant unpartitioned cookie
              // access?

              manager->DispatchChangeEvent(std::move(registration), change);
            },
            weak_factory_.GetWeakPtr(),
            service_worker_context_->browser_context(),
            GetContentClient()->browser(), change));
  }
}

// static
void CookieStoreManager::BindReceiverForFrame(
    RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::CookieStore> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(render_frame_host);
  RenderProcessHost* render_process_host = render_frame_host->GetProcess();
  DCHECK(render_process_host);

  StoragePartitionImpl* storage_partition = static_cast<StoragePartitionImpl*>(
      render_process_host->GetStoragePartition());

  RenderFrameHostImpl* render_frame_host_impl =
      static_cast<RenderFrameHostImpl*>(render_frame_host);
  storage_partition->GetCookieStoreManager()->BindReceiver(
      std::move(receiver), render_frame_host_impl->GetStorageKey());
}

// static
void CookieStoreManager::BindReceiverForWorker(
    const ServiceWorkerVersionBaseInfo& info,
    mojo::PendingReceiver<blink::mojom::CookieStore> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RenderProcessHost* render_process_host =
      RenderProcessHost::FromID(info.process_id);
  if (render_process_host == nullptr)
    return;

  StoragePartitionImpl* storage_partition = static_cast<StoragePartitionImpl*>(
      render_process_host->GetStoragePartition());
  storage_partition->GetCookieStoreManager()->BindReceiver(std::move(receiver),
                                                           info.storage_key);
}

void CookieStoreManager::DispatchChangeEvent(
    scoped_refptr<ServiceWorkerRegistration> registration,
    const net::CookieChangeInfo& change) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  scoped_refptr<ServiceWorkerVersion> active_version =
      registration->active_version();
  if (active_version->running_status() !=
      blink::EmbeddedWorkerStatus::kRunning) {
    active_version->RunAfterStartWorker(
        ServiceWorkerMetrics::EventType::COOKIE_CHANGE,
        base::BindOnce(&CookieStoreManager::DidStartWorkerForChangeEvent,
                       weak_factory_.GetWeakPtr(), std::move(registration),
                       change));
    return;
  }

  int request_id = active_version->StartRequest(
      ServiceWorkerMetrics::EventType::COOKIE_CHANGE, base::DoNothing());

  active_version->endpoint()->DispatchCookieChangeEvent(
      change, active_version->CreateSimpleEventCallback(request_id));
}

void CookieStoreManager::DidStartWorkerForChangeEvent(
    scoped_refptr<ServiceWorkerRegistration> registration,
    const net::CookieChangeInfo& change,
    blink::ServiceWorkerStatusCode start_worker_status) {
  if (start_worker_status != blink::ServiceWorkerStatusCode::kOk)
    return;
  DispatchChangeEvent(std::move(registration), change);
}

}  // namespace content
