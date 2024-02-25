// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/mdns_client.h"

#include "net/base/address_family.h"
#include "net/base/net_errors.h"
#include "net/base/network_interfaces.h"
#include "net/dns/mdns_client_impl.h"
#include "net/dns/public/util.h"
#include "net/log/net_log.h"
#include "net/log/net_log_source.h"

namespace net {

namespace {

int Bind(AddressFamily address_family,
         uint32_t interface_index,
         DatagramServerSocket* socket) {
  socket->AllowAddressSharingForMulticast();
  socket->SetMulticastInterface(interface_index);

  int rv = socket->Listen(dns_util::GetMdnsReceiveEndPoint(address_family));
  if (rv < OK)
    return rv;

  return socket->JoinGroup(
      dns_util::GetMdnsGroupEndPoint(address_family).address());
}

}  // namespace

const base::TimeDelta MDnsTransaction::kTransactionTimeout = base::Seconds(3);

// static
std::unique_ptr<MDnsSocketFactory> MDnsSocketFactory::CreateDefault() {
  return std::make_unique<MDnsSocketFactoryImpl>();
}

// static
std::unique_ptr<MDnsClient> MDnsClient::CreateDefault() {
  return std::make_unique<MDnsClientImpl>();
}

InterfaceIndexFamilyList GetMDnsInterfacesToBind() {
  NetworkInterfaceList network_list;
  InterfaceIndexFamilyList interfaces;
  if (!GetNetworkList(&network_list, INCLUDE_HOST_SCOPE_VIRTUAL_INTERFACES))
    return interfaces;
  for (const auto& network_interface : network_list) {
    AddressFamily family = GetAddressFamily(network_interface.address);
    if (family == ADDRESS_FAMILY_IPV4 || family == ADDRESS_FAMILY_IPV6) {
      interfaces.emplace_back(network_interface.interface_index, family);
    }
  }
  std::sort(interfaces.begin(), interfaces.end());
  // Interfaces could have multiple addresses. Filter out duplicate entries.
  interfaces.erase(std::unique(interfaces.begin(), interfaces.end()),
                   interfaces.end());
  return interfaces;
}

std::unique_ptr<DatagramServerSocket> CreateAndBindMDnsSocket(
    AddressFamily address_family,
    uint32_t interface_index,
    NetLog* net_log) {
  auto socket = std::make_unique<UDPServerSocket>(net_log, NetLogSource());

  int rv = Bind(address_family, interface_index, socket.get());
  if (rv != OK) {
    socket.reset();
    VLOG(1) << "MDNS bind failed, address_family=" << address_family
            << ", error=" << rv;
  }
  return socket;
}

}  // namespace net
