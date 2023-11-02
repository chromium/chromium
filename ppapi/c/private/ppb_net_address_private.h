/* Copyright 2012 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From private/ppb_net_address_private.idl,
 *   modified Mon Jun 24 09:52:39 2013.
 */

#ifndef PPAPI_C_PRIVATE_PPB_NET_ADDRESS_PRIVATE_H_
#define PPAPI_C_PRIVATE_PPB_NET_ADDRESS_PRIVATE_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_var.h"

#define PPB_NETADDRESS_PRIVATE_INTERFACE_0_1 "PPB_NetAddress_Private;0.1"
#define PPB_NETADDRESS_PRIVATE_INTERFACE_1_0 "PPB_NetAddress_Private;1.0"
#define PPB_NETADDRESS_PRIVATE_INTERFACE_1_1 "PPB_NetAddress_Private;1.1"
#define PPB_NETADDRESS_PRIVATE_INTERFACE PPB_NETADDRESS_PRIVATE_INTERFACE_1_1

/**
 * @file
 * This file defines the <code>PPB_NetAddress_Private</code> interface.
 */


/**
 * @addtogroup Enums
 * @{
 */
typedef enum {
  /**
   * The address family is unspecified.
   */
  PP_NETADDRESSFAMILY_PRIVATE_UNSPECIFIED = 0,
  /**
   * The Internet Protocol version 4 (IPv4) address family.
   */
  PP_NETADDRESSFAMILY_PRIVATE_IPV4 = 1,
  /**
   * The Internet Protocol version 6 (IPv6) address family.
   */
  PP_NETADDRESSFAMILY_PRIVATE_IPV6 = 2
} PP_NetAddressFamily_Private;
PP_COMPILE_ASSERT_SIZE_IN_BYTES(PP_NetAddressFamily_Private, 4);
/**
 * @}
 */

/**
 * @addtogroup Structs
 * @{
 */
/**
 * This is an opaque type holding a network address. Plugins must
 * never access members of this struct directly.
 */
struct PP_NetAddress_Private {
  uint32_t size;
  int8_t data[128];
};
PP_COMPILE_ASSERT_STRUCT_SIZE_IN_BYTES(PP_NetAddress_Private, 132);
/**
 * @}
 */

/**
 * @addtogroup Interfaces
 * @{
 */
/**
 * The <code>PPB_NetAddress_Private</code> interface provides operations on
 * network addresses.
 */
struct PPB_NetAddress_Private_1_1 {
  /**
   * Returns PP_TRUE if the two addresses are equal (host and port).
   */
  PP_Bool (*AreEqual)(const struct PP_NetAddress_Private* addr1,
                      const struct PP_NetAddress_Private* addr2);
  /**
   * Returns PP_TRUE if the two addresses refer to the same host.
   */
  PP_Bool (*AreHostsEqual)(const struct PP_NetAddress_Private* addr1,
                           const struct PP_NetAddress_Private* addr2);
  /**
   * Returns a human-readable description of the network address, optionally
   * including the port (e.g., "192.168.0.1", "192.168.0.1:99", or "[::1]:80"),
   * or an undefined var on failure.
   */
  struct PP_Var (*Describe)(PP_Module module,
                            const struct PP_NetAddress_Private* addr,
                            PP_Bool include_port);
  /**
   * Replaces the port in the given source address. Returns PP_TRUE on success.
   */
  PP_Bool (*ReplacePort)(const struct PP_NetAddress_Private* src_addr,
                         uint16_t port,
                         struct PP_NetAddress_Private* addr_out);
  /**
   * Gets the "any" address (for IPv4 or IPv6); for use with UDP Bind.
   */
  void (*GetAnyAddress)(PP_Bool is_ipv6, struct PP_NetAddress_Private* addr);
  /**
   * Gets the address family.
   */
  PP_NetAddressFamily_Private (*GetFamily)(
      const struct PP_NetAddress_Private* addr);
  /**
   * Gets the port. The port is returned in host byte order.
   */
  uint16_t (*GetPort)(const struct PP_NetAddress_Private* addr);
  /**
   * Gets the address. The output, address, must be large enough for the
   * current socket family. The output will be the binary representation of an
   * address for the current socket family. For IPv4 and IPv6 the address is in
   * network byte order. PP_TRUE is returned if the address was successfully
   * retrieved.
   */
  PP_Bool (*GetAddress)(const struct PP_NetAddress_Private* addr,
                        void* address,
                        uint16_t address_size);
  /**
   * Returns ScopeID for IPv6 addresses or 0 for IPv4.
   */
  uint32_t (*GetScopeID)(const struct PP_NetAddress_Private* addr);
  /**
   * Creates NetAddress with the specified IPv4 address and port
   * number.
   */
  void (*CreateFromIPv4Address)(const uint8_t ip[4],
                                uint16_t port,
                                struct PP_NetAddress_Private* addr_out);
  /**
   * Creates NetAddress with the specified IPv6 address, scope_id and
   * port number.
   */
  void (*CreateFromIPv6Address)(const uint8_t ip[16],
                                uint32_t scope_id,
                                uint16_t port,
                                struct PP_NetAddress_Private* addr_out);
};

typedef struct PPB_NetAddress_Private_1_1 PPB_NetAddress_Private;

struct PPB_NetAddress_Private_0_1 {
  PP_Bool (*AreEqual)(const struct PP_NetAddress_Private* addr1,
                      const struct PP_NetAddress_Private* addr2);
  PP_Bool (*AreHostsEqual)(const struct PP_NetAddress_Private* addr1,
                           const struct PP_NetAddress_Private* addr2);
  struct PP_Var (*Describe)(PP_Module module,
                            const struct PP_NetAddress_Private* addr,
                            PP_Bool include_port);
  PP_Bool (*ReplacePort)(const struct PP_NetAddress_Private* src_addr,
                         uint16_t port,
                         struct PP_NetAddress_Private* addr_out);
  void (*GetAnyAddress)(PP_Bool is_ipv6, struct PP_NetAddress_Private* addr);
};

struct PPB_NetAddress_Private_1_0 {
  PP_Bool (*AreEqual)(const struct PP_NetAddress_Private* addr1,
                      const struct PP_NetAddress_Private* addr2);
  PP_Bool (*AreHostsEqual)(const struct PP_NetAddress_Private* addr1,
                           const struct PP_NetAddress_Private* addr2);
  struct PP_Var (*Describe)(PP_Module module,
                            const struct PP_NetAddress_Private* addr,
                            PP_Bool include_port);
  PP_Bool (*ReplacePort)(const struct PP_NetAddress_Private* src_addr,
                         uint16_t port,
                         struct PP_NetAddress_Private* addr_out);
  void (*GetAnyAddress)(PP_Bool is_ipv6, struct PP_NetAddress_Private* addr);
  PP_NetAddressFamily_Private (*GetFamily)(
      const struct PP_NetAddress_Private* addr);
  uint16_t (*GetPort)(const struct PP_NetAddress_Private* addr);
  PP_Bool (*GetAddress)(const struct PP_NetAddress_Private* addr,
                        void* address,
                        uint16_t address_size);
};
/**
 * @}
 */

#endif  /* PPAPI_C_PRIVATE_PPB_NET_ADDRESS_PRIVATE_H_ */

