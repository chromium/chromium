// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_IP_PROTECTION_IP_PROTECTION_PROXY_LIST_MANAGER_H_
#define SERVICES_NETWORK_IP_PROTECTION_IP_PROTECTION_PROXY_LIST_MANAGER_H_

#include "base/component_export.h"

namespace net {

class ProxyChain;

}  // namespace net

namespace network {

// Manages a list of currently cached proxy hostnames.
//
// This class is responsible for checking, fetching, and refreshing the proxy
// list for IpProtectionConfigCache.
class COMPONENT_EXPORT(NETWORK_SERVICE) IpProtectionProxyListManager {
 public:
  virtual ~IpProtectionProxyListManager() = default;

  // Check whether a proxy list is available.
  virtual bool IsProxyListAvailable() = 0;

  // Return the currently cached proxy list. This list may be empty even
  // if `IsProxyListAvailable()` returned true.
  virtual const std::vector<net::ProxyChain>& ProxyList() = 0;

  // Return the `GeoId` string which is the geo for which the current list is
  // valid.

  // This is a formatted version of the `network::mojom::GeoHint`. It consists
  // of a concatenation of the country region, iso region, and city name
  // (separated by commas). If there are fields missing, they are omitted, and
  // there is no trailing comma. String can be empty if a successful request
  // fetching proxy lists has not occurred.
  virtual const std::string& GeoId() = 0;

  // Request a refresh of the proxy list. Call this when it's likely that the
  // proxy list is out of date.
  virtual void RequestRefreshProxyList() = 0;
};

}  // namespace network

#endif  // SERVICES_NETWORK_IP_PROTECTION_IP_PROTECTION_PROXY_LIST_MANAGER_H_
