/* Copyright 2012 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From dev/ppp_video_decoder_dev.idl modified Fri Dec 13 15:21:30 2013. */

#ifndef PPAPI_C_DEV_PPP_VIDEO_DECODER_DEV_H_
#define PPAPI_C_DEV_PPP_VIDEO_DECODER_DEV_H_

#include "ppapi/c/dev/pp_video_dev.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_size.h"
#include "ppapi/c/pp_stdint.h"

#define PPP_VIDEODECODER_DEV_INTERFACE_0_11 "PPP_VideoDecoder(Dev);0.11"
#define PPP_VIDEODECODER_DEV_INTERFACE PPP_VIDEODECODER_DEV_INTERFACE_0_11

/**
 * @file
 * This file defines the <code>PPP_VideoDecoder_Dev</code> interface.
 */


/**
 * @addtogroup Interfaces
 * @{
 */
/**
 * PPP_VideoDecoder_Dev structure contains the function pointers that the
 * plugin MUST implement to provide services needed by the video decoder
 * implementation.
 *
 * See PPB_VideoDecoder_Dev for general usage tips.
 */
struct PPP_VideoDecoder_Dev_0_11 {
  /**
   * Callback function to provide buffers for the decoded output pictures. If
   * succeeds plugin must provide buffers through AssignPictureBuffers function
   * to the API. If |req_num_of_bufs| matching exactly the specification
   * given in the parameters cannot be allocated decoder should be destroyed.
   *
   * Decoding will not proceed until buffers have been provided.
   *
   * Parameters:
   *  |instance| the plugin instance to which the callback is responding.
   *  |decoder| the PPB_VideoDecoder_Dev resource.
   *  |req_num_of_bufs| tells how many buffers are needed by the decoder.
   *  |dimensions| tells the dimensions of the buffer to allocate.
   *  |texture_target| the type of texture used. Sample targets in use are
   *      TEXTURE_2D (most platforms) and TEXTURE_EXTERNAL_OES (on ARM).
   */
  void (*ProvidePictureBuffers)(PP_Instance instance,
                                PP_Resource decoder,
                                uint32_t req_num_of_bufs,
                                const struct PP_Size* dimensions,
                                uint32_t texture_target);
  /**
   * Callback function for decoder to deliver unneeded picture buffers back to
   * the plugin.
   *
   * Parameters:
   *  |instance| the plugin instance to which the callback is responding.
   *  |decoder| the PPB_VideoDecoder_Dev resource.
   *  |picture_buffer| points to the picture buffer that is no longer needed.
   */
  void (*DismissPictureBuffer)(PP_Instance instance,
                               PP_Resource decoder,
                               int32_t picture_buffer_id);
  /**
   * Callback function for decoder to deliver decoded pictures ready to be
   * displayed. Decoder expects the plugin to return the buffer back to the
   * decoder through ReusePictureBuffer function in PPB Video Decoder API.
   *
   * Parameters:
   *  |instance| the plugin instance to which the callback is responding.
   *  |decoder| the PPB_VideoDecoder_Dev resource.
   *  |picture| is the picture that is ready.
   */
  void (*PictureReady)(PP_Instance instance,
                       PP_Resource decoder,
                       const struct PP_Picture_Dev* picture);
  /**
   * Error handler callback for decoder to deliver information about detected
   * errors to the plugin.
   *
   * Parameters:
   *  |instance| the plugin instance to which the callback is responding.
   *  |decoder| the PPB_VideoDecoder_Dev resource.
   *  |error| error is the enumeration specifying the error.
   */
  void (*NotifyError)(PP_Instance instance,
                      PP_Resource decoder,
                      PP_VideoDecodeError_Dev error);
};

typedef struct PPP_VideoDecoder_Dev_0_11 PPP_VideoDecoder_Dev;
/**
 * @}
 */

#endif  /* PPAPI_C_DEV_PPP_VIDEO_DECODER_DEV_H_ */

