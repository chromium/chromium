// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "services/network/p2p/socket_manager.h"

#include <stddef.h>

#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/not_fatal_until.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "net/base/address_family.h"
#include "net/base/address_list.h"
#include "net/base/net_errors.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/network_interfaces.h"
#include "net/base/sys_addrinfo.h"
#include "net/dns/dns_util.h"
#include "net/dns/host_resolver.h"
#include "net/http/http_network_session.h"
#include "net/log/net_log_source.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/client_socket_factory.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "services/network/p2p/socket.h"
#include "services/network/proxy_resolving_client_socket_factory.h"
#include "services/network/public/cpp/p2p_param_traits.h"
#include "third_party/webrtc/media/base/rtp_utils.h"
#include "third_party/webrtc/media/base/turn_utils.h"

namespace network {

namespace {

// Used by GetDefaultLocalAddress as the target to connect to for getting the
// default local address. They are public DNS servers on the internet.
const uint8_t kPublicIPv4Host[] = {8, 8, 8, 8};
const uint8_t kPublicIPv6Host[] = {
    0x20, 0x01, 0x48, 0x60, 0x48, 0x60, 0, 0, 0, 0, 0, 0, 0, 0, 0x88, 0x88};
const int kPublicPort = 53;  // DNS port.

// Experimentation shows that creating too many sockets creates odd problems
// because of resource exhaustion in the Unix sockets domain.
// Trouble has been seen on Linux at 3479 sockets in test, so leave a margin.
const int kMaxSimultaneousSockets = 3000;

const size_t kMinRtcpHeaderLength = 8;
const size_t kDtlsRecordHeaderLength = 13;

bool IsDtlsPacket(base::span<const uint8_t> data) {
  return data.size() >= kDtlsRecordHeaderLength &&
         (data[0] > 19 && data[0] < 64);
}

bool IsRtcpPacket(base::span<const uint8_t> data) {
  if (data.size() < kMinRtcpHeaderLength)
    return false;

  int type = data[1] & 0x7F;
  return type >= 64 && type < 96;
}

// Names ending in ".local." are link-local and used with Multicast DNS as
// described in RFC6762 (https://tools.ietf.org/html/rfc6762#section-3).
constexpr char kLocalTld[] = ".local.";

bool HasLocalTld(const std::string& host_name) {
  return EndsWith(host_name, kLocalTld, base::CompareCase::INSENSITIVE_ASCII);
}

net::DnsQueryType FamilyToDnsQueryType(int family) {
  return net::AddressFamilyToDnsQueryType(net::ToAddressFamily(family));
}

}  // namespace

DefaultLocalAddresses::DefaultLocalAddresses() = default;
DefaultLocalAddresses::~DefaultLocalAddresses() = default;

class P2PSocketManager::DnsRequest {
 public:
  using DoneCallback = base::OnceCallback<void(const net::IPAddressList&)>;

  DnsRequest(net::HostResolver* host_resolver, bool enable_mdns)
      : resolver_(host_resolver), enable_mdns_(enable_mdns) {}

  void Resolve(const std::string& host_name,
               std::optional<int> family,
               const net::NetworkAnonymizationKey& network_anonymization_key,
               DoneCallback done_callback) {
    DCHECK(!done_callback.is_null());

    host_name_ = host_name;
    done_callback_ = std::move(done_callback);

    // Return an error if it's an empty string.
    if (host_name_.empty()) {
      net::IPAddressList address_list;
      std::move(done_callback_).Run(address_list);
      return;
    }

    // Add period at the end to make sure that we only resolve
    // fully-qualified names.
    if (host_name_.back() != '.')
      host_name_ += '.';

    net::HostPortPair host(host_name_, 0);

    net::HostResolver::ResolveHostParameters parameters;
    if (enable_mdns_ && HasLocalTld(host_name_)) {
#if BUILDFLAG(ENABLE_MDNS)
      // HostResolver/MDnsClient expects a key without a trailing dot.
      host.set_host(host_name_.substr(0, host_name_.size() - 1));
      parameters.source = net::HostResolverSource::MULTICAST_DNS;
#endif  // ENABLE_MDNS
    }
    if (family.has_value()) {
      parameters.dns_query_type = FamilyToDnsQueryType(family.value());
    }
    request_ = resolver_->CreateRequest(host, network_anonymization_key,
                                        net::NetLogWithSource(), parameters);

    int result = request_->Start(base::BindOnce(
        &P2PSocketManager::DnsRequest::OnDone, base::Unretained(this)));
    if (result != net::ERR_IO_PENDING)
      OnDone(result);
  }

