/* Copyright 2012 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From dev/ppp_video_capture_dev.idl modified Mon Oct 01 14:26:07 2012. */

#ifndef PPAPI_C_DEV_PPP_VIDEO_CAPTURE_DEV_H_
#define PPAPI_C_DEV_PPP_VIDEO_CAPTURE_DEV_H_

#include "ppapi/c/dev/pp_video_capture_dev.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"

#define PPP_VIDEO_CAPTURE_DEV_INTERFACE_0_1 "PPP_VideoCapture(Dev);0.1"
#define PPP_VIDEO_CAPTURE_DEV_INTERFACE PPP_VIDEO_CAPTURE_DEV_INTERFACE_0_1

/**
 * @file
 * This file defines the <code>PPP_VideoCapture_Dev</code> interface.
 */


/**
 * @addtogroup Interfaces
 * @{
 */
/**
 * Video Capture client interface. See |PPB_VideoCapture_Dev| for general theory
 * of operation.
 */
struct PPP_VideoCapture_Dev_0_1 {
  /**
   * Signals the capture device information, such as resolution and frame rate,
   * and the array of buffers that the browser will use to send pixel data.
   *
   * |info| is a pointer to the PP_VideoCaptureDeviceInfo_Dev structure
   * containing resolution and frame rate.
   * |buffer_count| is the number of buffers, and |buffers| is the array of
   * PPB_Buffer_Dev buffers.
   *
   * Note: the buffers are passed without an extra reference. The plugin is
   * expected to add its own references to the buffers.
   */
  void (*OnDeviceInfo)(PP_Instance instance,
                       PP_Resource video_capture,
                       const struct PP_VideoCaptureDeviceInfo_Dev* info,
                       uint32_t buffer_count,
                       const PP_Resource buffers[]);
  /**
   * Signals status changes on the VideoCapture. |status| is a
   * one of the values from PP_VideoCaptureStatus_Dev;
   */
  void (*OnStatus)(PP_Instance instance,
                   PP_Resource video_capture,
                   uint32_t status);
  /**
   * Signals an error from the video capture system.
   *
   * Errors that can be generated:
   * - PP_ERROR_NOMEMORY: not enough memory was available to allocate buffers.
   * - PP_ERROR_FAILED: video capture could not start.
   */
  void (*OnError)(PP_Instance instance,
                  PP_Resource video_capture,
                  uint32_t error_code);
  /**
   * Signals that a buffer is available for consumption by the plugin.
   *
   * |buffer| is the index of the buffer, in the array returned by OnDeviceInfo.
   */
  void (*OnBufferReady)(PP_Instance instance,
                        PP_Resource video_capture,
                        uint32_t buffer);
};

typedef struct PPP_VideoCapture_Dev_0_1 PPP_VideoCapture_Dev;
/**
 * @}
 */

#endif  /* PPAPI_C_DEV_PPP_VIDEO_CAPTURE_DEV_H_ */

