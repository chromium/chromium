/* Copyright 2012 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From dev/ppb_video_capture_dev.idl modified Thu Dec 12 15:36:11 2013. */

#ifndef PPAPI_C_DEV_PPB_VIDEO_CAPTURE_DEV_H_
#define PPAPI_C_DEV_PPB_VIDEO_CAPTURE_DEV_H_

#include "ppapi/c/dev/pp_video_capture_dev.h"
#include "ppapi/c/dev/ppb_device_ref_dev.h"
#include "ppapi/c/pp_array_output.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"

#define PPB_VIDEOCAPTURE_DEV_INTERFACE_0_3 "PPB_VideoCapture(Dev);0.3"
#define PPB_VIDEOCAPTURE_DEV_INTERFACE PPB_VIDEOCAPTURE_DEV_INTERFACE_0_3

/**
 * @file
 * This file defines the <code>PPB_VideoCapture_Dev</code> interface.
 */


/**
 * @addtogroup Interfaces
 * @{
 */
/**
 * Video capture interface. It goes hand-in-hand with PPP_VideoCapture_Dev.
 *
 * Theory of operation:
 * 1- Create a VideoCapture resource using Create.
 * 2- Find available video capture devices using EnumerateDevices.
 * 3- Open a video capture device. In addition to a device reference (0 can be
 * used to indicate the default device), you pass in the requested info
 * (resolution, frame rate), as well as suggest a number of buffers you will
 * need.
 * 4- Start the capture using StartCapture.
 * 5- Receive the OnDeviceInfo callback, in PPP_VideoCapture_Dev, which will
 * give you the actual capture info (the requested one is not guaranteed), as
 * well as an array of buffers allocated by the browser.
 * 6- On every frame captured by the browser, OnBufferReady (in
 * PPP_VideoCapture_Dev) is called with the index of the buffer from the array
 * containing the new frame. The buffer is now "owned" by the plugin, and the
 * browser won't reuse it until ReuseBuffer is called.
 * 7- When the plugin is done with the buffer, call ReuseBuffer.
 * 8- Stop the capture using StopCapture.
 * 9- Close the device.
 *
 * The browser may change the resolution based on the constraints of the system,
 * in which case OnDeviceInfo will be called again, with new buffers.
 *
 * The buffers contain the pixel data for a frame. The format is planar YUV
 * 4:2:0, one byte per pixel, tightly packed (width x height Y values, then
 * width/2 x height/2 U values, then width/2 x height/2 V values).
 */
struct PPB_VideoCapture_Dev_0_3 {
  /**
   * Creates a new VideoCapture.
   */
  PP_Resource (*Create)(PP_Instance instance);
  /**
   * Returns PP_TRUE if the given resource is a VideoCapture.
   */
  PP_Bool (*IsVideoCapture)(PP_Resource video_capture);
  /**
   * Enumerates video capture devices.
   *
   * @param[in] video_capture A <code>PP_Resource</code> corresponding to a
   * video capture resource.
   * @param[in] output An output array which will receive
   * <code>PPB_DeviceRef_Dev</code> resources on success. Please note that the
   * ref count of those resources has already been increased by 1 for the
   * caller.
   * @param[in] callback A <code>PP_CompletionCallback</code> to run on
   * completion.
   *
   * @return An error code from <code>pp_errors.h</code>.
   */
  int32_t (*EnumerateDevices)(PP_Resource video_capture,
                              struct PP_ArrayOutput output,
                              struct PP_CompletionCallback callback);
  /**
   * Requests device change notifications.
   *
   * @param[in] video_capture A <code>PP_Resource</code> corresponding to a
   * video capture resource.
   * @param[in] callback The callback to receive notifications. If not NULL, it
   * will be called once for the currently available devices, and then every
   * time the list of available devices changes. All calls will happen on the
   * same thread as the one on which MonitorDeviceChange() is called. It will
   * receive notifications until <code>video_capture</code> is destroyed or
   * <code>MonitorDeviceChange()</code> is called to set a new callback for
   * <code>video_capture</code>. You can pass NULL to cancel sending
   * notifications.
   * @param[inout] user_data An opaque pointer that will be passed to
   * <code>callback</code>.
   *
   * @return An error code from <code>pp_errors.h</code>.
   */
  int32_t (*MonitorDeviceChange)(PP_Resource video_capture,
                                 PP_MonitorDeviceChangeCallback callback,
                                 void* user_data);
  /**
   * Opens a video capture device. |device_ref| identifies a video capture
   * device. It could be one of the resource in the array returned by
   * |EnumerateDevices()|, or 0 which means the default device.
   * |requested_info| is a pointer to a structure containing the requested
   * resolution and frame rate. |buffer_count| is the number of buffers
   * requested by the plugin. Note: it is only used as advisory, the browser may
   * allocate more or fewer based on available resources. How many buffers
   * depends on usage. At least 2 to make sure latency doesn't cause lost
   * frames. If the plugin expects to hold on to more than one buffer at a time
   * (e.g. to do multi-frame processing, like video encoding), it should request
   * that many more.
   */
  int32_t (*Open)(PP_Resource video_capture,
                  PP_Resource device_ref,
                  const struct PP_VideoCaptureDeviceInfo_Dev* requested_info,
                  uint32_t buffer_count,
                  struct PP_CompletionCallback callback);
  /**
   * Starts the capture.
   *
   * Returns PP_ERROR_FAILED if called when the capture was already started, or
   * PP_OK on success.
   */
  int32_t (*StartCapture)(PP_Resource video_capture);
  /**
   * Allows the browser to reuse a buffer that was previously sent by
   * PPP_VideoCapture_Dev.OnBufferReady. |buffer| is the index of the buffer in
   * the array returned by PPP_VideoCapture_Dev.OnDeviceInfo.
   *
   * Returns PP_ERROR_BADARGUMENT if buffer is out of range (greater than the
   * number of buffers returned by PPP_VideoCapture_Dev.OnDeviceInfo), or if it
   * is not currently owned by the plugin. Returns PP_OK otherwise.
   */
  int32_t (*ReuseBuffer)(PP_Resource video_capture, uint32_t buffer);
  /**
   * Stops the capture.
   *
   * Returns PP_ERROR_FAILED if the capture wasn't already started, or PP_OK on
   * success.
   */
  int32_t (*StopCapture)(PP_Resource video_capture);
  /**
   * Closes the video capture device, and stops capturing if necessary. It is
   * not valid to call |Open()| again after a call to this method.
   * If a video capture resource is destroyed while a device is still open, then
   * it will be implicitly closed, so you are not required to call this method.
   */
  void (*Close)(PP_Resource video_capture);
};

typedef struct PPB_VideoCapture_Dev_0_3 PPB_VideoCapture_Dev;
/**
 * @}
 */

#endif  /* PPAPI_C_DEV_PPB_VIDEO_CAPTURE_DEV_H_ */

