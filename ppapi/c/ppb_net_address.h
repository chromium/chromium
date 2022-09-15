/* Copyright 2013 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From ppb_net_address.idl modified Sat Jun 22 10:14:31 2013. */

#ifndef PPAPI_C_PPB_NET_ADDRESS_H_
#define PPAPI_C_PPB_NET_ADDRESS_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_var.h"

#define PPB_NETADDRESS_INTERFACE_1_0 "PPB_NetAddress;1.0"
#define PPB_NETADDRESS_INTERFACE PPB_NETADDRESS_INTERFACE_1_0

/**
 * @file
 * This file defines the <code>PPB_NetAddress</code> interface.
 */


/**
 * @addtogroup Enums
 * @{
 */
/**
 * Network address family types.
 */
typedef enum {
  /**
   * The address family is unspecified.
   */
  PP_NETADDRESS_FAMILY_UNSPECIFIED = 0,
  /**
   * The Internet Protocol version 4 (IPv4) address family.
   */
  PP_NETADDRESS_FAMILY_IPV4 = 1,
  /**
   * The Internet Protocol version 6 (IPv6) address family.
   */
  PP_NETADDRESS_FAMILY_IPV6 = 2
} PP_NetAddress_Family;
PP_COMPILE_ASSERT_SIZE_IN_BYTES(PP_NetAddress_Family, 4);
/**
 * @}
 */

/**
 * @addtogroup Structs
 * @{
 */
/**
 * All members are expressed in network byte order.
 */
struct PP_NetAddress_IPv4 {
  /**
   * Port number.
   */
  uint16_t port;
  /**
   * IPv4 address.
   */
  uint8_t addr[4];
};
PP_COMPILE_ASSERT_STRUCT_SIZE_IN_BYTES(PP_NetAddress_IPv4, 6);

/**
 * All members are expressed in network byte order.
 */
struct PP_NetAddress_IPv6 {
  /**
   * Port number.
   */
  uint16_t port;
  /**
   * IPv6 address.
   */
  uint8_t addr[16];
};
PP_COMPILE_ASSERT_STRUCT_SIZE_IN_BYTES(PP_NetAddress_IPv6, 18);
/**
 * @}
 */

/**
 * @addtogroup Interfaces
 * @{
 */
/**
 * The <code>PPB_NetAddress</code> interface provides operations on network
 * addresses.
 */
struct PPB_NetAddress_1_0 {
  /**
   * Creates a <code>PPB_NetAddress</code> resource with the specified IPv4
   * address.
   *
   * @param[in] instance A <code>PP_Instance</code> identifying one instance of
   * a module.
   * @param[in] ipv4_addr An IPv4 address.
   *
   * @return A <code>PP_Resource</code> representing the same address as
   * <code>ipv4_addr</code> or 0 on failure.
   */
  PP_Resource (*CreateFromIPv4Address)(
      PP_Instance instance,
      const struct PP_NetAddress_IPv4* ipv4_addr);
  /**
   * Creates a <code>PPB_NetAddress</code> resource with the specified IPv6
   * address.
   *
   * @param[in] instance A <code>PP_Instance</code> identifying one instance of
   * a module.
   * @param[in] ipv6_addr An IPv6 address.
   *
   * @return A <code>PP_Resource</code> representing the same address as
   * <code>ipv6_addr</code> or 0 on failure.
   */
  PP_Resource (*CreateFromIPv6Address)(
      PP_Instance instance,
      const struct PP_NetAddress_IPv6* ipv6_addr);
  /**
   * Determines if a given resource is a network address.
   *
   * @param[in] resource A <code>PP_Resource</code> to check.
   *
   * @return <code>PP_TRUE</code> if the input is a <code>PPB_NetAddress</code>
   * resource; <code>PP_FALSE</code> otherwise.
   */
  PP_Bool (*IsNetAddress)(PP_Resource resource);
  /**
   * Gets the address family.
   *
   * @param[in] addr A <code>PP_Resource</code> corresponding to a network
   * address.
   *
   * @return The address family on success;
   * <code>PP_NETADDRESS_FAMILY_UNSPECIFIED</code> on failure.
   */
  PP_NetAddress_Family (*GetFamily)(PP_Resource addr);
  /**
   * Returns a human-readable description of the network address. The
   * description is in the form of host [ ":" port ] and conforms to
   * http://tools.ietf.org/html/rfc3986#section-3.2 for IPv4 and IPv6 addresses
   * (e.g., "192.168.0.1", "192.168.0.1:99", or "[::1]:80").
   *
   * @param[in] addr A <code>PP_Resource</code> corresponding to a network
   * address.
   * @param[in] include_port Whether to include the port number in the
   * description.
   *
   * @return A string <code>PP_Var</code> on success; an undefined
   * <code>PP_Var</code> on failure.
   */
  struct PP_Var (*DescribeAsString)(PP_Resource addr, PP_Bool include_port);
  /**
   * Fills a <code>PP_NetAddress_IPv4</code> structure if the network address is
   * of <code>PP_NETADDRESS_FAMILY_IPV4</code> address family.
   * Note that passing a network address of
   * <code>PP_NETADDRESS_FAMILY_IPV6</code> address family will fail even if the
   * address is an IPv4-mapped IPv6 address.
   *
   * @param[in] addr A <code>PP_Resource</code> corresponding to a network
   * address.
   * @param[out] ipv4_addr A <code>PP_NetAddress_IPv4</code> structure to store
   * the result.
   *
   * @return A <code>PP_Bool</code> value indicating whether the operation
   * succeeded.
   */
  PP_Bool (*DescribeAsIPv4Address)(PP_Resource addr,
                                   struct PP_NetAddress_IPv4* ipv4_addr);
  /**
   * Fills a <code>PP_NetAddress_IPv6</code> structure if the network address is
   * of <code>PP_NETADDRESS_FAMILY_IPV6</code> address family.
   * Note that passing a network address of
   * <code>PP_NETADDRESS_FAMILY_IPV4</code> address family will fail - this
   * method doesn't map it to an IPv6 address.
   *
   * @param[in] addr A <code>PP_Resource</code> corresponding to a network
   * address.
   * @param[out] ipv6_addr A <code>PP_NetAddress_IPv6</code> structure to store
   * the result.
   *
   * @return A <code>PP_Bool</code> value indicating whether the operation
   * succeeded.
   */
  PP_Bool (*DescribeAsIPv6Address)(PP_Resource addr,
                                   struct PP_NetAddress_IPv6* ipv6_addr);
};

typedef struct PPB_NetAddress_1_0 PPB_NetAddress;
/**
 * @}
 */

#endif  /* PPAPI_C_PPB_NET_ADDRESS_H_ */

