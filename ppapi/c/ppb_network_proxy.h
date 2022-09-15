/* Copyright 2013 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From ppb_network_proxy.idl modified Fri Jun 21 09:37:20 2013. */

#ifndef PPAPI_C_PPB_NETWORK_PROXY_H_
#define PPAPI_C_PPB_NETWORK_PROXY_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_var.h"

#define PPB_NETWORKPROXY_INTERFACE_1_0 "PPB_NetworkProxy;1.0"
#define PPB_NETWORKPROXY_INTERFACE PPB_NETWORKPROXY_INTERFACE_1_0

/**
 * @file
 * This file defines the <code>PPB_NetworkProxy</code> interface.
 */


/**
 * @addtogroup Interfaces
 * @{
 */
/**
 * This interface provides a way to determine the appropriate proxy settings
 * for a given URL.
 *
 * Permissions: Apps permission <code>socket</code> with subrule
 * <code>resolve-proxy</code> is required for using this API.
 * For more details about network communication permissions, please see:
 * http://developer.chrome.com/apps/app_network.html
 */
struct PPB_NetworkProxy_1_0 {
  /**
   * Retrieves the proxy that will be used for the given URL. The result will
   * be a string in PAC format. For more details about PAC format, please see
   * http://en.wikipedia.org/wiki/Proxy_auto-config
   *
   * @param[in] instance A <code>PP_Instance</code> identifying one instance
   * of a module.
   *
   * @param[in] url A string <code>PP_Var</code> containing a URL.
   *
   * @param[out] proxy_string A <code>PP_Var</code> that GetProxyForURL will
   * set upon successful completion. If the call fails, <code>proxy_string
   * </code> will be unchanged. Otherwise, it will be set to a string <code>
   * PP_Var</code> containing the appropriate PAC string for <code>url</code>.
   * If set, <code>proxy_string</code> will have a reference count of 1 which
   * the plugin must manage.
   *
   * @param[in] callback A <code>PP_CompletionCallback</code> to be called upon
   * completion.
   *
   * @return An int32_t containing an error code from <code>pp_errors.h</code>.
   */
  int32_t (*GetProxyForURL)(PP_Instance instance,
                            struct PP_Var url,
                            struct PP_Var* proxy_string,
                            struct PP_CompletionCallback callback);
};

typedef struct PPB_NetworkProxy_1_0 PPB_NetworkProxy;
/**
 * @}
 */

#endif  /* PPAPI_C_PPB_NETWORK_PROXY_H_ */

