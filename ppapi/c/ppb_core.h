/* Copyright 2012 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From ppb_core.idl modified Mon Mar 19 12:02:10 2012. */

#ifndef PPAPI_C_PPB_CORE_H_
#define PPAPI_C_PPB_CORE_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_time.h"

#define PPB_CORE_INTERFACE_1_0 "PPB_Core;1.0"
#define PPB_CORE_INTERFACE PPB_CORE_INTERFACE_1_0

/**
 * @file
 * This file defines the <code>PPB_Core</code> interface defined by the browser
 * and containing pointers to functions related to memory management, time, and
 * threads.
 */


/**
 * @addtogroup Interfaces
 * @{
 */
/**
 * The <code>PPB_Core</code> interface contains pointers to functions related
 * to memory management, time, and threads on the browser.
 *
 */
struct PPB_Core_1_0 {
  /**
   *
   * AddRefResource() adds a reference to a resource.
   *
   * @param[in] config A <code>PP_Resource</code> corresponding to a
   * resource.
   */
  void (*AddRefResource)(PP_Resource resource);
  /**
   * ReleaseResource() removes a reference from a resource.
   *
   * @param[in] config A <code>PP_Resource</code> corresponding to a
   * resource.
   */
  void (*ReleaseResource)(PP_Resource resource);
  /**
   * GetTime() returns the "wall clock time" according to the
   * browser.
   *
   * @return A <code>PP_Time</code> containing the "wall clock time" according
   * to the browser.
   */
  PP_Time (*GetTime)(void);
  /**
   * GetTimeTicks() returns the "tick time" according to the browser.
   * This clock is used by the browser when passing some event times to the
   * module (e.g. using the <code>PP_InputEvent::time_stamp_seconds</code>
   * field). It is not correlated to any actual wall clock time
   * (like GetTime()). Because of this, it will not run change if the user
   * changes their computer clock.
   *
   * @return A <code>PP_TimeTicks</code> containing the "tick time" according
   * to the browser.
   */
  PP_TimeTicks (*GetTimeTicks)(void);
  /**
   * CallOnMainThread() schedules work to be executed on the main module thread
   * after the specified delay. The delay may be 0 to specify a call back as
   * soon as possible.
   *
   * The <code>result</code> parameter will just be passed as the second
   * argument to the callback. Many applications won't need this, but it allows
   * a module to emulate calls of some callbacks which do use this value.
   *
   * <strong>Note:</strong> CallOnMainThread, even when used from the main
   * thread with a delay of 0 milliseconds, will never directly invoke the
   * callback.  Even in this case, the callback will be scheduled
   * asynchronously.
   *
   * <strong>Note:</strong> If the browser is shutting down or if the module
   * has no instances, then the callback function may not be called.
   *
   * @param[in] delay_in_milliseconds An int32_t delay in milliseconds.
   * @param[in] callback A <code>PP_CompletionCallback</code> callback function
   * that the browser will call after the specified delay.
   * @param[in] result An int32_t that the browser will pass to the given
   * <code>PP_CompletionCallback</code>.
   */
  void (*CallOnMainThread)(int32_t delay_in_milliseconds,
                           struct PP_CompletionCallback callback,
                           int32_t result);
  /**
   * IsMainThread() returns true if the current thread is the main pepper
   * thread.
   *
   * This function is useful for implementing sanity checks, and deciding if
   * dispatching using CallOnMainThread() is required.
   *
   * @return A <code>PP_Bool</code> containing <code>PP_TRUE</code> if the
   * current thread is the main pepper thread, otherwise <code>PP_FALSE</code>.
   */
  PP_Bool (*IsMainThread)(void);
};

typedef struct PPB_Core_1_0 PPB_Core;
/**
 * @}
 */

#endif  /* PPAPI_C_PPB_CORE_H_ */