 private:
  void OnDone(int result) {
    net::IPAddressList list;
    const net::AddressList* addresses = request_->GetAddressResults();
    if (result != net::OK || !addresses) {
      LOG(ERROR) << "Failed to resolve address for " << host_name_
                 << ", errorcode: " << result;
      std::move(done_callback_).Run(list);
      return;
    }

    for (const auto& endpoint : *addresses) {
      list.push_back(endpoint.address());
    }
    std::move(done_callback_).Run(list);
  }

  std::string host_name_;
  raw_ptr<net::HostResolver> resolver_;
  std::unique_ptr<net::HostResolver::ResolveHostRequest> request_;

  DoneCallback done_callback_;

  const bool enable_mdns_;
};

P2PSocketManager::P2PSocketManager(
    const net::NetworkAnonymizationKey& network_anonymization_key,
    mojo::PendingRemote<mojom::P2PTrustedSocketManagerClient>
        trusted_socket_manager_client,
    mojo::PendingReceiver<mojom::P2PTrustedSocketManager>
        trusted_socket_manager_receiver,
    mojo::PendingReceiver<mojom::P2PSocketManager> socket_manager_receiver,
    DeleteCallback delete_callback,
    net::URLRequestContext* url_request_context)
    : delete_callback_(std::move(delete_callback)),
      url_request_context_(url_request_context),
      network_anonymization_key_(network_anonymization_key),
      network_list_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE})),
      trusted_socket_manager_client_(std::move(trusted_socket_manager_client)),
      trusted_socket_manager_receiver_(
          this,
          std::move(trusted_socket_manager_receiver)),
      socket_manager_receiver_(this, std::move(socket_manager_receiver)) {
  trusted_socket_manager_receiver_.set_disconnect_handler(base::BindOnce(
      &P2PSocketManager::OnConnectionError, base::Unretained(this)));
  socket_manager_receiver_.set_disconnect_handler(base::BindOnce(
      &P2PSocketManager::OnConnectionError, base::Unretained(this)));
}

P2PSocketManager::~P2PSocketManager() {
  // Reset the P2PSocketManager receiver before dropping pending dns requests.
  socket_manager_receiver_.reset();

  sockets_.clear();
  dns_requests_.clear();

  if (network_notification_client_)
    net::NetworkChangeNotifier::RemoveNetworkChangeObserver(this);

  proxy_resolving_socket_factory_.reset();
}

void P2PSocketManager::OnNetworkChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  // NetworkChangeNotifier always emits CONNECTION_NONE notification whenever
  // network configuration changes. All other notifications can be ignored.
  if (type != net::NetworkChangeNotifier::CONNECTION_NONE)
    return;
  if (notifications_paused_) {
    pending_network_change_notification_ = true;
    return;
  }

  // Notify the renderer about changes to list of network interfaces.
  network_list_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&P2PSocketManager::DoGetNetworkList),
      base::BindOnce(&P2PSocketManager::DoGetDefaultLocalAddresses,
                     weak_factory_.GetWeakPtr()));
}

void P2PSocketManager::PauseNetworkChangeNotifications() {
  notifications_paused_ = true;
}

void P2PSocketManager::ResumeNetworkChangeNotifications() {
  notifications_paused_ = false;
  if (pending_network_change_notification_) {
    pending_network_change_notification_ = false;
    OnNetworkChanged(net::NetworkChangeNotifier::CONNECTION_NONE);
  }
}

void P2PSocketManager::AddAcceptedConnection(
    std::unique_ptr<P2PSocket> accepted_connection) {
  sockets_[accepted_connection.get()] = std::move(accepted_connection);
}

void P2PSocketManager::DestroySocket(P2PSocket* socket) {
  auto iter = sockets_.find(socket);
  CHECK(iter != sockets_.end(), base::NotFatalUntil::M130);
  sockets_.erase(iter);
}

void P2PSocketManager::DumpPacket(base::span<const uint8_t> packet,
                                  bool incoming) {
  if ((incoming && !dump_incoming_rtp_packet_) ||
      (!incoming && !dump_outgoing_rtp_packet_)) {
    return;
  }

  if (IsDtlsPacket(packet) || IsRtcpPacket(packet))
    return;

  size_t rtp_packet_pos = 0;
  size_t rtp_packet_size = packet.size();
  if (!cricket::UnwrapTurnPacket(packet.data(), packet.size(), &rtp_packet_pos,
                                 &rtp_packet_size)) {
    return;
  }

  auto rtp_packet = packet.subspan(rtp_packet_pos, rtp_packet_size);

  size_t header_size = 0;
  bool valid = cricket::ValidateRtpHeader(rtp_packet.data(), rtp_packet.size(),
                                          &header_size);
  if (!valid) {
    NOTREACHED_IN_MIGRATION();
    return;
  }

  std::vector<uint8_t> header_buffer(rtp_packet.data(),
                                     rtp_packet.data() + header_size);
  trusted_socket_manager_client_->DumpPacket(header_buffer, rtp_packet.size(),
                                             incoming);
}

