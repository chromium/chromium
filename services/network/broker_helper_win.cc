// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/broker_helper_win.h"

#include "base/no_destructor.h"
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
  if (address.IsLoopback())
    return true;

  for (const auto& network_interface : interfaces_) {
    if (network_interface.address == address)
      return true;
  }
  return false;
}

}  // namespace network
