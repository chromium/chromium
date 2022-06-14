// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/networking_private/lacros_networking_private_observer.h"

#include "chromeos/lacros/lacros_service.h"

using crosapi::mojom::NetworkingPrivate;

LacrosNetworkingPrivateObserver::LacrosNetworkingPrivateObserver()
    : receiver_{this} {
  chromeos::LacrosService* service = chromeos::LacrosService::Get();
  if (!service->IsAvailable<NetworkingPrivate>()) {
    DLOG(WARNING) << "crosapi::mojom::NetworkingPrivate is not available";
    return;
  }

  if (service->GetInterfaceVersion(NetworkingPrivate::Uuid_) <
      static_cast<int>(
          NetworkingPrivate::MethodMinVersions::kAddObserverMinVersion)) {
    DLOG(WARNING) << "Unsupported ash version.";
    return;
  }

  service->GetRemote<NetworkingPrivate>()->AddObserver(
      receiver_.BindNewPipeAndPassRemote());
}

LacrosNetworkingPrivateObserver::~LacrosNetworkingPrivateObserver() = default;

void LacrosNetworkingPrivateObserver::OnNetworksChangedEvent(
    const std::vector<std::string>& network_guids) {
  for (auto& observer : lacros_observers_) {
    observer.OnNetworksChangedEvent(network_guids);
  }
}

void LacrosNetworkingPrivateObserver::OnNetworkListChangedEvent(
    const std::vector<std::string>& network_guids) {
  for (auto& observer : lacros_observers_) {
    observer.OnNetworkListChangedEvent(network_guids);
  }
}

void LacrosNetworkingPrivateObserver::OnDeviceStateListChanged() {
  for (auto& observer : lacros_observers_) {
    observer.OnDeviceStateListChanged();
  }
}

void LacrosNetworkingPrivateObserver::AddObserver(
    extensions::NetworkingPrivateDelegateObserver* observer) {
  lacros_observers_.AddObserver(observer);
}

void LacrosNetworkingPrivateObserver::RemoveObserver(
    extensions::NetworkingPrivateDelegateObserver* observer) {
  lacros_observers_.RemoveObserver(observer);
}

bool LacrosNetworkingPrivateObserver::HasObservers() const {
  return !lacros_observers_.empty();
}
