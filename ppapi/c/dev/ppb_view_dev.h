/* Copyright 2012 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From dev/ppb_view_dev.idl modified Mon Jun 18 14:55:58 2012. */

#ifndef PPAPI_C_DEV_PPB_VIEW_DEV_H_
#define PPAPI_C_DEV_PPB_VIEW_DEV_H_

#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"

#define PPB_VIEW_DEV_INTERFACE_0_1 "PPB_View(Dev);0.1"
#define PPB_VIEW_DEV_INTERFACE PPB_VIEW_DEV_INTERFACE_0_1

/**
 * @file
 * This file contains the <code>PPB_View_Dev</code> interface. */


/**
 * @addtogroup Interfaces
 * @{
 */
/* PPB_View_Dev interface */
struct PPB_View_Dev_0_1 {
  /**
   * GetDeviceScale returns the scale factor between device pixels and DIPs
   * (also known as logical pixels or UI pixels on some platforms). This allows
   * the developer to render their contents at device resolution, even as
   * coordinates / sizes are given in DIPs through the API.
   *
   * Note that the coordinate system for Pepper APIs is DIPs. Also note that
   * one DIP might not equal one CSS pixel - when page scale/zoom is in effect.
   *
   * @param[in] resource A <code>PP_Resource</code> corresponding to a
   * <code>PPB_View</code> resource.
   *
   * @return A <code>float</code> value representing the number of device pixels
   * per DIP. If the resource is invalid, the value will be 0.0.
   */
  float (*GetDeviceScale)(PP_Resource resource);
  /**
   * GetCSSScale returns the scale factor between DIPs and CSS pixels. This
   * allows proper scaling between DIPs - as sent via the Pepper API - and CSS
   * pixel coordinates used for Web content.
   *
   * @param[in] resource A <code>PP_Resource</code> corresponding to a
   * <code>PPB_View</code> resource.
   *
   * @return css_scale A <code>float</code> value representing the number of
   * DIPs per CSS pixel. If the resource is invalid, the value will be 0.0.
   */
  float (*GetCSSScale)(PP_Resource resource);
};

typedef struct PPB_View_Dev_0_1 PPB_View_Dev;
/**
 * @}
 */

#endif  /* PPAPI_C_DEV_PPB_VIEW_DEV_H_ */

