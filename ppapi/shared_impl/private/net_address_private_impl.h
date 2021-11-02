// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_PRIVATE_NET_ADDRESS_PRIVATE_IMPL_H_
#define PPAPI_SHARED_IMPL_PRIVATE_NET_ADDRESS_PRIVATE_IMPL_H_

#include <stdint.h>
#include <string>

#include "base/macros.h"
#include "net/base/ip_address.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/ppb_net_address.h"
#include "ppapi/shared_impl/ppapi_shared_export.h"

struct PP_NetAddress_Private;
struct sockaddr;

namespace ppapi {

class PPAPI_SHARED_EXPORT NetAddressPrivateImpl {
 public:
  static bool ValidateNetAddress(const PP_NetAddress_Private& addr);

  static bool SockaddrToNetAddress(const sockaddr* sa,
                                   uint32_t sa_length,
                                   PP_NetAddress_Private* net_addr);

  static bool IPEndPointToNetAddress(const net::IPAddressBytes& address,
                                     uint16_t port,
                                     PP_NetAddress_Private* net_addr);

  static bool NetAddressToIPEndPoint(const PP_NetAddress_Private& net_addr,
                                     net::IPAddressBytes* address,
                                     uint16_t* port);

  static std::string DescribeNetAddress(const PP_NetAddress_Private& addr,
                                        bool include_port);

  static void GetAnyAddress(PP_Bool is_ipv6, PP_NetAddress_Private* addr);

  // Conversion methods to make PPB_NetAddress resource work with
  // PP_NetAddress_Private.
  // TODO(yzshen): Remove them once PPB_NetAddress resource doesn't use
  // PP_NetAddress_Private as storage type.
  static void CreateNetAddressPrivateFromIPv4Address(
      const PP_NetAddress_IPv4& ipv4_addr,
      PP_NetAddress_Private* addr);
  static void CreateNetAddressPrivateFromIPv6Address(
      const PP_NetAddress_IPv6& ipv6_addr,
      PP_NetAddress_Private* addr);
  static PP_NetAddress_Family GetFamilyFromNetAddressPrivate(
      const PP_NetAddress_Private& addr);
  static bool DescribeNetAddressPrivateAsIPv4Address(
      const PP_NetAddress_Private& addr,
      PP_NetAddress_IPv4* ipv4_addr);
  static bool DescribeNetAddressPrivateAsIPv6Address(
      const PP_NetAddress_Private& addr,
      PP_NetAddress_IPv6* ipv6_addr);

  static const PP_NetAddress_Private kInvalidNetAddress;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(NetAddressPrivateImpl);
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_PRIVATE_NET_ADDRESS_PRIVATE_IMPL_H_
