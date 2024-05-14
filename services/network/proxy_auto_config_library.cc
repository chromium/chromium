// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/proxy_auto_config_library.h"

#include <list>
#include <set>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "net/base/address_list.h"
#include "net/base/ip_address.h"
#include "net/base/network_interfaces.h"
#include "net/dns/host_resolver_proc.h"
#include "net/dns/host_resolver_system_task.h"
#include "net/log/net_log_source.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/datagram_client_socket.h"

namespace network {

MyIpAddressImpl::MyIpAddressImpl(Mode mode) : mode_(mode) {
  my_ip_address_clients_.set_disconnect_handler(base::BindRepeating(
      &MyIpAddressImpl::ClientDisconnected, base::Unretained(this)));
}

MyIpAddressImpl::~MyIpAddressImpl() = default;

void MyIpAddressImpl::AddRequest(
    mojo::PendingRemote<proxy_resolver::mojom::HostResolverRequestClient>
        my_ip_address_client) {
  bool was_running = !my_ip_address_clients_.empty();
  my_ip_address_clients_.Add(std::move(my_ip_address_client));
  if (was_running)
    return;

  DCHECK(seen_ips_.empty());
  DCHECK(candidate_ips_.empty());
  DCHECK(link_local_ips_.empty());
  DCHECK(!done_);

  // Start by trying source IPs of sockets connected to public internet IPs.
  next_state_ = State::kConnectSocketsPublicInternetRoutes;
  DoLoop();
}

void MyIpAddressImpl::SetSocketFactoryForTest(
    net::ClientSocketFactory* socket_factory) {
  override_socket_factory_ = socket_factory;
}

void MyIpAddressImpl::SetHostResolverProcForTest(
    scoped_refptr<net::HostResolverProc> host_resolver_proc) {
  host_resolver_proc_ = std::move(host_resolver_proc);
}

void MyIpAddressImpl::DoLoop() {
  DCHECK_NE(next_state_, State::kNone);
  int rv = net::OK;
  do {
    State state = next_state_;
    next_state_ = State::kNone;
    switch (state) {
      case State::kNone:
        NOTREACHED_IN_MIGRATION() << "bad state";
        rv = net::ERR_FAILED;
        break;
      case State::kConnectSocketsPublicInternetRoutes:
        rv = DoConnectSocketsPublicInternetRoutes();
        break;
      case State::kTestResolvingHostname:
        rv = DoTestResolvingHostname();
        break;
      case State::kConnectSocketsPrivateIpRoutes:
        rv = DoConnectSocketsPrivateIPRoutes();
        break;
      case State::kSendResultsAndReset:
        rv = DoSendResultsAndReset();
        break;
    }
  } while (rv != net::ERR_IO_PENDING && next_state_ != State::kNone);
}

int MyIpAddressImpl::DoConnectSocketsPublicInternetRoutes() {
  DCHECK(!done_);

  next_state_ = State::kTestResolvingHostname;
  // 8.8.8.8 and 2001:4860:4860::8888 are Google DNS.
  return DoConnectSockets({net::IPAddress(8, 8, 8, 8),
                           net::IPAddress(0x20, 0x01, 0x48, 0x60, 0x48, 0x60, 0,
                                          0, 0, 0, 0, 0, 0, 0, 0x88, 0x88)});
}

int MyIpAddressImpl::DoTestResolvingHostname() {
  DCHECK(!done_);

  next_state_ = State::kConnectSocketsPrivateIpRoutes;

  net::HostResolverSystemTask::Params task_params(host_resolver_proc_,
                                                  /*max_retry_attempts=*/0);
  auto system_dns_resolution_task =
      net::HostResolverSystemTask::CreateForOwnHostname(
          net::AddressFamily::ADDRESS_FAMILY_UNSPECIFIED,
          /*flags=*/0, std::move(task_params));
  net::HostResolverSystemTask* system_dns_resolution_task_ptr =
      system_dns_resolution_task.get();
  system_dns_resolution_task_ptr->Start(base::BindOnce(
      &MyIpAddressImpl::ReceiveDnsResults, weak_ptr_factory_.GetWeakPtr(),
      std::move(system_dns_resolution_task)));

  return net::ERR_IO_PENDING;
}

int MyIpAddressImpl::DoConnectSocketsPrivateIPRoutes() {
  DCHECK(!done_);

  // There are no other strategies to try after connecting these sockets, so
  // transition to State::kSendResultsAndReset even if !done_.
  next_state_ = State::kSendResultsAndReset;

  return DoConnectSockets({
      // Representative IP from each range in RFC 1918.
      net::IPAddress(10, 0, 0, 0),
      net::IPAddress(172, 16, 0, 0),
      net::IPAddress(192, 168, 0, 0),
      // Representative IP for Unique Local Address (FC00::/7).
      net::IPAddress(0xfc, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
  });
}

int MyIpAddressImpl::DoSendResultsAndReset() {
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

  Reset();

  return net::OK;
}

void MyIpAddressImpl::Reset() {
  my_ip_address_clients_.Clear();
  seen_ips_.clear();
  socket_results_.clear();
  candidate_ips_.clear();
  link_local_ips_.clear();
  done_ = false;
  next_state_ = State::kNone;
  // We no longer care about results from ongoing async operations.
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void MyIpAddressImpl::ClientDisconnected(mojo::RemoteSetElementId /*id*/) {
  if (my_ip_address_clients_.empty())
    Reset();
}

struct MyIpAddressImpl::SocketConnectionResult {
  // Holds the result of the connection attempt. If the socket is connecting
  // asynchronously this will be net::ERR_IO_PENDING until the connection
  // completes.
  int net_error = net::OK;
  std::unique_ptr<net::DatagramClientSocket> socket;
};

int MyIpAddressImpl::DoConnectSockets(
    std::vector<net::IPAddress> destination_ips) {
  net::ClientSocketFactory* socket_factory =
      override_socket_factory_ ? override_socket_factory_.get()
                               : net::ClientSocketFactory::GetDefaultFactory();

  DCHECK(socket_results_.empty());

  // Connect a socket for each ip in `destination_ips` and store them in the
  // `socket_results_` queue in the same order as `destination_ips`.
  for (const auto& destination_ip : destination_ips) {
    SocketConnectionResult& socket_result = socket_results_.emplace_back();

    socket_result.socket = socket_factory->CreateDatagramClientSocket(
        net::DatagramSocket::DEFAULT_BIND, nullptr, net::NetLogSource());

    net::IPEndPoint destination(destination_ip, /*port=*/80);

    socket_result.net_error = socket_result.socket->ConnectAsync(
        destination, base::BindOnce(&MyIpAddressImpl::OnConnectedSocket,
                                    weak_ptr_factory_.GetWeakPtr(),
                                    std::ref(socket_result)));
  }

  return DoProcessConnectedSockets();
}

int MyIpAddressImpl::DoProcessConnectedSockets() {
  while (!done_ && !socket_results_.empty()) {
    SocketConnectionResult& socket_result = socket_results_.front();
    // The next socket in the queue has not yet finished connecting. Return
    // net::ERR_IO_PENDING and defer processing until later.
    if (socket_result.net_error == net::ERR_IO_PENDING)
      return net::ERR_IO_PENDING;

    // If the socket was successfully connected, get its local address and
    // Add() it to the candidates.
    if (socket_result.net_error == net::OK) {
      DCHECK(socket_result.socket);
      net::IPEndPoint source;
      int rv = socket_result.socket->GetLocalAddress(&source);
      if (rv != net::OK)
        continue;

      // Add() may set `done_` in some situations, which will exit the loop.
      Add(source.address());
    }

    socket_results_.pop_front();
  }

  MarkAsDoneIfHaveCandidates();
  return net::OK;
}

void MyIpAddressImpl::OnConnectedSocket(SocketConnectionResult& socket_result,
                                        int result) {
  socket_result.net_error = result;

  int rv = DoProcessConnectedSockets();
  if (rv != net::ERR_IO_PENDING) {
    // Done receiving connected sockets and gathering results.
    DoLoop();
  }
}

void MyIpAddressImpl::ReceiveDnsResults(
    std::unique_ptr<net::HostResolverSystemTask> /*system_dns_resolution_task*/,
    const net::AddressList& addrlist,
    int /*os_error*/,
    int net_error) {
  if (net_error == net::OK) {
    for (const auto& e : addrlist.endpoints())
      Add(e.address());
  }

  // May set `next_state_` to kSendResultsAndReset.
  MarkAsDoneIfHaveCandidates();
  DoLoop();
}

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

void MyIpAddressImpl::MarkAsDoneIfHaveCandidates() {
  if (!candidate_ips_.empty() || done_) {
    done_ = true;
    next_state_ = State::kSendResultsAndReset;
  }
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
