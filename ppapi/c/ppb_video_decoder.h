/* Copyright 2014 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From ppb_video_decoder.idl modified Mon Sep 28 15:23:30 2015. */

#ifndef PPAPI_C_PPB_VIDEO_DECODER_H_
#define PPAPI_C_PPB_VIDEO_DECODER_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_codecs.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_point.h"
#include "ppapi/c/pp_rect.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_size.h"
#include "ppapi/c/pp_stdint.h"

#define PPB_VIDEODECODER_INTERFACE_0_1 "PPB_VideoDecoder;0.1"
#define PPB_VIDEODECODER_INTERFACE_0_2 "PPB_VideoDecoder;0.2"
#define PPB_VIDEODECODER_INTERFACE_1_0 "PPB_VideoDecoder;1.0"
#define PPB_VIDEODECODER_INTERFACE_1_1 "PPB_VideoDecoder;1.1"
#define PPB_VIDEODECODER_INTERFACE PPB_VIDEODECODER_INTERFACE_1_1

/**
 * @file
 * This file defines the <code>PPB_VideoDecoder</code> interface.
 */


/**
 * @addtogroup Interfaces
 * @{
 */
/**
 * Video decoder interface.
 *
 * Typical usage:
 * - Call Create() to create a new video decoder resource.
 * - Call Initialize() to initialize it with a 3d graphics context and the
 *   desired codec profile.
 * - Call Decode() continuously (waiting for each previous call to complete) to
 *   push bitstream buffers to the decoder.
 * - Call GetPicture() continuously (waiting for each previous call to complete)
 *   to pull decoded pictures from the decoder.
 * - Call Flush() to signal end of stream to the decoder and perform shutdown
 *   when it completes.
 * - Call Reset() to quickly stop the decoder (e.g. to implement Seek) and wait
 *   for the callback before restarting decoding at another point.
 * - To destroy the decoder, the plugin should release all of its references to
 *   it. Any pending callbacks will abort before the decoder is destroyed.
 *
 * Available video codecs vary by platform.
 * All: theora, vorbis, vp8.
 * Chrome and ChromeOS: aac, h264.
 * ChromeOS: mpeg4.
 */
