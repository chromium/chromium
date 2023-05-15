// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/networking_private/lacros_networking_private_observer.h"

#include "chromeos/lacros/lacros_service.h"

using crosapi::mojom::NetworkingPrivate;

namespace {

extensions::api::networking_private::CaptivePortalStatus
GetApiCaptivePortalStatus(crosapi::mojom::CaptivePortalStatus mojoStatus) {
  switch (mojoStatus) {
    case crosapi::mojom::CaptivePortalStatus::kUnknown:
      return extensions::api::networking_private::CaptivePortalStatus::kUnknown;
    case crosapi::mojom::CaptivePortalStatus::kOffline:
      return extensions::api::networking_private::CaptivePortalStatus::kOffline;
    case crosapi::mojom::CaptivePortalStatus::kOnline:
      return extensions::api::networking_private::CaptivePortalStatus::kOnline;
    case crosapi::mojom::CaptivePortalStatus::kPortal:
      return extensions::api::networking_private::CaptivePortalStatus::kPortal;
    case crosapi::mojom::CaptivePortalStatus::kProxyAuthRequired:
      return extensions::api::networking_private::CaptivePortalStatus::
          kProxyAuthRequired;
  }
}

}  // namespace

LacrosNetworkingPrivateObserver::LacrosNetworkingPrivateObserver()
    : receiver_{this} {
  chromeos::LacrosService* service = chromeos::LacrosService::Get();
  if (!service->IsAvailable<NetworkingPrivate>()) {
    DLOG(WARNING) << "crosapi::mojom::NetworkingPrivate is not available";
    return;
  }

  if (service->GetInterfaceVersion<NetworkingPrivate>() <
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

void LacrosNetworkingPrivateObserver::OnPortalDetectionCompleted(
    const std::string& networkGuid,
    crosapi::mojom::CaptivePortalStatus status) {
  for (auto& observer : lacros_observers_) {
    observer.OnPortalDetectionCompleted(networkGuid,
                                        GetApiCaptivePortalStatus(status));
  }
}

void LacrosNetworkingPrivateObserver::OnCertificateListsChanged() {
  for (auto& observer : lacros_observers_) {
    observer.OnCertificateListsChanged();
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
