// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_NETWORKING_PRIVATE_NETWORKING_PRIVATE_DELEGATE_OBSERVER_H_
#define EXTENSIONS_BROWSER_API_NETWORKING_PRIVATE_NETWORKING_PRIVATE_DELEGATE_OBSERVER_H_

#include <string>
#include <vector>
#include "extensions/common/api/networking_private.h"

namespace extensions {

// Implemented by event handlers so they are notified when a change event
// occurs. Triggered by NetworkingPrivateServiceClient,
// NetworkingPrivateLinux or LacrosNetworkingPrivateObserver.
class NetworkingPrivateDelegateObserver {
 public:
  NetworkingPrivateDelegateObserver& operator=(
      const NetworkingPrivateDelegateObserver&) = delete;

  // Notifes observers when properties may have changed for the networks listed
  // in |network_guids|.
  virtual void OnNetworksChangedEvent(
      const std::vector<std::string>& network_guids) = 0;

  // Notifies observers that the list of networks changed. |network_guids|
  // contains the complete list of network guids.
  virtual void OnNetworkListChangedEvent(
      const std::vector<std::string>& network_guids) = 0;

  // Notifies observers that the list of devices has changed or any device
  // state properties have changed.
  virtual void OnDeviceStateListChanged() = 0;

  // Notifies observers when portal detection for a network completes. Sends
  // the guid of the network and the corresponding captive portal status.
  virtual void OnPortalDetectionCompleted(
      std::string networkGuid,
      api::networking_private::CaptivePortalStatus status) = 0;

  // Notifies observers when any certificate list has changed.
  virtual void OnCertificateListsChanged() = 0;

 protected:
  virtual ~NetworkingPrivateDelegateObserver() = default;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_NETWORKING_PRIVATE_NETWORKING_PRIVATE_DELEGATE_OBSERVER_H_
