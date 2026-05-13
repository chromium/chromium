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

  // Windows firewall blocks connections from an App-Container to:
  //   * Any loopback address, due to the built-in `AppContainerLoopback`
  //     rule.
  //   * Any address that the OS classifies as belonging to a "public"
  //     network. The classification is performed by the Network Location
  //     Awareness (NLA) service on a per-interface basis. NLA leaves some
  //     interfaces unclassified -- for example interfaces installed by VPN
  //     clients or virtualization software (VMware, Hyper-V, WSL, F5
  //     Big-IP), point-to-point adapters with no default gateway, or
  //     interfaces on hosts upgraded from Windows 10 -- and traffic over
  //     those interfaces is dropped by the
  //     "UWP Default Outbound Block Rule" sublayer regardless of the
  //     `internetClient` and `privateNetworkClientServer` capabilities
  //     granted to the App-Container. See crbug.com/466139402.
  //
  // We cannot reliably detect which interfaces NLA has misclassified, and
  // we cannot enumerate every reachable subnet (e.g. routes installed by a
  // VPN may direct traffic to addresses that do not match any local
  // interface prefix). To keep the network service usable on every
  // configuration, broker every connection whose destination is *not*
  // publicly routable. That covers loopback, IPv4/IPv6 link-local,
  // RFC1918 / Unique-Local addresses, the IPv4 / IPv6 unspecified
  // address, Carrier-Grade NAT, and every other IANA-reserved range that
  // may be subject to the UWP firewall block.
  if (!address.IsPubliclyRoutable()) {
    return true;
  }

  // Backstop: also broker connections to any address on the same prefix as
  // a local interface. This covers the unusual case where a host's local
  // network uses publicly-routable addresses (for example an organization
  // assigned a public /24).
  for (const auto& network_interface : interfaces_) {
    // `IPAddressMatchesPrefix` CHECKs for invalid prefix lengths but
    // `prefix_length` comes from Windows where "a value of 255 is commonly used
    // to represent an illegal value".
    const uint32_t prefix_length =
        std::min(network_interface.address.size() * 8,
                 base::strict_cast<size_t>(network_interface.prefix_length));

    if (net::IPAddressMatchesPrefix(address, network_interface.address,
                                    prefix_length)) {
      return true;
    }
  }

  return false;
}

}  // namespace network
