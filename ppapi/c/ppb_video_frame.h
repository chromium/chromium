/* Copyright 2014 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From ppb_video_frame.idl modified Tue Mar 25 18:28:57 2014. */

#ifndef PPAPI_C_PPB_VIDEO_FRAME_H_
#define PPAPI_C_PPB_VIDEO_FRAME_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_size.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_time.h"

#define PPB_VIDEOFRAME_INTERFACE_0_1 "PPB_VideoFrame;0.1"
#define PPB_VIDEOFRAME_INTERFACE PPB_VIDEOFRAME_INTERFACE_0_1

/**
 * @file
 * Defines the <code>PPB_VideoFrame</code> interface.
 */


/**
 * @addtogroup Enums
 * @{
 */
typedef enum {
  /**
   * Unknown format value.
   */
  PP_VIDEOFRAME_FORMAT_UNKNOWN = 0,
  /**
   * 12bpp YVU planar 1x1 Y, 2x2 VU samples.
   */
  PP_VIDEOFRAME_FORMAT_YV12 = 1,
  /**
   * 12bpp YUV planar 1x1 Y, 2x2 UV samples.
   */
  PP_VIDEOFRAME_FORMAT_I420 = 2,
  /**
   * 32bpp BGRA.
   */
  PP_VIDEOFRAME_FORMAT_BGRA = 3,
  /**
   * The last format.
   */
  PP_VIDEOFRAME_FORMAT_LAST = PP_VIDEOFRAME_FORMAT_BGRA
} PP_VideoFrame_Format;
/**
 * @}
 */

/**
 * @addtogroup Interfaces
 * @{
 */
struct PPB_VideoFrame_0_1 {
  /**
   * Determines if a resource is a VideoFrame resource.
   *
   * @param[in] resource The <code>PP_Resource</code> to test.
   *
   * @return A <code>PP_Bool</code> with <code>PP_TRUE</code> if the given
   * resource is a VideoFrame resource or <code>PP_FALSE</code> otherwise.
   */
  PP_Bool (*IsVideoFrame)(PP_Resource resource);
  /**
   * Gets the timestamp of the video frame.
   *
   * @param[in] frame A <code>PP_Resource</code> corresponding to a video frame
   * resource.
   *
   * @return A <code>PP_TimeDelta</code> containing the timestamp of the video
   * frame. Given in seconds since the start of the containing video stream.
   */
  PP_TimeDelta (*GetTimestamp)(PP_Resource frame);
  /**
   * Sets the timestamp of the video frame. Given in seconds since the
   * start of the containing video stream.
   *
   * @param[in] frame A <code>PP_Resource</code> corresponding to a video frame
   * resource.
   * @param[in] timestamp A <code>PP_TimeDelta</code> containing the timestamp
   * of the video frame. Given in seconds since the start of the containing
   * video stream.
   */
  void (*SetTimestamp)(PP_Resource frame, PP_TimeDelta timestamp);
  /**
   * Gets the format of the video frame.
   *
   * @param[in] frame A <code>PP_Resource</code> corresponding to a video frame
   * resource.
   *
   * @return A <code>PP_VideoFrame_Format</code> containing the format of the
   * video frame.
   */
  PP_VideoFrame_Format (*GetFormat)(PP_Resource frame);
  /**
   * Gets the size of the video frame.
   *
   * @param[in] frame A <code>PP_Resource</code> corresponding to a video frame
   * resource.
   * @param[out] size A <code>PP_Size</code>.
   *
   * @return A <code>PP_Bool</code> with <code>PP_TRUE</code> on success or
   * <code>PP_FALSE</code> on failure.
   */
  PP_Bool (*GetSize)(PP_Resource frame, struct PP_Size* size);
  /**
   * Gets the data buffer for video frame pixels.
   *
   * @param[in] frame A <code>PP_Resource</code> corresponding to a video frame
   * resource.
   *
   * @return A pointer to the beginning of the data buffer.
   */
  void* (*GetDataBuffer)(PP_Resource frame);
  /**
   * Gets the size of data buffer.
   *
   * @param[in] frame A <code>PP_Resource</code> corresponding to a video frame
   * resource.
   *
   * @return The size of the data buffer.
   */
  uint32_t (*GetDataBufferSize)(PP_Resource frame);
};

typedef struct PPB_VideoFrame_0_1 PPB_VideoFrame;
/**
 * @}
 */

#endif  /* PPAPI_C_PPB_VIDEO_FRAME_H_ */

