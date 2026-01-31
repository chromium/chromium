// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/broker_helper_win.h"

#include <algorithm>

#include "base/no_destructor.h"
#include "base/numerics/safe_conversions.h"
#include "base/threading/thread_local.h"
#include "net/base/ip_address.h"
#include "net/base/network_change_notifier.h"
#include "net/base/network_interfaces.h"

namespace network {

BrokerHelperWin::BrokerHelperWin() {
  net::NetworkChangeNotifier::AddNetworkChangeObserver(this);
  RefreshNetworkList();
}

BrokerHelperWin::~BrokerHelperWin() {
  net::NetworkChangeNotifier::RemoveNetworkChangeObserver(this);
}

void BrokerHelperWin::OnNetworkChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  RefreshNetworkList();
}

void BrokerHelperWin::RefreshNetworkList() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  net::NetworkInterfaceList network_interfaces;
  bool result = GetNetworkList(&network_interfaces,
                               net::INCLUDE_HOST_SCOPE_VIRTUAL_INTERFACES);
  if (result)
    interfaces_ = std::move(network_interfaces);
}

bool BrokerHelperWin::ShouldBroker(const net::IPAddress& address) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (delegate_) {
    return delegate_->ShouldBroker();
  }

  // Windows firewall blocks connections from App-Container to loopback
  // addresses due to rule `AppContainerLoopback`.
  if (address.IsLoopback()) {
    return true;
  }

  for (const auto& network_interface : interfaces_) {
    // `IPAddressMatchesPrefix` CHECKs for invalid prefix lengths but
    // `prefix_length` comes from Windows where "a value of 255 is commonly used
    // to represent an illegal value".
    const uint32_t prefix_length =
        std::min(network_interface.address.size() * 8,
                 base::strict_cast<size_t>(network_interface.prefix_length));

    // Windows firewall blocks connections to local network interfaces from
    // App-Container due to rule `AppContainerLoopback`.
    //
    // In addition, on some hosts, link-local traffic through network interfaces
    // that are not assigned a network connection profile are blocked due to
    // rule 'UWP Default Outbound Block Rule`, so broker these connections.
    if (net::IPAddressMatchesPrefix(address, network_interface.address,
                                    prefix_length)) {
      return true;
    }
  }

  return false;
}

}  // namespace network