struct PPB_VideoDecoder_1_1 {
  /**
   * Creates a new video decoder resource.
   *
   * @param[in] instance A <code>PP_Instance</code> identifying the instance
   * with the video decoder.
   *
   * @return A <code>PP_Resource</code> corresponding to a video decoder if
   * successful or 0 otherwise.
   */
  PP_Resource (*Create)(PP_Instance instance);
  /**
   * Determines if the given resource is a video decoder.
   *
   * @param[in] resource A <code>PP_Resource</code> identifying a resource.
   *
   * @return <code>PP_TRUE</code> if the resource is a
   * <code>PPB_VideoDecoder</code>, <code>PP_FALSE</code> if the resource is
   * invalid or some other type.
   */
  PP_Bool (*IsVideoDecoder)(PP_Resource resource);
  /**
   * Initializes a video decoder resource. This should be called after Create()
   * and before any other functions.
   *
   * @param[in] video_decoder A <code>PP_Resource</code> identifying the video
   * decoder.
   * @param[in] graphics3d_context A <code>PPB_Graphics3D</code> resource to use
   * during decoding.
   * @param[in] profile A <code>PP_VideoProfile</code> specifying the video
   * codec profile.
   * @param[in] acceleration A <code>PP_HardwareAcceleration</code> specifying
   * whether to use a hardware accelerated or a software implementation.
   * @param[in] min_picture_count A count of pictures the plugin would like to
   * have in flight. This is effectively the number of times the plugin can
   * call GetPicture() and get a decoded frame without calling
   * RecyclePicture(). The decoder has its own internal minimum count, and will
   * take the larger of its internal and this value. A client that doesn't care
   * can therefore just pass in zero for this argument.
   * @param[in] callback A <code>PP_CompletionCallback</code> to be called upon
   * completion.
   *
   * @return An int32_t containing an error code from <code>pp_errors.h</code>.
   * Returns PP_ERROR_NOTSUPPORTED if video decoding is not available, or the
   * requested profile is not supported. In this case, the client may call
   * Initialize() again with different parameters to find a good configuration.
   * Returns PP_ERROR_BADARGUMENT if the requested minimum picture count is
   * unreasonably large.
   */
  int32_t (*Initialize)(PP_Resource video_decoder,
                        PP_Resource graphics3d_context,
                        PP_VideoProfile profile,
                        PP_HardwareAcceleration acceleration,
                        uint32_t min_picture_count,
                        struct PP_CompletionCallback callback);
  /**
   * Decodes a bitstream buffer. Copies |size| bytes of data from the plugin's
   * |buffer|. The plugin should wait until the decoder signals completion by
   * returning PP_OK or by running |callback| before calling Decode() again.
   *
   * In general, each bitstream buffer should contain a demuxed bitstream frame
   * for the selected video codec. For example, H264 decoders expect to receive
   * one AnnexB NAL unit, including the 4 byte start code prefix, while VP8
   * decoders expect to receive a bitstream frame without the IVF frame header.
   *
   * If the call to Decode() eventually results in a picture, the |decode_id|
   * parameter is copied into the returned picture. The plugin can use this to
   * associate decoded pictures with Decode() calls (e.g. to assign timestamps
   * or frame numbers to pictures.) This value is opaque to the API so the
   * plugin is free to pass any value.
   *
   * @param[in] video_decoder A <code>PP_Resource</code> identifying the video
   * decoder.
   * @param[in] decode_id An optional value, chosen by the plugin, that can be
   * used to associate calls to Decode() with decoded pictures returned by
   * GetPicture().
   * @param[in] size Buffer size in bytes.
   * @param[in] buffer Starting address of buffer.
   * @param[in] callback A <code>PP_CompletionCallback</code> to be called on
   * completion.
   *
   * @return An int32_t containing an error code from <code>pp_errors.h</code>.
   * Returns PP_ERROR_FAILED if the decoder isn't initialized or if a Flush()
   * or Reset() call is pending.
   * Returns PP_ERROR_INPROGRESS if there is another Decode() call pending.
   * Returns PP_ERROR_NOMEMORY if a bitstream buffer can't be created.
   * Returns PP_ERROR_ABORTED when Reset() is called while Decode() is pending.
   */
  int32_t (*Decode)(PP_Resource video_decoder,
                    uint32_t decode_id,
                    uint32_t size,
                    const void* buffer,
                    struct PP_CompletionCallback callback);
  /**
   * Gets the next picture from the decoder. The picture is valid after the
   * decoder signals completion by returning PP_OK or running |callback|. The
   * plugin can call GetPicture() again after the decoder signals completion.
   * When the plugin is finished using the picture, it should return it to the
   * system by calling RecyclePicture().
   *
   * @param[in] video_decoder A <code>PP_Resource</code> identifying the video
   * decoder.
   * @param[out] picture A <code>PP_VideoPicture</code> to hold the decoded
   * picture.
   * @param[in] callback A <code>PP_CompletionCallback</code> to be called on
   * completion.
   *
   * @return An int32_t containing an error code from <code>pp_errors.h</code>.
   * Returns PP_ERROR_FAILED if the decoder isn't initialized or if a Reset()
   * call is pending.
   * Returns PP_ERROR_INPROGRESS if there is another GetPicture() call pending.
   * Returns PP_ERROR_ABORTED when Reset() is called, or if a call to Flush()
   * completes while GetPicture() is pending.
   */
  int32_t (*GetPicture)(PP_Resource video_decoder,
                        struct PP_VideoPicture* picture,
                        struct PP_CompletionCallback callback);
  /**
   * Recycles a picture that the plugin has received from the decoder.
   * The plugin should call this as soon as it has finished using the texture so
   * the decoder can decode more pictures.
   *
   * @param[in] video_decoder A <code>PP_Resource</code> identifying the video
   * decoder.
   * @param[in] picture A <code>PP_VideoPicture</code> to return to
   * the decoder.
   */
  void (*RecyclePicture)(PP_Resource video_decoder,
                         const struct PP_VideoPicture* picture);
  /**
   * Flushes the decoder. The plugin should call Flush() when it reaches the
   * end of its video stream in order to stop cleanly. The decoder will run any
   * pending Decode() call to completion. The plugin should make no further
   * calls to the decoder other than GetPicture() and RecyclePicture() until
   * the decoder signals completion by running |callback|. Just before
   * completion, any pending GetPicture() call will complete by running its
   * callback with result PP_ERROR_ABORTED to signal that no more pictures are
   * available. Any pictures held by the plugin remain valid during and after
   * the flush and should be recycled back to the decoder.
   *
   * @param[in] video_decoder A <code>PP_Resource</code> identifying the video
   * decoder.
   * @param[in] callback A <code>PP_CompletionCallback</code> to be called on
   * completion.
   *
   * @return An int32_t containing an error code from <code>pp_errors.h</code>.
   * Returns PP_ERROR_FAILED if the decoder isn't initialized.
   */
  int32_t (*Flush)(PP_Resource video_decoder,
                   struct PP_CompletionCallback callback);
  /**
   * Resets the decoder as quickly as possible. The plugin can call Reset() to
   * skip to another position in the video stream. After Reset() returns, any
   * pending calls to Decode() and GetPicture()) abort, causing their callbacks
   * to run with PP_ERROR_ABORTED. The plugin should not make further calls to
   * the decoder other than RecyclePicture() until the decoder signals
   * completion by running |callback|. Any pictures held by the plugin remain
   * valid during and after the reset and should be recycled back to the
   * decoder.
   *
   * @param[in] video_decoder A <code>PP_Resource</code> identifying the video
   * decoder.
   * @param[in] callback A <code>PP_CompletionCallback</code> to be called on
   * completion.
   *
   * @return An int32_t containing an error code from <code>pp_errors.h</code>.
   * Returns PP_ERROR_FAILED if the decoder isn't initialized.
   */
  int32_t (*Reset)(PP_Resource video_decoder,
                   struct PP_CompletionCallback callback);
};

