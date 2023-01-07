/* Copyright 2014 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From private/ppb_camera_device_private.idl,
 *   modified Wed Nov  2 15:54:24 2016.
 */

#ifndef PPAPI_C_PRIVATE_PPB_CAMERA_DEVICE_PRIVATE_H_
#define PPAPI_C_PRIVATE_PPB_CAMERA_DEVICE_PRIVATE_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_var.h"

#define PPB_CAMERADEVICE_PRIVATE_INTERFACE_0_1 "PPB_CameraDevice_Private;0.1"
#define PPB_CAMERADEVICE_PRIVATE_INTERFACE \
    PPB_CAMERADEVICE_PRIVATE_INTERFACE_0_1

/**
 * @file
 * Defines the <code>PPB_CameraDevice_Private</code> interface. Used for
 * manipulating a camera device.
 */


/**
 * @addtogroup Interfaces
 * @{
 */
/**
 * To query camera capabilities:
 * 1. Get a PPB_CameraDevice_Private object by Create().
 * 2. Open() camera device with track id of MediaStream video track.
 * 3. Call GetCameraCapabilities() to get a
 *    <code>PPB_CameraCapabilities_Private</code> object, which can be used to
 *    query camera capabilities.
 */
struct PPB_CameraDevice_Private_0_1 {
  /**
   * Creates a PPB_CameraDevice_Private resource.
   *
   * @param[in] instance A <code>PP_Instance</code> identifying one instance
   * of a module.
   *
   * @return A <code>PP_Resource</code> corresponding to a
   * PPB_CameraDevice_Private resource if successful, 0 if failed.
   */
  PP_Resource (*Create)(PP_Instance instance);
  /**
   * Determines if a resource is a camera device resource.
   *
   * @param[in] resource The <code>PP_Resource</code> to test.
   *
   * @return A <code>PP_Bool</code> with <code>PP_TRUE</code> if the given
   * resource is a camera device resource or <code>PP_FALSE</code>
   * otherwise.
   */
  PP_Bool (*IsCameraDevice)(PP_Resource resource);
  /**
   * Opens a camera device.
   *
   * @param[in] camera_device A <code>PP_Resource</code> corresponding to a
   * camera device resource.
   * @param[in] device_id A <code>PP_Var</code> identifying a camera device. The
   * type is string. The ID can be obtained from
   * navigator.mediaDevices.enumerateDevices() or MediaStreamVideoTrack.id.
   * @param[in] callback A <code>PP_CompletionCallback</code> to be called upon
   * completion of <code>Open()</code>.
   *
   * @return An error code from <code>pp_errors.h</code>.
   */
  int32_t (*Open)(PP_Resource camera_device,
                  struct PP_Var device_id,
                  struct PP_CompletionCallback callback);
  /**
   * Disconnects from the camera and cancels all pending requests.
   * After this returns, no callbacks will be called. If <code>
   * PPB_CameraDevice_Private</code> is destroyed and is not closed yet, this
   * function will be automatically called. Calling this more than once has no
   * effect.
   *
   * @param[in] camera_device A <code>PP_Resource</code> corresponding to a
   * camera device resource.
   */
  void (*Close)(PP_Resource camera_device);
  /**
   * Gets the camera capabilities.
   *
   * The camera capabilities do not change for a given camera source.
   *
   * @param[in] camera_device A <code>PP_Resource</code> corresponding to a
   * camera device resource.
   * @param[out] capabilities A <code>PPB_CameraCapabilities_Private</code> for
   * storing the camera capabilities on success. Otherwise, the value will not
   * be changed.
   * @param[in] callback <code>PP_CompletionCallback</code> to be called upon
   * completion of <code>GetCameraCapabilities()</code>.
   *
   * @return An int32_t containing a result code from <code>pp_errors.h</code>.
   */
  int32_t (*GetCameraCapabilities)(PP_Resource camera_device,
                                   PP_Resource* capabilities,
                                   struct PP_CompletionCallback callback);
};

typedef struct PPB_CameraDevice_Private_0_1 PPB_CameraDevice_Private;
/**
 * @}
 */

#endif  /* PPAPI_C_PRIVATE_PPB_CAMERA_DEVICE_PRIVATE_H_ */

