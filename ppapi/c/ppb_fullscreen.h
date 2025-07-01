/* Copyright 2012 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From ppb_fullscreen.idl modified Wed Dec 21 19:08:34 2011. */

#ifndef PPAPI_C_PPB_FULLSCREEN_H_
#define PPAPI_C_PPB_FULLSCREEN_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_size.h"
#include "ppapi/c/pp_stdint.h"

#define PPB_FULLSCREEN_INTERFACE_1_0 "PPB_Fullscreen;1.0"
#define PPB_FULLSCREEN_INTERFACE PPB_FULLSCREEN_INTERFACE_1_0

/**
 * @file
 * This file defines the <code>PPB_Fullscreen</code> interface for
 * handling transitions of a module instance to and from fullscreen mode.
 */


/**
 * @addtogroup Interfaces
 * @{
 */
/**
 * The <code>PPB_Fullscreen</code> interface is implemented by the browser.
 * This interface provides a way of checking the current screen mode and
 * toggling fullscreen mode.
 */
struct PPB_Fullscreen_1_0 {
  /**
   * IsFullscreen() checks whether the module instance is currently in
   * fullscreen mode.
   *
   * @param[in] instance A <code>PP_Instance</code> identifying one instance
   * of a module.
   *
   * @return <code>PP_TRUE</code> if the module instance is in fullscreen mode,
   * <code>PP_FALSE</code> if the module instance is not in fullscreen mode.
   */
  PP_Bool (*IsFullscreen)(PP_Instance instance);
  /**
   * SetFullscreen() switches the module instance to and from fullscreen
   * mode.
   *
   * The transition to and from fullscreen mode is asynchronous. During the
   * transition, IsFullscreen() will return the previous value and
   * no 2D or 3D device can be bound. The transition ends at DidChangeView()
   * when IsFullscreen() returns the new value. You might receive other
   * DidChangeView() calls while in transition.
   *
   * The transition to fullscreen mode can only occur while the browser is
   * processing a user gesture, even if <code>PP_TRUE</code> is returned.
   *
   * @param[in] instance A <code>PP_Instance</code> identifying one instance
   * of a module.
   * @param[in] fullscreen <code>PP_TRUE</code> to enter fullscreen mode, or
   * <code>PP_FALSE</code> to exit fullscreen mode.
   *
   * @return <code>PP_TRUE</code> on success or <code>PP_FALSE</code> on
   * failure.
   */
  PP_Bool (*SetFullscreen)(PP_Instance instance, PP_Bool fullscreen);
  /**
   * GetScreenSize() gets the size of the screen in pixels. The module instance
   * will be resized to this size when SetFullscreen() is called to enter
   * fullscreen mode.
   *
   * @param[in] instance A <code>PP_Instance</code> identifying one instance
   * of a module.
   * @param[out] size The size of the entire screen in pixels.
   *
   * @return <code>PP_TRUE</code> on success or <code>PP_FALSE</code> on
   * failure.
   */
  PP_Bool (*GetScreenSize)(PP_Instance instance, struct PP_Size* size);
};

typedef struct PPB_Fullscreen_1_0 PPB_Fullscreen;
/**
 * @}
 */

#endif  /* PPAPI_C_PPB_FULLSCREEN_H_ */

