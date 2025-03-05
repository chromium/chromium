// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/device_bound_session_manager.h"

#include "base/containers/unique_ptr_adapters.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/device_bound_sessions/session_service.h"
#include "services/network/public/mojom/clear_data_filter.mojom.h"

namespace network {

// static
std::unique_ptr<DeviceBoundSessionManager> DeviceBoundSessionManager::Create(
    net::device_bound_sessions::SessionService* service) {
  if (!service) {
    return nullptr;
  }

  return base::WrapUnique(new DeviceBoundSessionManager(service));
}

DeviceBoundSessionManager::DeviceBoundSessionManager(
    net::device_bound_sessions::SessionService* service)
    : service_(service) {}

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
    const net::device_bound_sessions::SessionKey& session_key) {
  service_->DeleteSessionAndNotify(
      session_key.site, net::device_bound_sessions::Session::Id(session_key.id),
      base::NullCallback());
}

void DeviceBoundSessionManager::DeleteAllSessions(
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

  service_->DeleteAllSessions(created_after_time, created_before_time,
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

void DeviceBoundSessionManager::RemoveObserver(
    DeviceBoundSessionManager::ObserverRegistration* registration) {
  std::erase_if(observer_registrations_, base::MatchesUniquePtr(registration));
}

}  // namespace network