net::NetworkInterfaceList P2PSocketManager::DoGetNetworkList() {
  net::NetworkInterfaceList list;
  if (!net::GetNetworkList(&list, net::EXCLUDE_HOST_SCOPE_VIRTUAL_INTERFACES)) {
    LOG(ERROR) << "GetNetworkList failed.";
  }
  return list;
}

void P2PSocketManager::DoGetDefaultLocalAddresses(
    net::NetworkInterfaceList list) {
  DefaultLocalAddresses* default_local_addresses = new DefaultLocalAddresses();
  GetDefaultLocalAddress(
      AF_INET,
      base::BindOnce(&P2PSocketManager::MaybeFinishDoGetDefaultLocalAddresses,
                     weak_factory_.GetWeakPtr(), default_local_addresses, list,
                     AF_INET));
  GetDefaultLocalAddress(
      AF_INET6,
      base::BindOnce(&P2PSocketManager::MaybeFinishDoGetDefaultLocalAddresses,
                     weak_factory_.GetWeakPtr(), default_local_addresses, list,
                     AF_INET6));
}

void P2PSocketManager::MaybeFinishDoGetDefaultLocalAddresses(
    DefaultLocalAddresses* default_local_addresses,
    net::NetworkInterfaceList list,
    int family,
    net::IPAddress addr) {
  if (family == AF_INET) {
    default_local_addresses->default_ipv4_local_address = addr;
  } else {
    default_local_addresses->default_ipv6_local_address = addr;
  }

  if (!default_local_addresses->default_ipv6_local_address.has_value() ||
      !default_local_addresses->default_ipv4_local_address.has_value()) {
    return;
  }

  SendNetworkList(list,
                  default_local_addresses->default_ipv4_local_address.value(),
                  default_local_addresses->default_ipv6_local_address.value());
  delete default_local_addresses;
}

void P2PSocketManager::GetDefaultLocalAddress(int family,
                                              GetDefaultCallback callback) {
  DCHECK(family == AF_INET || family == AF_INET6);

  auto socket =
      url_request_context_->GetNetworkSessionContext()
          ->client_socket_factory->CreateDatagramClientSocket(
              net::DatagramSocket::DEFAULT_BIND, nullptr, net::NetLogSource());

  net::IPAddress ip_address;
  if (family == AF_INET) {
    ip_address = net::IPAddress(kPublicIPv4Host);
  } else {
    ip_address = net::IPAddress(kPublicIPv6Host);
  }

  auto* socket_ptr = socket.get();
  auto split_connect_callback = base::SplitOnceCallback(base::BindOnce(
      &P2PSocketManager::FinishGetDefaultLocalAddress,
      weak_factory_.GetWeakPtr(), std::move(socket), std::move(callback)));
  int rv = socket_ptr->ConnectAsync(net::IPEndPoint(ip_address, kPublicPort),
                                    std::move(split_connect_callback.first));
  // If ConnectAsync returns synchronously then it will never run the callback
  // that was passed in, so run the callback here to make sure
  // FinishGetDefaultLocalAddress runs.
  if (rv != net::ERR_IO_PENDING) {
    std::move(split_connect_callback.second).Run(rv);
  }
}

void P2PSocketManager::FinishGetDefaultLocalAddress(
    std::unique_ptr<net::DatagramClientSocket> socket,
    GetDefaultCallback callback,
    int result) {
  if (result != net::OK) {
    std::move(callback).Run(net::IPAddress());
    return;
  }

  net::IPEndPoint local_address;
  if (socket->GetLocalAddress(&local_address) != net::OK) {
    std::move(callback).Run(net::IPAddress());
    return;
  }

  std::move(callback).Run(local_address.address());
}

void P2PSocketManager::SendNetworkList(
    const net::NetworkInterfaceList& list,
    net::IPAddress default_ipv4_local_address,
    net::IPAddress default_ipv6_local_address) {
  network_notification_client_->NetworkListChanged(
      list, default_ipv4_local_address, default_ipv6_local_address);
}

