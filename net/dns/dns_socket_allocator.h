// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_DNS_SOCKET_ALLOCATOR_H_
#define NET_DNS_DNS_SOCKET_ALLOCATOR_H_

#include <memory>
#include <vector>

#include "net/base/net_export.h"
#include "net/base/rand_callback.h"

namespace net {

class ClientSocketFactory;
class DatagramClientSocket;
class IPEndPoint;
class NetLog;
struct NetLogSource;
class StreamSocket;

// Allocation logic for DNS UDP and TCP sockets.
class NET_EXPORT_PRIVATE DnsSocketAllocator {
 public:
  DnsSocketAllocator(ClientSocketFactory* factory,
                     std::vector<IPEndPoint> nameservers,
                     NetLog* net_log);
  ~DnsSocketAllocator();

  DnsSocketAllocator(const DnsSocketAllocator&) = delete;
  DnsSocketAllocator& operator=(const DnsSocketAllocator&) = delete;

  // Creates a UDP client socket that is already connected to the nameserver
  // referenced by |server_index| and sets |out_connection_error| to the result
  // of the connection. On error connecting the socket, returns null.
  std::unique_ptr<DatagramClientSocket> CreateConnectedUdpSocket(
      size_t server_index,
      int* out_connection_error);

  // Creates a StreamSocket for TCP to the nameserver referenced by
  // |server_index|. Does not connect the seocket.
  std::unique_ptr<StreamSocket> CreateTcpSocket(size_t server_index,
                                                const NetLogSource& source);

 private:
  ClientSocketFactory* const socket_factory_;
  NetLog* const net_log_;
  const std::vector<IPEndPoint> nameservers_;
};

}  // namespace net

#endif  // NET_DNS_DNS_SOCKET_ALLOCATOR_H_
