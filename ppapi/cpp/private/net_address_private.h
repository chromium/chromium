// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_PRIVATE_NET_ADDRESS_PRIVATE_H_
#define PPAPI_CPP_PRIVATE_NET_ADDRESS_PRIVATE_H_

#include <string>

#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/private/ppb_net_address_private.h"

namespace pp {

class NetAddressPrivate {
 public:
  // Returns true if the required interface is available.
  static bool IsAvailable();

  static bool AreEqual(const PP_NetAddress_Private& addr1,
                       const PP_NetAddress_Private& addr2);
  static bool AreHostsEqual(const PP_NetAddress_Private& addr1,
                            const PP_NetAddress_Private& addr2);
  static std::string Describe(const PP_NetAddress_Private& addr,
                              bool include_port);
  static bool ReplacePort(const PP_NetAddress_Private& addr_in,
                          uint16_t port,
                          PP_NetAddress_Private* addr_out);
  static bool GetAnyAddress(bool is_ipv6, PP_NetAddress_Private* addr);
  static PP_NetAddressFamily_Private GetFamily(
      const PP_NetAddress_Private& addr);
  static uint16_t GetPort(const PP_NetAddress_Private& addr);
  static bool GetAddress(const PP_NetAddress_Private& addr,
                         void* address,
                         uint16_t address_size);
  static uint32_t GetScopeID(const PP_NetAddress_Private& addr);
  static bool CreateFromIPv4Address(const uint8_t ip[4],
                                    uint16_t port,
                                    struct PP_NetAddress_Private* addr_out);
  static bool CreateFromIPv6Address(const uint8_t ip[16],
                                    uint32_t scope_id,
                                    uint16_t port,
                                    struct PP_NetAddress_Private* addr_out);
};

}  // namespace pp

#endif  // PPAPI_CPP_PRIVATE_NET_ADDRESS_PRIVATE_H_
