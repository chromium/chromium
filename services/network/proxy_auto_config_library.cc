// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/proxy_auto_config_library.h"

#include "net/base/address_list.h"
#include "net/base/ip_address.h"
#include "net/base/network_interfaces.h"
#include "net/dns/host_resolver_proc.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/udp_client_socket.h"

namespace network {

namespace {

enum class Mode {
  kMyIpAddress,
  kMyIpAddressEx,
};

// Helper used to accumulate and select the best candidate IP addresses.
//
// myIpAddress() is a broken API available to PAC scripts.
// It has the problematic definition of:
// "Returns the IP address of the host machine."
//
// This has ambiguity on what should happen for multi-homed hosts which may have
// multiple IP addresses to choose from. To be unambiguous we would need to
// know which hosts is going to be connected to, in order to use the outgoing
// IP for that request.
//
// However at this point that is not known, as the proxy still hasn't been
// decided.
//
// The strategy used here is to prioritize the IP address that would be used
// for connecting to the public internet by testing which interface is used for
// connecting to 8.8.8.8 and 2001:4860:4860::8888 (public IPs).
//
// If that fails, we will try resolving the machine's hostname, and also probing
// for routes in the private IP space.
//
// Link-local IP addresses are not generally returned, however may be if no
// other IP was found by the probes.
class MyIpAddressImpl {
 public:
  MyIpAddressImpl() = default;

  // Used for mocking the socket dependency.
  void SetSocketFactoryForTest(net::ClientSocketFactory* socket_factory) {
    override_socket_factory_ = socket_factory;
  }

  // Used for mocking the DNS dependency.
  void SetDNSResultForTest(const net::AddressList& addrs) {
    override_dns_result_ = std::make_unique<net::AddressList>(addrs);
  }

  net::IPAddressList Run(Mode mode) {
    DCHECK(candidate_ips_.empty());
    DCHECK(link_local_ips_.empty());
    DCHECK(!done_);

    mode_ = mode;

    // Try several different methods to obtain IP addresses.
    TestPublicInternetRoutes();
    TestResolvingHostname();
    TestPrivateIPRoutes();

    return mode_ == Mode::kMyIpAddress ? GetResultForMyIpAddress()
                                       : GetResultForMyIpAddressEx();
  }

 private:
  // Adds |address| to the result.
  void Add(const net::IPAddress& address) {
    if (done_)
      return;

    // Don't consider loopback addresses (ex: 127.0.0.1). These can notably be
    // returned when probing addresses associated with the hostname.
    if (address.IsLoopback())
      return;

    if (!seen_ips_.insert(address).second)
      return;  // Duplicate IP address.

    // Link-local addresses are only used as a last-resort if there are no
    // better addresses.
    if (address.IsLinkLocal()) {
      link_local_ips_.push_back(address);
      return;
    }

    // For legacy reasons IPv4 addresses are favored over IPv6 for myIpAddress()
    // - https://crbug.com/905126 - so this only stops the search when a IPv4
    // address is found.
    if ((mode_ == Mode::kMyIpAddress) && address.IsIPv4())
      done_ = true;

    candidate_ips_.push_back(address);
  }

  net::IPAddressList GetResultForMyIpAddress() const {
    DCHECK_EQ(Mode::kMyIpAddress, mode_);

    if (!candidate_ips_.empty())
      return GetSingleResultFavoringIPv4(candidate_ips_);

    if (!link_local_ips_.empty())
      return GetSingleResultFavoringIPv4(link_local_ips_);

    return {};
  }

  net::IPAddressList GetResultForMyIpAddressEx() const {
    DCHECK_EQ(Mode::kMyIpAddressEx, mode_);

    if (!candidate_ips_.empty())
      return candidate_ips_;

    if (!link_local_ips_.empty()) {
      // Note that only a single link-local address is returned here, even
      // though multiple could be returned for this API. See
      // http://crbug.com/905366 before expanding this.
      return GetSingleResultFavoringIPv4(link_local_ips_);
    }

    return {};
  }

  // Tests what source IP address would be used for sending a UDP packet to the
  // given destination IP. This does not hit the network and should be fast.
  void TestRoute(const net::IPAddress& destination_ip) {
    if (done_)
      return;

    net::ClientSocketFactory* socket_factory =
        override_socket_factory_
            ? override_socket_factory_
            : net::ClientSocketFactory::GetDefaultFactory();

    auto socket = socket_factory->CreateDatagramClientSocket(
        net::DatagramSocket::DEFAULT_BIND, nullptr, net::NetLogSource());

    net::IPEndPoint destination(destination_ip, /*port=*/80);

    if (socket->Connect(destination) != net::OK)
      return;

    net::IPEndPoint source;
    if (socket->GetLocalAddress(&source) != net::OK)
      return;

    Add(source.address());
  }

