// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_socket_allocator.h"

#include "base/logging.h"
#include "base/rand_util.h"
#include "build/build_config.h"
#include "net/base/address_list.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/log/net_log_source.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/datagram_client_socket.h"
#include "net/socket/stream_socket.h"

namespace net {

namespace {

// On Windows, can't request specific (random) ports, since that will trigger
// firewall prompts, so request default ones (but experimentally, the OS appears
// to still allocate random ports).
#if defined(OS_WIN)
const DatagramSocket::BindType kBindType = DatagramSocket::DEFAULT_BIND;
#else
const DatagramSocket::BindType kBindType = DatagramSocket::RANDOM_BIND;
#endif

}  // namespace

DnsSocketAllocator::DnsSocketAllocator(ClientSocketFactory* socket_factory,
                                       std::vector<IPEndPoint> nameservers,
                                       NetLog* net_log)
    : socket_factory_(socket_factory),
      net_log_(net_log),
      nameservers_(std::move(nameservers)) {
  DCHECK(socket_factory_);
}

DnsSocketAllocator::~DnsSocketAllocator() = default;

std::unique_ptr<DatagramClientSocket>
DnsSocketAllocator::CreateConnectedUdpSocket(size_t server_index,
                                             int* out_connection_error) {
  DCHECK_LT(server_index, nameservers_.size());
  DCHECK(out_connection_error);

  std::unique_ptr<DatagramClientSocket> socket;

  NetLogSource no_source;
  socket = socket_factory_->CreateDatagramClientSocket(kBindType, net_log_,
                                                       no_source);
  DCHECK(socket);

  *out_connection_error = socket->Connect(nameservers_[server_index]);
  if (*out_connection_error != OK) {
    DVLOG(1) << "Failed to connect socket: " << *out_connection_error;
    socket.reset();
  }

  return socket;
}

std::unique_ptr<StreamSocket> DnsSocketAllocator::CreateTcpSocket(
    size_t server_index,
    const NetLogSource& source) {
  DCHECK_LT(server_index, nameservers_.size());

  // TODO(https://crbug.com/1123197): Pass a non-null NetworkQualityEstimator.
  NetworkQualityEstimator* network_quality_estimator = nullptr;

  return socket_factory_->CreateTransportClientSocket(
      AddressList(nameservers_[server_index]), nullptr,
      network_quality_estimator, net_log_, source);
}

}  // namespace net
