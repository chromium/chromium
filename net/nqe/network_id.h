// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_NQE_NETWORK_ID_H_
#define NET_NQE_NETWORK_ID_H_

#include <string>

#include "net/base/net_export.h"
#include "net/base/network_change_notifier.h"

namespace net::nqe::internal {

// NetworkID is used to uniquely identify a network.
// For the purpose of network quality estimation and caching, a network is
// uniquely identified by a combination of |type| and
// |id|. This approach is unable to distinguish networks with
// same name (e.g., different Wi-Fi networks with same SSID).
// This is a protected member to expose it to tests.
struct NET_EXPORT_PRIVATE NetworkID {
  static NetworkID FromString(const std::string& network_id);

  NetworkID(NetworkChangeNotifier::ConnectionType type,
            const std::string& id,
            int32_t signal_strength);
  NetworkID(const NetworkID& other);
  ~NetworkID();

  bool operator==(const NetworkID& other) const;

  bool operator!=(const NetworkID& other) const;

  NetworkID& operator=(const NetworkID& other);

  // Overloaded to support ordered collections.
  bool operator<(const NetworkID& other) const;

  std::string ToString() const;

  // Connection type of the network.
  NetworkChangeNotifier::ConnectionType type;

  // Name of this network. This is set to:
  // - Wi-Fi SSID if the device is connected to a Wi-Fi access point and the
  //   SSID name is available, or
  // - MCC/MNC code of the cellular carrier if the device is connected to a
  //   cellular network, or
  // - "Ethernet" in case the device is connected to ethernet.
  // - An empty string in all other cases or if the network name is not
  //   exposed by platform APIs.
  std::string id;

  // Signal strength of the network. Set to INT32_MIN when the value is
  // unavailable. Otherwise, must be between 0 and 4 (both inclusive). This may
  // take into account many different radio technology inputs. 0 represents very
  // poor signal strength while 4 represents a very strong signal strength. The
  // range is capped between 0 and 4 to ensure that a change in the value
  // indicates a non-negligible change in the signal quality.
  //
  // TODO(crbug.com/40937712): This should use std::optional instead of a magic
  // value.
  int32_t signal_strength;
};

}  // namespace net::nqe::internal

#endif  // NET_NQE_NETWORK_ID_H_
