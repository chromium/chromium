/* Copyright 2015 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From private/pp_video_capture_format.idl,
 *   modified Wed Feb 18 01:41:26 2015.
 */

#ifndef PPAPI_C_PRIVATE_PP_VIDEO_CAPTURE_FORMAT_H_
#define PPAPI_C_PRIVATE_PP_VIDEO_CAPTURE_FORMAT_H_

#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_size.h"
#include "ppapi/c/pp_stdint.h"

/**
 * @file
 * This file defines the struct used to hold a video capture format.
 */


/**
 * @addtogroup Structs
 * @{
 */
/**
 * The <code>PP_VideoCaptureFormat</code> struct represents a video capture
 * format.
 */
struct PP_VideoCaptureFormat {
  /**
   * Frame size in pixels.
   */
  struct PP_Size frame_size;
  /**
   * Frame rate in frames per second.
   */
  float frame_rate;
};
PP_COMPILE_ASSERT_STRUCT_SIZE_IN_BYTES(PP_VideoCaptureFormat, 12);
/**
 * @}
 */

#endif  /* PPAPI_C_PRIVATE_PP_VIDEO_CAPTURE_FORMAT_H_ */

