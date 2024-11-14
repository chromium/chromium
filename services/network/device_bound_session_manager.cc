// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/device_bound_session_manager.h"

#include "net/device_bound_sessions/session_service.h"

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
  service_->DeleteSession(
      session_key.site,
      net::device_bound_sessions::Session::Id(session_key.id));
}

}  // namespace network
