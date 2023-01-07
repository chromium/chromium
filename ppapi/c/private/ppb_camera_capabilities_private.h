/* Copyright 2014 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From private/ppb_camera_capabilities_private.idl,
 *   modified Thu Feb 19 09:06:18 2015.
 */

#ifndef PPAPI_C_PRIVATE_PPB_CAMERA_CAPABILITIES_PRIVATE_H_
#define PPAPI_C_PRIVATE_PPB_CAMERA_CAPABILITIES_PRIVATE_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_size.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/private/pp_video_capture_format.h"

#define PPB_CAMERACAPABILITIES_PRIVATE_INTERFACE_0_1 \
    "PPB_CameraCapabilities_Private;0.1"
#define PPB_CAMERACAPABILITIES_PRIVATE_INTERFACE \
    PPB_CAMERACAPABILITIES_PRIVATE_INTERFACE_0_1

/**
 * @file
 * This file defines the PPB_CameraCapabilities_Private interface for
 * establishing an image capture configuration resource within the browser.
 */


/**
 * @addtogroup Interfaces
 * @{
 */
/**
 * The <code>PPB_CameraCapabilities_Private</code> interface contains pointers
 * to several functions for getting the image capture capabilities within the
 * browser.
 */
struct PPB_CameraCapabilities_Private_0_1 {
  /**
   * IsCameraCapabilities() determines if the given resource is a
   * <code>PPB_CameraCapabilities_Private</code>.
   *
   * @param[in] resource A <code>PP_Resource</code> corresponding to an image
   * capture capabilities resource.
   *
   * @return A <code>PP_Bool</code> containing <code>PP_TRUE</code> if the given
   * resource is an <code>PP_CameraCapabilities_Private</code> resource,
   * otherwise <code>PP_FALSE</code>.
   */
  PP_Bool (*IsCameraCapabilities)(PP_Resource resource);
  /**
   * GetSupportedVideoCaptureFormats() returns the supported video capture
   * formats for the given <code>PPB_CameraCapabilities_Private</code>.
   *
   * @param[in] capabilities A <code>PP_Resource</code> corresponding to an
   * image capture capabilities resource.
   * @param[out] array_size The size of preview size array.
   * @param[out] formats An array of <code>PP_VideoCaptureFormat</code>
   * corresponding to the supported video capture formats. The ownership of the
   * array belongs to <code>PPB_CameraCapabilities_Private</code> and the caller
   * should not free it. When a PPB_CameraCapabilities_Private is deleted, the
   * array returning from this is no longer valid.
   */
  void (*GetSupportedVideoCaptureFormats)(
      PP_Resource capabilities,
      uint32_t* array_size,
      struct PP_VideoCaptureFormat** formats);
};

typedef struct PPB_CameraCapabilities_Private_0_1
    PPB_CameraCapabilities_Private;
/**
 * @}
 */

#endif  /* PPAPI_C_PRIVATE_PPB_CAMERA_CAPABILITIES_PRIVATE_H_ */