  void TestPublicInternetRoutes() {
    if (done_)
      return;

    // 8.8.8.8 and 2001:4860:4860::8888 are Google DNS.
    TestRoute(net::IPAddress(8, 8, 8, 8));
    TestRoute(net::IPAddress(0x20, 0x01, 0x48, 0x60, 0x48, 0x60, 0, 0, 0, 0, 0,
                             0, 0, 0, 0x88, 0x88));

    MarkAsDoneIfHaveCandidates();
  }

  // Marks the current search as done if candidate IPs have been found.
  //
  // This is used to stop exploring for IPs if any of the high-level tests find
  // a match (i.e. either the public internet route test, or hostname test, or
  // private route test found something).
  //
  // In the case of myIpAddressEx() this means it will be conservative in which
  // IPs it returns and not enumerate the full set. See http://crbug.com/905366
  // before expanding that policy.
  void MarkAsDoneIfHaveCandidates() {
    if (!candidate_ips_.empty())
      done_ = true;
  }

  void TestPrivateIPRoutes() {
    if (done_)
      return;

    // Representative IP from each range in RFC 1918.
    TestRoute(net::IPAddress(10, 0, 0, 0));
    TestRoute(net::IPAddress(172, 16, 0, 0));
    TestRoute(net::IPAddress(192, 168, 0, 0));

    // Representative IP for Unique Local Address (FC00::/7).
    TestRoute(
        net::IPAddress(0xfc, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0));

    MarkAsDoneIfHaveCandidates();
  }

  void TestResolvingHostname() {
    if (done_)
      return;

    net::AddressList addrlist;

    int resolver_error;

    if (override_dns_result_) {
      addrlist = *override_dns_result_;
      resolver_error = addrlist.empty() ? net::ERR_NAME_NOT_RESOLVED : net::OK;
    } else {
      resolver_error = SystemHostResolverCall(
          net::GetHostName(), net::AddressFamily::ADDRESS_FAMILY_UNSPECIFIED, 0,
          &addrlist,
          /*os_error=*/nullptr);
    }

    if (resolver_error != net::OK)
      return;

    for (const auto& e : addrlist.endpoints())
      Add(e.address());

    MarkAsDoneIfHaveCandidates();
  }

  static net::IPAddressList GetSingleResultFavoringIPv4(
      const net::IPAddressList& ips) {
    for (const auto& ip : ips) {
      if (ip.IsIPv4())
        return {ip};
    }

    if (!ips.empty())
      return {ips.front()};

    return {};
  }

  std::set<net::IPAddress> seen_ips_;

  // The preferred ordered candidate IPs so far.
  net::IPAddressList candidate_ips_;

  // The link-local IP addresses seen so far (not part of |candidate_ips_|).
  net::IPAddressList link_local_ips_;

  // The operation being carried out.
  Mode mode_;

  // Whether the search for results has completed.
  //
  // Once "done", calling Add() will not change the final result. This is used
  // to short-circuit early.
  bool done_ = false;

  net::ClientSocketFactory* override_socket_factory_ = nullptr;
  std::unique_ptr<net::AddressList> override_dns_result_;

  DISALLOW_COPY_AND_ASSIGN(MyIpAddressImpl);
};

}  // namespace

net::IPAddressList PacMyIpAddress() {
  MyIpAddressImpl impl;
  return impl.Run(Mode::kMyIpAddress);
}

net::IPAddressList PacMyIpAddressEx() {
  MyIpAddressImpl impl;
  return impl.Run(Mode::kMyIpAddressEx);
}

net::IPAddressList PacMyIpAddressForTest(
    net::ClientSocketFactory* socket_factory,
    const net::AddressList& dns_result) {
  MyIpAddressImpl impl;
  impl.SetSocketFactoryForTest(socket_factory);
  impl.SetDNSResultForTest(dns_result);
  return impl.Run(Mode::kMyIpAddress);
}

net::IPAddressList PacMyIpAddressExForTest(
    net::ClientSocketFactory* socket_factory,
    const net::AddressList& dns_result) {
  MyIpAddressImpl impl;
  impl.SetSocketFactoryForTest(socket_factory);
  impl.SetDNSResultForTest(dns_result);
  return impl.Run(Mode::kMyIpAddressEx);
}

}  // namespace network
