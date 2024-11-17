// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_DEVICE_BOUND_SESSION_MANAGER_H_
#define SERVICES_NETWORK_DEVICE_BOUND_SESSION_MANAGER_H_

#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/network/public/mojom/device_bound_sessions.mojom.h"

namespace net::device_bound_sessions {
class SessionService;
struct SessionKey;
}  // namespace net::device_bound_sessions

namespace network {

class DeviceBoundSessionManager : public mojom::DeviceBoundSessionManager {
 public:
  static std::unique_ptr<DeviceBoundSessionManager> Create(
      net::device_bound_sessions::SessionService* service);

  ~DeviceBoundSessionManager() override;

  void AddReceiver(
      mojo::PendingReceiver<mojom::DeviceBoundSessionManager> receiver);

  // network::mojom::DeviceBoundSessionManager
  void GetAllSessions(GetAllSessionsCallback callback) override;
  void DeleteSession(
      const net::device_bound_sessions::SessionKey& session_key) override;

 private:
  explicit DeviceBoundSessionManager(
      net::device_bound_sessions::SessionService* service);

  raw_ptr<net::device_bound_sessions::SessionService> service_;
  mojo::ReceiverSet<network::mojom::DeviceBoundSessionManager> receivers_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_DEVICE_BOUND_SESSION_MANAGER_H_
