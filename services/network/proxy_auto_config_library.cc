// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/proxy_auto_config_library.h"

#include <set>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/task/thread_pool.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/address_list.h"
#include "net/base/ip_address.h"
#include "net/base/network_interfaces.h"
#include "net/dns/host_resolver_system_task.h"
#include "net/log/net_log_source.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/datagram_client_socket.h"

namespace network {

MyIpAddressImpl::MyIpAddressImpl(Mode mode)
    : worker_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN,
           base::TaskPriority::USER_VISIBLE})),
      mode_(mode) {}

MyIpAddressImpl::~MyIpAddressImpl() {
  // The remotes in |my_ip_address_clients_| cannot be deleted on the same
  // thread as |override_socket_factory_|.
  DCHECK(my_ip_address_clients_.empty() || !override_socket_factory_);
}

void MyIpAddressImpl::AddRequest(
    mojo::PendingRemote<proxy_resolver::mojom::HostResolverRequestClient>
        my_ip_address_client) {
  worker_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&MyIpAddressImpl::AddRequestOnWorkerThread,
                                this, std::move(my_ip_address_client)));
}

void MyIpAddressImpl::SetSocketFactoryForTest(
    net::ClientSocketFactory* socket_factory) {
  override_socket_factory_ = socket_factory;
}

void MyIpAddressImpl::SetDNSResultForTest(const net::AddressList& addrs) {
  override_dns_result_ = std::make_unique<net::AddressList>(addrs);
}

void MyIpAddressImpl::AddRequestOnWorkerThread(
    mojo::PendingRemote<proxy_resolver::mojom::HostResolverRequestClient>
        my_ip_address_client) {
  bool was_running = !my_ip_address_clients_.empty();
  // TODO(mpdenton): currently it should not be possible for `was_running` to be
  // true because this method runs synchronously. When MyIpAddressImpl becomes
  // asynchronous this will become possible.
  my_ip_address_clients_.Add(std::move(my_ip_address_client));
  if (was_running)
    return;

  DCHECK(seen_ips_.empty());
  DCHECK(candidate_ips_.empty());
  DCHECK(link_local_ips_.empty());
  DCHECK(!done_);

  // Try several different methods to obtain IP addresses.
  TestPublicInternetRoutes();
  TestResolvingHostname();
  TestPrivateIPRoutes();

  SendResultsAndReset();
}

void MyIpAddressImpl::SendResultsAndReset() {
  std::vector<net::IPAddress> my_ip_addresses =
      mode_ == Mode::kMyIpAddress ? GetResultForMyIpAddress()
                                  : GetResultForMyIpAddressEx();

  // TODO(eroman): Note that this code always returns a success response (with
  // loopback) rather than passing forward the error. This is to ensure that
  // the response gets cached on the proxy resolver process side, since this
  // layer here does not currently do any caching or de-duplication. This
  // should be cleaned up once the interfaces are refactored. Lastly note that
  // for myIpAddress() this doesn't change the final result. However for
  // myIpAddressEx() it means we return 127.0.0.1 rather than empty string.
  if (my_ip_addresses.empty())
    my_ip_addresses.push_back(net::IPAddress::IPv4Localhost());

  for (auto& client : my_ip_address_clients_) {
    client->ReportResult(net::OK, my_ip_addresses);
  }

  my_ip_address_clients_.Clear();
  seen_ips_.clear();
  candidate_ips_.clear();
  link_local_ips_.clear();
  done_ = false;
}

// Adds `address` to the result.
void MyIpAddressImpl::Add(const net::IPAddress& address) {
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

net::IPAddressList MyIpAddressImpl::GetResultForMyIpAddress() const {
  DCHECK_EQ(Mode::kMyIpAddress, mode_);

  if (!candidate_ips_.empty())
    return GetSingleResultFavoringIPv4(candidate_ips_);

  if (!link_local_ips_.empty())
    return GetSingleResultFavoringIPv4(link_local_ips_);

  return {};
}

net::IPAddressList MyIpAddressImpl::GetResultForMyIpAddressEx() const {
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
void MyIpAddressImpl::TestRoute(const net::IPAddress& destination_ip) {
  if (done_)
    return;

  net::ClientSocketFactory* socket_factory =
      override_socket_factory_ ? override_socket_factory_.get()
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

void MyIpAddressImpl::TestPublicInternetRoutes() {
  if (done_)
    return;

  // 8.8.8.8 and 2001:4860:4860::8888 are Google DNS.
  TestRoute(net::IPAddress(8, 8, 8, 8));
  TestRoute(net::IPAddress(0x20, 0x01, 0x48, 0x60, 0x48, 0x60, 0, 0, 0, 0, 0, 0,
                           0, 0, 0x88, 0x88));

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
void MyIpAddressImpl::MarkAsDoneIfHaveCandidates() {
  if (!candidate_ips_.empty())
    done_ = true;
}

void MyIpAddressImpl::TestPrivateIPRoutes() {
  if (done_)
    return;

  // Representative IP from each range in RFC 1918.
  TestRoute(net::IPAddress(10, 0, 0, 0));
  TestRoute(net::IPAddress(172, 16, 0, 0));
  TestRoute(net::IPAddress(192, 168, 0, 0));

  // Representative IP for Unique Local Address (FC00::/7).
  TestRoute(net::IPAddress(0xfc, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0));

  MarkAsDoneIfHaveCandidates();
}

void MyIpAddressImpl::TestResolvingHostname() {
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

// static
net::IPAddressList MyIpAddressImpl::GetSingleResultFavoringIPv4(
    const net::IPAddressList& ips) {
  for (const auto& ip : ips) {
    if (ip.IsIPv4())
      return {ip};
  }

  if (!ips.empty())
    return {ips.front()};

  return {};
}

}  // namespace network
