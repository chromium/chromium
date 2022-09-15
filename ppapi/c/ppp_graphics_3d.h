/* Copyright 2012 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From ppp_graphics_3d.idl modified Wed Mar 21 17:35:39 2012. */

#ifndef PPAPI_C_PPP_GRAPHICS_3D_H_
#define PPAPI_C_PPP_GRAPHICS_3D_H_

#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_stdint.h"

#define PPP_GRAPHICS_3D_INTERFACE_1_0 "PPP_Graphics_3D;1.0"
#define PPP_GRAPHICS_3D_INTERFACE PPP_GRAPHICS_3D_INTERFACE_1_0

/**
 * @file
 * Defines the <code>PPP_Graphics3D</code> struct representing a 3D graphics
 * context within the browser.
 */


/**
 * @addtogroup Interfaces
 * @{
 */
/**
 * <code>PPP_Graphics3D</code> defines the notification interface for a 3D
 * graphics context.
 */
struct PPP_Graphics3D_1_0 {
  /**
   * Called when the OpenGL ES window is invalidated and needs to be repainted.
   */
  void (*Graphics3DContextLost)(PP_Instance instance);
};

typedef struct PPP_Graphics3D_1_0 PPP_Graphics3D;
/**
 * @}
 */

#endif  /* PPAPI_C_PPP_GRAPHICS_3D_H_ */

