// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/device_bound_session_manager.h"

#include "base/barrier_callback.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/functional/callback_helpers.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/cookies/canonical_cookie.h"
#include "net/device_bound_sessions/session_params.h"
#include "net/device_bound_sessions/session_service.h"
#include "services/network/cookie_manager.h"
#include "services/network/public/mojom/clear_data_filter.mojom.h"

namespace network {

// static
std::unique_ptr<DeviceBoundSessionManager> DeviceBoundSessionManager::Create(
    net::device_bound_sessions::SessionService* service,
    CookieManager* cookie_manager) {
  if (!service) {
    return nullptr;
  }

  return base::WrapUnique(
      new DeviceBoundSessionManager(service, cookie_manager));
}

DeviceBoundSessionManager::DeviceBoundSessionManager(
    net::device_bound_sessions::SessionService* service,
    CookieManager* cookie_manager)
    : service_(service), cookie_manager_(cookie_manager) {}

DeviceBoundSessionManager::~DeviceBoundSessionManager() = default;

void DeviceBoundSessionManager::AddReceiver(
    mojo::PendingReceiver<mojom::DeviceBoundSessionManager> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void DeviceBoundSessionManager::GetAllSessions(
    DeviceBoundSessionManager::GetAllSessionsCallback callback) {
  service_->GetAllSessionsAsync(std::move(callback));
}

void DeviceBoundSessionManager::DeleteSession(
    net::device_bound_sessions::DeletionReason reason,
    const net::device_bound_sessions::SessionKey& session_key) {
  service_->DeleteSessionAndNotify(
      reason,
      {session_key.site,
       net::device_bound_sessions::Session::Id(session_key.id)},
      base::NullCallback());
}

void DeviceBoundSessionManager::DeleteAllSessions(
    net::device_bound_sessions::DeletionReason reason,
    std::optional<base::Time> created_after_time,
    std::optional<base::Time> created_before_time,
    network::mojom::ClearDataFilterPtr filter,
    base::OnceClosure completion_callback) {
  base::RepeatingCallback<bool(const url::Origin&, const net::SchemefulSite&)>
      origin_and_site_matcher;
  if (filter) {
    origin_and_site_matcher = base::BindRepeating(
        // TODO(crbug.com/384437667): Consolidate ClearDataFilter matching logic
        [](const mojom::ClearDataFilter& filter, const url::Origin& origin,
           const net::SchemefulSite& site) {
          bool is_match = base::Contains(filter.origins, origin);
          if (!is_match && !filter.domains.empty()) {
            const std::string etld1_for_origin =
                net::registry_controlled_domains::GetDomainAndRegistry(
                    site.GetURL(), net::registry_controlled_domains::
                                       INCLUDE_PRIVATE_REGISTRIES);
            is_match = base::Contains(filter.domains, etld1_for_origin);
          }

          switch (filter.type) {
            case mojom::ClearDataFilter::Type::KEEP_MATCHES:
              return !is_match;
            case mojom::ClearDataFilter::Type::DELETE_MATCHES:
              return is_match;
          }
        },
        *filter);
  }

  service_->DeleteAllSessions(reason, created_after_time, created_before_time,
                              origin_and_site_matcher,
                              std::move(completion_callback));
}

DeviceBoundSessionManager::ObserverRegistration::ObserverRegistration() =
    default;
DeviceBoundSessionManager::ObserverRegistration::~ObserverRegistration() =
    default;

void DeviceBoundSessionManager::AddObserver(
    const GURL& url,
    mojo::PendingRemote<network::mojom::DeviceBoundSessionAccessObserver>
        observer) {
  auto registration = std::make_unique<ObserverRegistration>();
  registration->remote.Bind(std::move(observer));
  registration->remote.set_disconnect_handler(
      base::BindOnce(&DeviceBoundSessionManager::RemoveObserver,
                     // base::Unretained is safe because `this` owns
                     // `registration`, which owns the callback.
                     base::Unretained(this), registration.get()));
  registration->subscription = service_->AddObserver(
      url,
      base::BindRepeating(&network::mojom::DeviceBoundSessionAccessObserver::
                              OnDeviceBoundSessionAccessed,
                          base::Unretained(registration->remote.get())));
  observer_registrations_.push_back(std::move(registration));
}

void DeviceBoundSessionManager::CreateBoundSessions(
    std::vector<net::device_bound_sessions::SessionParams> params,
    const std::vector<uint8_t>& wrapped_key,
    const std::vector<net::CanonicalCookie>& cookies_to_set,
    const net::CookieOptions& cookie_options,
    CreateBoundSessionsCallback callback) {
  auto barrier_callback = base::BarrierCallback<
      net::device_bound_sessions::SessionError::ErrorType>(
      params.size(),
      base::BindOnce(&DeviceBoundSessionManager::OnCreateBoundSessionsAdded,
                     weak_factory_.GetWeakPtr(), cookies_to_set, cookie_options,
                     std::move(callback)));

  for (net::device_bound_sessions::SessionParams& param : params) {
    GURL fetcher_url = param.fetcher_url;
    service_->AddSession(net::SchemefulSite(fetcher_url), std::move(param),
                         wrapped_key, barrier_callback);
  }
}

void DeviceBoundSessionManager::OnCreateBoundSessionsAdded(
    const std::vector<net::CanonicalCookie>& cookies_to_set,
    const net::CookieOptions& cookie_options,
    CreateBoundSessionsCallback callback,
    std::vector<net::device_bound_sessions::SessionError::ErrorType>
        session_results) {
  if (cookies_to_set.empty()) {
    std::move(callback).Run(std::move(session_results),
                            std::vector<net::CookieInclusionStatus>());
    return;
  }

  auto final_callback = base::BindOnce(
      [](CreateBoundSessionsCallback callback,
         std::vector<net::device_bound_sessions::SessionError::ErrorType>
             create_session_results,
         std::vector<net::CookieAccessResult> results) {
        std::vector<net::CookieInclusionStatus> cookie_results;
        cookie_results.reserve(results.size());
        for (auto& result : results) {
          cookie_results.push_back(std::move(result.status));
        }
        std::move(callback).Run(std::move(create_session_results),
                                std::move(cookie_results));
      },
      std::move(callback), std::move(session_results));

  auto barrier_callback = base::BarrierCallback<net::CookieAccessResult>(
      cookies_to_set.size(), std::move(final_callback));

  for (const auto& cookie : cookies_to_set) {
    cookie_manager_->SetCanonicalCookie(
        cookie, net::cookie_util::SimulatedCookieSource(cookie, "https"),
        cookie_options, barrier_callback);
  }
}

void DeviceBoundSessionManager::RemoveObserver(
    DeviceBoundSessionManager::ObserverRegistration* registration) {
  std::erase_if(observer_registrations_, base::MatchesUniquePtr(registration));
}

}  // namespace network
