/* Copyright 2012 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From dev/ppp_network_state_dev.idl modified Wed Nov  7 09:50:23 2012. */

#ifndef PPAPI_C_DEV_PPP_NETWORK_STATE_DEV_H_
#define PPAPI_C_DEV_PPP_NETWORK_STATE_DEV_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_stdint.h"

#define PPP_NETWORK_STATE_DEV_INTERFACE_0_1 "PPP_NetworkState(Dev);0.1"
#define PPP_NETWORK_STATE_DEV_INTERFACE PPP_NETWORK_STATE_DEV_INTERFACE_0_1

/**
 * @file
 * This file defines the PPP_NetworkState interface.
 */


/**
 * @addtogroup Interfaces
 * @{
 */
struct PPP_NetworkState_Dev_0_1 {
  /**
   * Notification that the online state has changed for the user's network.
   * This will change as a result of a network cable being plugged or
   * unplugged, WiFi connections going up and down, or other events.
   *
   * Note that being "online" isn't a guarantee that any particular connections
   * will succeed.
   */
  void (*SetOnLine)(PP_Bool is_online);
};

typedef struct PPP_NetworkState_Dev_0_1 PPP_NetworkState_Dev;
/**
 * @}
 */

#endif  /* PPAPI_C_DEV_PPP_NETWORK_STATE_DEV_H_ */