typedef struct PPB_VideoDecoder_1_1 PPB_VideoDecoder;

struct PPB_VideoDecoder_0_1 {
  PP_Resource (*Create)(PP_Instance instance);
  PP_Bool (*IsVideoDecoder)(PP_Resource resource);
  int32_t (*Initialize)(PP_Resource video_decoder,
                        PP_Resource graphics3d_context,
                        PP_VideoProfile profile,
                        PP_Bool allow_software_fallback,
                        struct PP_CompletionCallback callback);
  int32_t (*Decode)(PP_Resource video_decoder,
                    uint32_t decode_id,
                    uint32_t size,
                    const void* buffer,
                    struct PP_CompletionCallback callback);
  int32_t (*GetPicture)(PP_Resource video_decoder,
                        struct PP_VideoPicture_0_1* picture,
                        struct PP_CompletionCallback callback);
  void (*RecyclePicture)(PP_Resource video_decoder,
                         const struct PP_VideoPicture* picture);
  int32_t (*Flush)(PP_Resource video_decoder,
                   struct PP_CompletionCallback callback);
  int32_t (*Reset)(PP_Resource video_decoder,
                   struct PP_CompletionCallback callback);
};

struct PPB_VideoDecoder_0_2 {
  PP_Resource (*Create)(PP_Instance instance);
  PP_Bool (*IsVideoDecoder)(PP_Resource resource);
  int32_t (*Initialize)(PP_Resource video_decoder,
                        PP_Resource graphics3d_context,
                        PP_VideoProfile profile,
                        PP_HardwareAcceleration acceleration,
                        struct PP_CompletionCallback callback);
  int32_t (*Decode)(PP_Resource video_decoder,
                    uint32_t decode_id,
                    uint32_t size,
                    const void* buffer,
                    struct PP_CompletionCallback callback);
  int32_t (*GetPicture)(PP_Resource video_decoder,
                        struct PP_VideoPicture_0_1* picture,
                        struct PP_CompletionCallback callback);
  void (*RecyclePicture)(PP_Resource video_decoder,
                         const struct PP_VideoPicture* picture);
  int32_t (*Flush)(PP_Resource video_decoder,
                   struct PP_CompletionCallback callback);
  int32_t (*Reset)(PP_Resource video_decoder,
                   struct PP_CompletionCallback callback);
};

struct PPB_VideoDecoder_1_0 {
  PP_Resource (*Create)(PP_Instance instance);
  PP_Bool (*IsVideoDecoder)(PP_Resource resource);
  int32_t (*Initialize)(PP_Resource video_decoder,
                        PP_Resource graphics3d_context,
                        PP_VideoProfile profile,
                        PP_HardwareAcceleration acceleration,
                        struct PP_CompletionCallback callback);
  int32_t (*Decode)(PP_Resource video_decoder,
                    uint32_t decode_id,
                    uint32_t size,
                    const void* buffer,
                    struct PP_CompletionCallback callback);
  int32_t (*GetPicture)(PP_Resource video_decoder,
                        struct PP_VideoPicture* picture,
                        struct PP_CompletionCallback callback);
  void (*RecyclePicture)(PP_Resource video_decoder,
                         const struct PP_VideoPicture* picture);
  int32_t (*Flush)(PP_Resource video_decoder,
                   struct PP_CompletionCallback callback);
  int32_t (*Reset)(PP_Resource video_decoder,
                   struct PP_CompletionCallback callback);
};
/**
 * @}
 */

#endif  /* PPAPI_C_PPB_VIDEO_DECODER_H_ */

