// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_socket_pool.h"

#include "base/logging.h"
#include "base/macros.h"
#include "base/rand_util.h"
#include "net/base/address_list.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/log/net_log_source.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/datagram_client_socket.h"
#include "net/socket/stream_socket.h"

namespace net {

namespace {

// When we initialize the SocketPool, we allocate kInitialPoolSize sockets.
// When we allocate a socket, we ensure we have at least kAllocateMinSize
// sockets to choose from.  Freed sockets are not retained.

// On Windows, we can't request specific (random) ports, since that will
// trigger firewall prompts, so request default ones, but keep a pile of
// them.  Everywhere else, request fresh, random ports each time.
#if defined(OS_WIN)
const DatagramSocket::BindType kBindType = DatagramSocket::DEFAULT_BIND;
const unsigned kInitialPoolSize = 256;
const unsigned kAllocateMinSize = 256;
#else
const DatagramSocket::BindType kBindType = DatagramSocket::RANDOM_BIND;
const unsigned kInitialPoolSize = 0;
const unsigned kAllocateMinSize = 1;
#endif

} // namespace

DnsSocketPool::DnsSocketPool(ClientSocketFactory* socket_factory,
                             const RandIntCallback& rand_int_callback)
    : socket_factory_(socket_factory),
      rand_int_callback_(rand_int_callback),
      net_log_(nullptr),
      nameservers_(nullptr),
      initialized_(false) {}

void DnsSocketPool::InitializeInternal(
    const std::vector<IPEndPoint>* nameservers,
    NetLog* net_log) {
  DCHECK(nameservers);
  DCHECK(!initialized_);

  net_log_ = net_log;
  nameservers_ = nameservers;
  initialized_ = true;
}

std::unique_ptr<StreamSocket> DnsSocketPool::CreateTCPSocket(
    unsigned server_index,
    const NetLogSource& source) {
  DCHECK_LT(server_index, nameservers_->size());

  return std::unique_ptr<StreamSocket>(
      socket_factory_->CreateTransportClientSocket(
          AddressList((*nameservers_)[server_index]), nullptr, net_log_,
          source));
}

std::unique_ptr<DatagramClientSocket> DnsSocketPool::CreateConnectedSocket(
    unsigned server_index) {
  DCHECK_LT(server_index, nameservers_->size());

  std::unique_ptr<DatagramClientSocket> socket;

  NetLogSource no_source;
  socket = socket_factory_->CreateDatagramClientSocket(kBindType, net_log_,
                                                       no_source);

  if (socket.get()) {
    int rv = socket->Connect((*nameservers_)[server_index]);
    if (rv != OK) {
      DVLOG(1) << "Failed to connect socket: " << rv;
      socket.reset();
    }
  } else {
    DVLOG(1) << "Failed to create socket.";
  }

  return socket;
}

int DnsSocketPool::GetRandomInt(int min, int max) {
  return rand_int_callback_.Run(min, max);
}

class NullDnsSocketPool : public DnsSocketPool {
 public:
  NullDnsSocketPool(ClientSocketFactory* factory,
                    const RandIntCallback& rand_int_callback)
      : DnsSocketPool(factory, rand_int_callback) {}

  void Initialize(const std::vector<IPEndPoint>* nameservers,
                  NetLog* net_log) override {
    InitializeInternal(nameservers, net_log);
  }

  std::unique_ptr<DatagramClientSocket> AllocateSocket(
      unsigned server_index) override {
    return CreateConnectedSocket(server_index);
  }

  void FreeSocket(unsigned server_index,
                  std::unique_ptr<DatagramClientSocket> socket) override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(NullDnsSocketPool);
};

// static
std::unique_ptr<DnsSocketPool> DnsSocketPool::CreateNull(
    ClientSocketFactory* factory,
    const RandIntCallback& rand_int_callback) {
  return std::unique_ptr<DnsSocketPool>(
      new NullDnsSocketPool(factory, rand_int_callback));
}

class DefaultDnsSocketPool : public DnsSocketPool {
 public:
  DefaultDnsSocketPool(ClientSocketFactory* factory,
                       const RandIntCallback& rand_int_callback)
      : DnsSocketPool(factory, rand_int_callback) {}

  ~DefaultDnsSocketPool() override;

  void Initialize(const std::vector<IPEndPoint>* nameservers,
                  NetLog* net_log) override;

  std::unique_ptr<DatagramClientSocket> AllocateSocket(
      unsigned server_index) override;

  void FreeSocket(unsigned server_index,
                  std::unique_ptr<DatagramClientSocket> socket) override;

 private:
  void FillPool(unsigned server_index, unsigned size);

  typedef std::vector<std::unique_ptr<DatagramClientSocket>> SocketVector;

  std::vector<SocketVector> pools_;

  DISALLOW_COPY_AND_ASSIGN(DefaultDnsSocketPool);
};

DnsSocketPool::~DnsSocketPool() = default;

// static
std::unique_ptr<DnsSocketPool> DnsSocketPool::CreateDefault(
    ClientSocketFactory* factory,
    const RandIntCallback& rand_int_callback) {
  return std::unique_ptr<DnsSocketPool>(
      new DefaultDnsSocketPool(factory, rand_int_callback));
}

void DefaultDnsSocketPool::Initialize(
    const std::vector<IPEndPoint>* nameservers,
    NetLog* net_log) {
  InitializeInternal(nameservers, net_log);

  DCHECK(pools_.empty());
  const unsigned num_servers = nameservers->size();
  pools_.resize(num_servers);
  for (unsigned server_index = 0; server_index < num_servers; ++server_index)
    FillPool(server_index, kInitialPoolSize);
}

DefaultDnsSocketPool::~DefaultDnsSocketPool() = default;

std::unique_ptr<DatagramClientSocket> DefaultDnsSocketPool::AllocateSocket(
    unsigned server_index) {
  DCHECK_LT(server_index, pools_.size());
  SocketVector& pool = pools_[server_index];

  FillPool(server_index, kAllocateMinSize);
  if (pool.size() == 0) {
    DVLOG(1) << "No DNS sockets available in pool " << server_index << "!";
    return std::unique_ptr<DatagramClientSocket>();
  }

  if (pool.size() < kAllocateMinSize) {
    DVLOG(1) << "Low DNS port entropy: wanted " << kAllocateMinSize
             << " sockets to choose from, but only have " << pool.size()
             << " in pool " << server_index << ".";
  }

  unsigned socket_index = GetRandomInt(0, pool.size() - 1);
  std::unique_ptr<DatagramClientSocket> socket = std::move(pool[socket_index]);
  pool[socket_index] = std::move(pool.back());
  pool.pop_back();

  return socket;
}

void DefaultDnsSocketPool::FreeSocket(
    unsigned server_index,
    std::unique_ptr<DatagramClientSocket> socket) {
  DCHECK_LT(server_index, pools_.size());
}

void DefaultDnsSocketPool::FillPool(unsigned server_index, unsigned size) {
  SocketVector& pool = pools_[server_index];

  for (unsigned pool_index = pool.size(); pool_index < size; ++pool_index) {
    std::unique_ptr<DatagramClientSocket> socket =
        CreateConnectedSocket(server_index);
    if (!socket)
      break;
    pool.push_back(std::move(socket));
  }
}

} // namespace net
