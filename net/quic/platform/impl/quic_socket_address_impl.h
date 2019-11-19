// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_PLATFORM_IMPL_QUIC_SOCKET_ADDRESS_IMPL_H_
#define NET_QUIC_PLATFORM_IMPL_QUIC_SOCKET_ADDRESS_IMPL_H_

#include "net/base/ip_endpoint.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_export.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_ip_address.h"

namespace quic {

class QUIC_EXPORT_PRIVATE QuicSocketAddressImpl {
 public:
  QuicSocketAddressImpl() = default;
  explicit QuicSocketAddressImpl(const net::IPEndPoint& addr);
  QuicSocketAddressImpl(QuicIpAddress address, uint16_t port);
  explicit QuicSocketAddressImpl(const struct sockaddr_storage& saddr);
  explicit QuicSocketAddressImpl(const sockaddr* saddr, socklen_t len);
  QuicSocketAddressImpl(const QuicSocketAddressImpl& other) = default;
  QuicSocketAddressImpl& operator=(const QuicSocketAddressImpl& other) =
      default;
  QuicSocketAddressImpl& operator=(QuicSocketAddressImpl&& other) = default;
  friend bool operator==(const QuicSocketAddressImpl& lhs,
                         const QuicSocketAddressImpl& rhs);
  friend bool operator!=(const QuicSocketAddressImpl& lhs,
                         const QuicSocketAddressImpl& rhs);

  bool IsInitialized() const;
  std::string ToString() const;
  int FromSocket(int fd);
  QuicSocketAddressImpl Normalized() const;

  QuicIpAddress host() const;
  uint16_t port() const;

  sockaddr_storage generic_address() const;
  const net::IPEndPoint& socket_address() const { return socket_address_; }

 private:
  net::IPEndPoint socket_address_;
};

}  // namespace quic

#endif  // NET_QUIC_PLATFORM_IMPL_QUIC_SOCKET_ADDRESS_IMPL_H_
