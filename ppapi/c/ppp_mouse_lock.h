/* Copyright 2012 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From ppp_mouse_lock.idl modified Wed Dec 21 19:08:34 2011. */

#ifndef PPAPI_C_PPP_MOUSE_LOCK_H_
#define PPAPI_C_PPP_MOUSE_LOCK_H_

#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_stdint.h"

#define PPP_MOUSELOCK_INTERFACE_1_0 "PPP_MouseLock;1.0"
#define PPP_MOUSELOCK_INTERFACE PPP_MOUSELOCK_INTERFACE_1_0

/**
 * @file
 * This file defines the <code>PPP_MouseLock</code> interface containing a
 * function that you must implement to receive mouse lock events from the
 * browser.
 */


/**
 * @addtogroup Interfaces
 * @{
 */
/**
 * The <code>PPP_MouseLock</code> interface contains a function that you must
 * implement to receive mouse lock events from the browser.
 */
struct PPP_MouseLock_1_0 {
  /**
   * MouseLockLost() is called when the instance loses the mouse lock, such as
   * when the user presses the ESC key.
   *
   * @param[in] instance A <code>PP_Instance</code> identifying one instance
   * of a module.
   */
  void (*MouseLockLost)(PP_Instance instance);
};

typedef struct PPP_MouseLock_1_0 PPP_MouseLock;
/**
 * @}
 */

#endif  /* PPAPI_C_PPP_MOUSE_LOCK_H_ */

