// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_NETWORKING_PRIVATE_LACROS_NETWORKING_PRIVATE_OBSERVER_H_
#define EXTENSIONS_BROWSER_API_NETWORKING_PRIVATE_LACROS_NETWORKING_PRIVATE_OBSERVER_H_

#include "chromeos/crosapi/mojom/networking_private.mojom.h"

#include "base/observer_list.h"
#include "extensions/browser/api/networking_private/networking_private_delegate_observer.h"
#include "mojo/public/cpp/bindings/receiver.h"

// Receives NetworkingPrivate notifications from Ash and forwards them to its
// own observers inside Lacros.
class LacrosNetworkingPrivateObserver
    : public crosapi::mojom::NetworkingPrivateDelegateObserver {
 public:
  LacrosNetworkingPrivateObserver();
  ~LacrosNetworkingPrivateObserver() override;
  LacrosNetworkingPrivateObserver(const LacrosNetworkingPrivateObserver&) =
      delete;
  LacrosNetworkingPrivateObserver& operator=(
      const LacrosNetworkingPrivateObserver&) = delete;

  // crosapi::mojom::NetworkingPrivateDelegateObserver overrides:
  void OnNetworksChangedEvent(
      const std::vector<std::string>& network_guids) override;
  void OnNetworkListChangedEvent(
      const std::vector<std::string>& network_guids) override;
  void OnDeviceStateListChanged() override;
  void OnPortalDetectionCompleted(
      const std::string& networkGuid,
      crosapi::mojom::CaptivePortalStatus status) override;
  void OnCertificateListsChanged() override;

  void AddObserver(extensions::NetworkingPrivateDelegateObserver* observer);
  void RemoveObserver(extensions::NetworkingPrivateDelegateObserver* observer);
  bool HasObservers() const;

 private:
  // Mojo endpoint that's responsible for receiving messages from Ash.
  mojo::Receiver<crosapi::mojom::NetworkingPrivateDelegateObserver> receiver_;

  // Notifications coming from Ash will be forwarded to these observers inside
  // Lacros.
  base::ObserverList<extensions::NetworkingPrivateDelegateObserver>::Unchecked
      lacros_observers_;
};

#endif  // EXTENSIONS_BROWSER_API_NETWORKING_PRIVATE_LACROS_NETWORKING_PRIVATE_OBSERVER_H_
