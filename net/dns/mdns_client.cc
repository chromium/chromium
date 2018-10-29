// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/mdns_client.h"

#include "net/base/ip_address.h"
#include "net/base/net_errors.h"
#include "net/base/network_interfaces.h"
#include "net/dns/dns_protocol.h"
#include "net/dns/mdns_client_impl.h"
#include "net/log/net_log.h"
#include "net/log/net_log_source.h"

namespace net {

namespace {

const char kMDnsMulticastGroupIPv4[] = "224.0.0.251";
const char kMDnsMulticastGroupIPv6[] = "FF02::FB";

IPEndPoint GetMDnsIPEndPoint(const char* address) {
  IPAddress multicast_group_number;
  bool success = multicast_group_number.AssignFromIPLiteral(address);
  DCHECK(success);
  return IPEndPoint(multicast_group_number,
                    dns_protocol::kDefaultPortMulticast);
}

int Bind(AddressFamily address_family,
         uint32_t interface_index,
         DatagramServerSocket* socket) {
  socket->AllowAddressSharingForMulticast();
  socket->SetMulticastInterface(interface_index);

  int rv = socket->Listen(GetMDnsReceiveEndPoint(address_family));
  if (rv < OK)
    return rv;

  return socket->JoinGroup(GetMDnsGroupEndPoint(address_family).address());
}

}  // namespace

const base::TimeDelta MDnsTransaction::kTransactionTimeout =
    base::TimeDelta::FromSeconds(3);

// static
std::unique_ptr<MDnsSocketFactory> MDnsSocketFactory::CreateDefault() {
  return std::unique_ptr<MDnsSocketFactory>(new MDnsSocketFactoryImpl);
}

// static
std::unique_ptr<MDnsClient> MDnsClient::CreateDefault() {
  return std::unique_ptr<MDnsClient>(new MDnsClientImpl());
}

IPEndPoint GetMDnsGroupEndPoint(AddressFamily address_family) {
  switch (address_family) {
    case ADDRESS_FAMILY_IPV4:
      return GetMDnsIPEndPoint(kMDnsMulticastGroupIPv4);
    case ADDRESS_FAMILY_IPV6:
      return GetMDnsIPEndPoint(kMDnsMulticastGroupIPv6);
    default:
      NOTREACHED();
      return IPEndPoint();
  }
}

IPEndPoint GetMDnsReceiveEndPoint(AddressFamily address_family) {
#ifdef OS_WIN
  // With Windows, binding to a mulitcast group address is not allowed.
  // Multicast messages will be received appropriate to the multicast groups the
  // socket has joined. Sockets intending to receive multicast messages should
  // bind to a wildcard address (e.g. 0.0.0.0).
  switch (address_family) {
    case ADDRESS_FAMILY_IPV4:
      return IPEndPoint(IPAddress::IPv4AllZeros(),
                        dns_protocol::kDefaultPortMulticast);
    case ADDRESS_FAMILY_IPV6:
      return IPEndPoint(IPAddress::IPv6AllZeros(),
                        dns_protocol::kDefaultPortMulticast);
    default:
      NOTREACHED();
      return IPEndPoint();
  }
#else   // !OS_WIN
  // With POSIX, any socket can receive messages for multicast groups joined by
  // any socket on the system. Sockets intending to receive messages for a
  // specific multicast group should bind to that group address.
  return GetMDnsGroupEndPoint(address_family);
#endif  // !OS_WIN
}

InterfaceIndexFamilyList GetMDnsInterfacesToBind() {
  NetworkInterfaceList network_list;
  InterfaceIndexFamilyList interfaces;
  if (!GetNetworkList(&network_list, INCLUDE_HOST_SCOPE_VIRTUAL_INTERFACES))
    return interfaces;
  for (size_t i = 0; i < network_list.size(); ++i) {
    AddressFamily family = GetAddressFamily(network_list[i].address);
    if (family == ADDRESS_FAMILY_IPV4 || family == ADDRESS_FAMILY_IPV6) {
      interfaces.push_back(
          std::make_pair(network_list[i].interface_index, family));
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
  std::unique_ptr<DatagramServerSocket> socket(
      new UDPServerSocket(net_log, NetLogSource()));

  int rv = Bind(address_family, interface_index, socket.get());
  if (rv != OK) {
    socket.reset();
    VLOG(1) << "MDNS bind failed, address_family=" << address_family
            << ", error=" << rv;
  }
  return socket;
}

}  // namespace net
