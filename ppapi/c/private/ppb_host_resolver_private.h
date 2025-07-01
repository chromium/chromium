/* Copyright 2012 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From private/ppb_host_resolver_private.idl,
 *   modified Mon Jun 24 09:49:40 2013.
 */

#ifndef PPAPI_C_PRIVATE_PPB_HOST_RESOLVER_PRIVATE_H_
#define PPAPI_C_PRIVATE_PPB_HOST_RESOLVER_PRIVATE_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/c/private/ppb_net_address_private.h"

#define PPB_HOSTRESOLVER_PRIVATE_INTERFACE_0_1 "PPB_HostResolver_Private;0.1"
#define PPB_HOSTRESOLVER_PRIVATE_INTERFACE \
    PPB_HOSTRESOLVER_PRIVATE_INTERFACE_0_1

/**
 * @file
 * This file defines the <code>PPB_HostResolver_Private</code> interface.
 */


/**
 * @addtogroup Enums
 * @{
 */
/**
 * The <code>PP_HostResolver_Flags</code> is an enumeration of the
 * different types of flags, that can be OR-ed and passed to host
 * resolver.
 */
typedef enum {
  /**
   * AI_CANONNAME
   */
  PP_HOST_RESOLVER_PRIVATE_FLAGS_CANONNAME = 1 << 0,
  /**
   * Hint to the resolver that only loopback addresses are configured.
   */
  PP_HOST_RESOLVER_PRIVATE_FLAGS_LOOPBACK_ONLY = 1 << 1
} PP_HostResolver_Private_Flags;
PP_COMPILE_ASSERT_SIZE_IN_BYTES(PP_HostResolver_Private_Flags, 4);
/**
 * @}
 */

/**
 * @addtogroup Structs
 * @{
 */
struct PP_HostResolver_Private_Hint {
  PP_NetAddressFamily_Private family;
  int32_t flags;
};
PP_COMPILE_ASSERT_STRUCT_SIZE_IN_BYTES(PP_HostResolver_Private_Hint, 8);
/**
 * @}
 */

/**
 * @addtogroup Interfaces
 * @{
 */
struct PPB_HostResolver_Private_0_1 {
  /**
   * Allocates a Host Resolver resource.
   */
  PP_Resource (*Create)(PP_Instance instance);
  /**
   * Determines if a given resource is a Host Resolver.
   */
  PP_Bool (*IsHostResolver)(PP_Resource resource);
  /**
   * Creates a new request to Host Resolver. |callback| is invoked
   * when request is processed and a list of network addresses is
   * obtained. These addresses can be be used in Connect, Bind or
   * Listen calls to connect to a given |host| and |port|.
   */
  int32_t (*Resolve)(PP_Resource host_resolver,
                     const char* host,
                     uint16_t port,
                     const struct PP_HostResolver_Private_Hint* hint,
                     struct PP_CompletionCallback callback);
  /**
   * Returns canonical name of host.
   */
  struct PP_Var (*GetCanonicalName)(PP_Resource host_resolver);
  /**
   * Returns number of network addresses obtained after Resolve call.
   */
  uint32_t (*GetSize)(PP_Resource host_resolver);
  /**
   * Stores in the |addr| |index|-th network address. |addr| can't be
   * NULL. Returns PP_TRUE if success or PP_FALSE if the given
   * resource is not a Host Resolver or |index| exceeds number of
   * available addresses.
   */
  PP_Bool (*GetNetAddress)(PP_Resource host_resolver,
                           uint32_t index,
                           struct PP_NetAddress_Private* addr);
};

typedef struct PPB_HostResolver_Private_0_1 PPB_HostResolver_Private;
/**
 * @}
 */

#endif  /* PPAPI_C_PRIVATE_PPB_HOST_RESOLVER_PRIVATE_H_ */

