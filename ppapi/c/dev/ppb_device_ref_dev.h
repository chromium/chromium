/* Copyright 2012 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From dev/ppb_device_ref_dev.idl modified Mon Jan 09 12:04:09 2017. */

#ifndef PPAPI_C_DEV_PPB_DEVICE_REF_DEV_H_
#define PPAPI_C_DEV_PPB_DEVICE_REF_DEV_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_var.h"

#define PPB_DEVICEREF_DEV_INTERFACE_0_1 "PPB_DeviceRef(Dev);0.1"
#define PPB_DEVICEREF_DEV_INTERFACE PPB_DEVICEREF_DEV_INTERFACE_0_1

/**
 * @file
 * This file defines the <code>PPB_DeviceRef_Dev</code> interface.
 */


/**
 * @addtogroup Typedefs
 * @{
 */
/**
 * Defines the callback type to receive device change notifications for
 * <code>PPB_AudioInput_Dev.MonitorDeviceChange()</code> and
 * <code>PPB_VideoCapture_Dev.MonitorDeviceChange()</code>.
 *
 * @param[inout] user_data The opaque pointer that the caller passed into
 * <code>MonitorDeviceChange()</code>.
 * @param[in] device_count How many devices in the array.
 * @param[in] devices An array of <code>PPB_DeviceRef_Dev</code>. Please note
 * that the ref count of the elements is not increased on behalf of the plugin.
 */
typedef void (*PP_MonitorDeviceChangeCallback)(void* user_data,
                                               uint32_t device_count,
                                               const PP_Resource devices[]);
/**
 * @}
 */

/**
 * @addtogroup Enums
 * @{
 */
/**
 * Device types.
 */
typedef enum {
  PP_DEVICETYPE_DEV_INVALID = 0,
  PP_DEVICETYPE_DEV_AUDIOCAPTURE = 1,
  PP_DEVICETYPE_DEV_VIDEOCAPTURE = 2,
  PP_DEVICETYPE_DEV_AUDIOOUTPUT = 3,
  PP_DEVICETYPE_DEV_MAX = PP_DEVICETYPE_DEV_AUDIOOUTPUT
} PP_DeviceType_Dev;
PP_COMPILE_ASSERT_SIZE_IN_BYTES(PP_DeviceType_Dev, 4);
/**
 * @}
 */

/**
 * @addtogroup Interfaces
 * @{
 */
struct PPB_DeviceRef_Dev_0_1 {
  /**
   * Determines if the provided resource is a device reference.
   *
   * @param[in] resource A <code>PP_Resource</code> corresponding to a generic
   * resource.
   *
   * @return A <code>PP_Bool</code> that is <code>PP_TRUE</code> if the given
   * resource is a device reference, otherwise <code>PP_FALSE</code>.
   */
  PP_Bool (*IsDeviceRef)(PP_Resource resource);
  /**
   * Gets the device type.
   *
   * @param[in] device_ref A <code>PP_Resource</code> corresponding to a device
   * reference.
   *
   * @return A <code>PP_DeviceType_Dev</code> value.
   */
  PP_DeviceType_Dev (*GetType)(PP_Resource device_ref);
  /**
   * Gets the device name.
   *
   * @param[in] device_ref A <code>PP_Resource</code> corresponding to a device
   * reference.
   *
   * @return A <code>PP_Var</code> of type <code>PP_VARTYPE_STRING</code>
   * containing the name of the device if successful; a <code>PP_Var</code> of
   * type <code>PP_VARTYPE_UNDEFINED</code> if failed.
   */
  struct PP_Var (*GetName)(PP_Resource device_ref);
};

typedef struct PPB_DeviceRef_Dev_0_1 PPB_DeviceRef_Dev;
/**
 * @}
 */

#endif  /* PPAPI_C_DEV_PPB_DEVICE_REF_DEV_H_ */

