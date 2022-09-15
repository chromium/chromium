/* Copyright 2012 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From ppb.idl modified Fri Jan 24 16:19:56 2014. */

#ifndef PPAPI_C_PPB_H_
#define PPAPI_C_PPB_H_

#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_stdint.h"

/**
 * @file
 * This file defines a function pointer type for the
 * <code>PPB_GetInterface</code> function.
 */


/**
 * @addtogroup Typedefs
 * @{
 */
/**
 * This function pointer type defines the signature for the
 * <code>PPB_GetInterface</code> function. A generic
 * <code>PPB_GetInterface</code> pointer is passed to
 * <code>PPP_InitializedModule</code> when your module is loaded. You can use
 * this pointer to request a pointer to a specific browser interface. Browser
 * interface names are ASCII strings and are generally defined in the header
 * file for the interface, such as <code>PPB_AUDIO_INTERFACE</code> found in
 * <code>ppb.audio.h</code> or
 * <code>PPB_GRAPHICS_2D_INTERFACE</code> in <code>ppb_graphics_2d.h</code>.
 * Click
 * <a href="globals_defs.html"
 * title="macros">here</a> for a complete list of interface
 * names.
 *
 * This value will be NULL if the interface is not supported on the browser.
 */
typedef const void* (*PPB_GetInterface)(const char* interface_name);
/**
 * @}
 */

#endif  /* PPAPI_C_PPB_H_ */