void P2PSocketManager::StartNetworkNotifications(
    mojo::PendingRemote<mojom::P2PNetworkNotificationClient> client) {
  DCHECK(!network_notification_client_);
  network_notification_client_.Bind(std::move(client));
  network_notification_client_.set_disconnect_handler(base::BindOnce(
      &P2PSocketManager::NetworkNotificationClientConnectionError,
      base::Unretained(this)));

  net::NetworkChangeNotifier::AddNetworkChangeObserver(this);

  OnNetworkChanged(net::NetworkChangeNotifier::CONNECTION_NONE);
}

void P2PSocketManager::GetHostAddress(
    const std::string& host_name,
    bool enable_mdns,
    mojom::P2PSocketManager::GetHostAddressCallback callback) {
  DoGetHostAddress(host_name, /*address_family=*/std::nullopt, enable_mdns,
                   std::move(callback));
}

void P2PSocketManager::GetHostAddressWithFamily(
    const std::string& host_name,
    int address_family,
    bool enable_mdns,
    mojom::P2PSocketManager::GetHostAddressCallback callback) {
  DoGetHostAddress(host_name, std::make_optional(address_family), enable_mdns,
                   std::move(callback));
}

void P2PSocketManager::DoGetHostAddress(
    const std::string& host_name,
    std::optional<int> address_family,
    bool enable_mdns,
    mojom::P2PSocketManager::GetHostAddressCallback callback) {
  auto request = std::make_unique<DnsRequest>(
      url_request_context_->host_resolver(), enable_mdns);
  DnsRequest* request_ptr = request.get();
  dns_requests_.insert(std::move(request));
  request_ptr->Resolve(
      host_name, address_family, network_anonymization_key_,
      base::BindOnce(&P2PSocketManager::OnAddressResolved,
                     base::Unretained(this), request_ptr, std::move(callback)));
}

void P2PSocketManager::CreateSocket(
    P2PSocketType type,
    const net::IPEndPoint& local_address,
    const P2PPortRange& port_range,
    const P2PHostAndIPEndPoint& remote_address,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
    const std::optional<base::UnguessableToken>& devtools_token,
    mojo::PendingRemote<mojom::P2PSocketClient> client,
    mojo::PendingReceiver<mojom::P2PSocket> receiver) {
  if (port_range.min_port > port_range.max_port ||
      (port_range.min_port == 0 && port_range.max_port != 0)) {
    trusted_socket_manager_client_->InvalidSocketPortRangeRequested();
    return;
  }

  if (!proxy_resolving_socket_factory_) {
    proxy_resolving_socket_factory_ =
        std::make_unique<ProxyResolvingClientSocketFactory>(
            url_request_context_);
  }
  if (sockets_.size() > kMaxSimultaneousSockets) {
    LOG(ERROR) << "Too many sockets created";
    return;
  }
  std::unique_ptr<P2PSocket> socket = P2PSocket::Create(
      this, std::move(client), std::move(receiver), type,
      net::NetworkTrafficAnnotationTag(traffic_annotation),
      url_request_context_->net_log(), proxy_resolving_socket_factory_.get(),
      &throttler_, devtools_token);

  if (!socket)
    return;

  P2PSocket* socket_ptr = socket.get();
  sockets_[socket_ptr] = std::move(socket);

  // Init() may call SocketManager::DestroySocket(), so it must be called after
  // adding the socket to |sockets_|.
  socket_ptr->Init(local_address, port_range.min_port, port_range.max_port,
                   remote_address, network_anonymization_key_);
}

void P2PSocketManager::StartRtpDump(bool incoming, bool outgoing) {
  dump_incoming_rtp_packet_ |= incoming;
  dump_outgoing_rtp_packet_ |= outgoing;
}

void P2PSocketManager::StopRtpDump(bool incoming, bool outgoing) {
  dump_incoming_rtp_packet_ &= !incoming;
  dump_outgoing_rtp_packet_ &= !outgoing;
}

void P2PSocketManager::NetworkNotificationClientConnectionError() {
  net::NetworkChangeNotifier::RemoveNetworkChangeObserver(this);
}

void P2PSocketManager::OnAddressResolved(
    DnsRequest* request,
    mojom::P2PSocketManager::GetHostAddressCallback callback,
    const net::IPAddressList& addresses) {
  std::move(callback).Run(addresses);

  dns_requests_.erase(dns_requests_.find(request));
}

void P2PSocketManager::OnConnectionError() {
  std::move(delete_callback_).Run(this);
}

}  // namespace network
