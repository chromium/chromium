// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_DEVICE_BOUND_SESSION_MANAGER_H_
#define SERVICES_NETWORK_DEVICE_BOUND_SESSION_MANAGER_H_

#include <vector>

#include "base/callback_list.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/device_bound_sessions/session_error.h"
#include "services/network/public/mojom/device_bound_sessions.mojom.h"

namespace net::device_bound_sessions {
class SessionService;
struct SessionKey;
}  // namespace net::device_bound_sessions

namespace network {

class CookieManager;

class COMPONENT_EXPORT(NETWORK_SERVICE) DeviceBoundSessionManager
    : public mojom::DeviceBoundSessionManager {
 public:
  static std::unique_ptr<DeviceBoundSessionManager> Create(
      net::device_bound_sessions::SessionService* service,
      CookieManager* cookie_manager);

  ~DeviceBoundSessionManager() override;

  void AddReceiver(
      mojo::PendingReceiver<mojom::DeviceBoundSessionManager> receiver);

  // network::mojom::DeviceBoundSessionManager
  void GetAllSessions(GetAllSessionsCallback callback) override;
  void DeleteSession(
      net::device_bound_sessions::DeletionReason reason,
      const net::device_bound_sessions::SessionKey& session_key) override;
  void DeleteAllSessions(net::device_bound_sessions::DeletionReason reason,
                         std::optional<base::Time> created_after_time,
                         std::optional<base::Time> created_before_time,
                         network::mojom::ClearDataFilterPtr filter,
                         base::OnceClosure completion_callback) override;
  void AddObserver(
      const GURL& url,
      mojo::PendingRemote<network::mojom::DeviceBoundSessionAccessObserver>
          observer) override;
  void CreateBoundSessions(
      std::vector<net::device_bound_sessions::SessionParams> params,
      const std::vector<uint8_t>& wrapped_key,
      const std::vector<net::CanonicalCookie>& cookies_to_set,
      const net::CookieOptions& cookie_options,
      CreateBoundSessionsCallback callback) override;

 private:
  // State associated with a DeviceBoundSessionAccessObserver.
  struct ObserverRegistration {
    ObserverRegistration();
    ~ObserverRegistration();

    // Mojo interface
    mojo::Remote<network::mojom::DeviceBoundSessionAccessObserver> remote;

    // Subscription for inclusion in the SessionService's CallbackList.
    base::ScopedClosureRunner subscription;
  };

  explicit DeviceBoundSessionManager(
      net::device_bound_sessions::SessionService* service,
      CookieManager* cookie_manager);

  // Remove an observer by its registration.
  void RemoveObserver(ObserverRegistration* registration);

  void OnCreateBoundSessionsAdded(
      const std::vector<net::CanonicalCookie>& cookies_to_set,
      const net::CookieOptions& cookie_options,
      CreateBoundSessionsCallback callback,
      std::vector<net::device_bound_sessions::SessionError::ErrorType>
          session_errors);

  raw_ptr<net::device_bound_sessions::SessionService> service_;
  // `raw_ptr` is safe because both `this` and `cookie_manager_` are
  // owned by the `NetworkContext`.
  raw_ptr<CookieManager> cookie_manager_;
  mojo::ReceiverSet<network::mojom::DeviceBoundSessionManager> receivers_;
  std::vector<std::unique_ptr<ObserverRegistration>> observer_registrations_;

  base::WeakPtrFactory<DeviceBoundSessionManager> weak_factory_{this};
};

}  // namespace network

#endif  // SERVICES_NETWORK_DEVICE_BOUND_SESSION_MANAGER_H_
