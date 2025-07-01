/* Copyright 2015 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From ppb_video_encoder.idl modified Wed Jul 15 11:34:20 2015. */

#ifndef PPAPI_C_PPB_VIDEO_ENCODER_H_
#define PPAPI_C_PPB_VIDEO_ENCODER_H_

#include "ppapi/c/pp_array_output.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_codecs.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_size.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/ppb_video_frame.h"

#define PPB_VIDEOENCODER_INTERFACE_0_1 "PPB_VideoEncoder;0.1" /* dev */
#define PPB_VIDEOENCODER_INTERFACE_0_2 "PPB_VideoEncoder;0.2"
#define PPB_VIDEOENCODER_INTERFACE PPB_VIDEOENCODER_INTERFACE_0_2

/**
 * @file
 * This file defines the <code>PPB_VideoEncoder</code> interface.
 */


/**
 * @addtogroup Interfaces
 * @{
 */
/**
 * Video encoder interface.
 *
 * Typical usage:
 * - Call Create() to create a new video encoder resource.
 * - Call GetSupportedFormats() to determine which codecs and profiles are
 *   available.
 * - Call Initialize() to initialize the encoder for a supported profile.
 * - Call GetVideoFrame() to get a blank frame and fill it in, or get a video
 *   frame from another resource, e.g. <code>PPB_MediaStreamVideoTrack</code>.
 * - Call Encode() to push the video frame to the encoder. If an external frame
 *   is pushed, wait for completion to recycle the frame.
 * - Call GetBitstreamBuffer() continuously (waiting for each previous call to
 *   complete) to pull encoded pictures from the encoder.
 * - Call RecycleBitstreamBuffer() after consuming the data in the bitstream
 *   buffer.
 * - To destroy the encoder, the plugin should release all of its references to
 *   it. Any pending callbacks will abort before the encoder is destroyed.
 *
 * Available video codecs vary by platform.
 * All: vp8 (software).
 * ChromeOS, depending on your device: h264 (hardware), vp8 (hardware)
 */
struct PPB_VideoEncoder_0_2 {
  /**
   * Creates a new video encoder resource.
   *
   * @param[in] instance A <code>PP_Instance</code> identifying the instance
   * with the video encoder.
   *
   * @return A <code>PP_Resource</code> corresponding to a video encoder if
   * successful or 0 otherwise.
   */
  PP_Resource (*Create)(PP_Instance instance);
  /**
   * Determines if the given resource is a video encoder.
   *
   * @param[in] resource A <code>PP_Resource</code> identifying a resource.
   *
   * @return <code>PP_TRUE</code> if the resource is a
   * <code>PPB_VideoEncoder</code>, <code>PP_FALSE</code> if the resource is
   * invalid or some other type.
   */
  PP_Bool (*IsVideoEncoder)(PP_Resource resource);
  /**
   * Gets an array of supported video encoder profiles.
   * These can be used to choose a profile before calling Initialize().
   *
   * @param[in] video_encoder A <code>PP_Resource</code> identifying the video
   * encoder.
   * @param[in] output A <code>PP_ArrayOutput</code> to receive the supported
   * <code>PP_VideoProfileDescription</code> structs.
   * @param[in] callback A <code>PP_CompletionCallback</code> to be called upon
   * completion.
   *
   * @return If >= 0, the number of supported profiles returned, otherwise an
   * error code from <code>pp_errors.h</code>.
   */
  int32_t (*GetSupportedProfiles)(PP_Resource video_encoder,
                                  struct PP_ArrayOutput output,
                                  struct PP_CompletionCallback callback);
  /**
   * Initializes a video encoder resource. The plugin should call Initialize()
   * successfully before calling any of the functions below.
   *
   * @param[in] video_encoder A <code>PP_Resource</code> identifying the video
   * encoder.
   * @param[in] input_format The <code>PP_VideoFrame_Format</code> of the
   * frames which will be encoded.
   * @param[in] input_visible_size A <code>PP_Size</code> specifying the
   * dimensions of the visible part of the input frames.
   * @param[in] output_profile A <code>PP_VideoProfile</code> specifying the
   * codec profile of the encoded output stream.
   * @param[in] acceleration A <code>PP_HardwareAcceleration</code> specifying
   * whether to use a hardware accelerated or a software implementation.
   * @param[in] callback A <code>PP_CompletionCallback</code> to be called upon
   * completion.
   *
   * @return An int32_t containing an error code from <code>pp_errors.h</code>.
   * Returns PP_ERROR_NOTSUPPORTED if video encoding is not available, or the
   * requested codec profile is not supported.
   */
  int32_t (*Initialize)(PP_Resource video_encoder,
                        PP_VideoFrame_Format input_format,
                        const struct PP_Size* input_visible_size,
                        PP_VideoProfile output_profile,
                        uint32_t initial_bitrate,
                        PP_HardwareAcceleration acceleration,
                        struct PP_CompletionCallback callback);
  /**
   * Gets the number of input video frames that the encoder may hold while
   * encoding. If the plugin is providing the video frames, it should have at
   * least this many available.
   *
   * @param[in] video_encoder A <code>PP_Resource</code> identifying the video
   * encoder.
   * @return An int32_t containing the number of frames required, or an error
   * code from <code>pp_errors.h</code>.
   * Returns PP_ERROR_FAILED if Initialize() has not successfully completed.
   */
  int32_t (*GetFramesRequired)(PP_Resource video_encoder);
  /**
   * Gets the coded size of the video frames required by the encoder. Coded
   * size is the logical size of the input frames, in pixels.  The encoder may
   * have hardware alignment requirements that make this different from
   * |input_visible_size|, as requested in the call to Initialize().
   *
   * @param[in] video_encoder A <code>PP_Resource</code> identifying the video
   * encoder.
   * @param[in] coded_size A <code>PP_Size</code> to hold the coded size.
   * @return An int32_t containing a result code from <code>pp_errors.h</code>.
   * Returns PP_ERROR_FAILED if Initialize() has not successfully completed.
   */
  int32_t (*GetFrameCodedSize)(PP_Resource video_encoder,
                               struct PP_Size* coded_size);
  /**
   * Gets a blank video frame which can be filled with video data and passed
   * to the encoder.
   *
   * @param[in] video_encoder A <code>PP_Resource</code> identifying the video
   * encoder.
   * @param[out] video_frame A blank <code>PPB_VideoFrame</code> resource.
   * @param[in] callback A <code>PP_CompletionCallback</code> to be called upon
   * completion.
   *
   * @return An int32_t containing an error code from <code>pp_errors.h</code>.
   * Returns PP_ERROR_FAILED if Initialize() has not successfully completed.
   */
  int32_t (*GetVideoFrame)(PP_Resource video_encoder,
                           PP_Resource* video_frame,
                           struct PP_CompletionCallback callback);
  /**
   * Encodes a video frame.
   *
   * @param[in] video_encoder A <code>PP_Resource</code> identifying the video
   * encoder.
   * @param[in] video_frame The <code>PPB_VideoFrame</code> to be encoded.
   * @param[in] force_keyframe A <code>PP_Bool> specifying whether the encoder
   * should emit a key frame for this video frame.
   * @param[in] callback A <code>PP_CompletionCallback</code> to be called upon
   * completion. Plugins that pass <code>PPB_VideoFrame</code> resources owned
   * by other resources should wait for completion before reusing them.
   *
   * @return An int32_t containing an error code from <code>pp_errors.h</code>.
   * Returns PP_ERROR_FAILED if Initialize() has not successfully completed.
   */
  int32_t (*Encode)(PP_Resource video_encoder,
                    PP_Resource video_frame,
                    PP_Bool force_keyframe,
                    struct PP_CompletionCallback callback);
  /**
   * Gets the next encoded bitstream buffer from the encoder.
   *
   * @param[in] video_encoder A <code>PP_Resource</code> identifying the video
   * encoder.
   * @param[out] bitstream_buffer A <code>PP_BitstreamBuffer</code> containing
   * encoded video data.
   * @param[in] callback A <code>PP_CompletionCallback</code> to be called upon
   * completion. The plugin can call GetBitstreamBuffer from the callback in
   * order to continuously "pull" bitstream buffers from the encoder.
   *
   * @return An int32_t containing an error code from <code>pp_errors.h</code>.
   * Returns PP_ERROR_FAILED if Initialize() has not successfully completed.
   * Returns PP_ERROR_INPROGRESS if a prior call to GetBitstreamBuffer() has
   * not completed.
   */
  int32_t (*GetBitstreamBuffer)(PP_Resource video_encoder,
                                struct PP_BitstreamBuffer* bitstream_buffer,
                                struct PP_CompletionCallback callback);
  /**
   * Recycles a bitstream buffer back to the encoder.
   *
   * @param[in] video_encoder A <code>PP_Resource</code> identifying the video
   * encoder.
   * @param[in] bitstream_buffer A <code>PP_BitstreamBuffer</code> that is no
   * longer needed by the plugin.
   */
  void (*RecycleBitstreamBuffer)(
      PP_Resource video_encoder,
      const struct PP_BitstreamBuffer* bitstream_buffer);
  /**
   * Requests a change to encoding parameters. This is only a request,
   * fulfilled on a best-effort basis.
   *
   * @param[in] video_encoder A <code>PP_Resource</code> identifying the video
   * encoder.
   * @param[in] bitrate The requested new bitrate, in bits per second.
   * @param[in] framerate The requested new framerate, in frames per second.
   */
  void (*RequestEncodingParametersChange)(PP_Resource video_encoder,
                                          uint32_t bitrate,
                                          uint32_t framerate);
  /**
   * Closes the video encoder, and cancels any pending encodes. Any pending
   * callbacks will still run, reporting <code>PP_ERROR_ABORTED</code> . It is
   * not valid to call any encoder functions after a call to this method.
   * <strong>Note:</strong> Destroying the video encoder closes it implicitly,
   * so you are not required to call Close().
   *
   * @param[in] video_encoder A <code>PP_Resource</code> identifying the video
   * encoder.
   */
  void (*Close)(PP_Resource video_encoder);
};

typedef struct PPB_VideoEncoder_0_2 PPB_VideoEncoder;

struct PPB_VideoEncoder_0_1 { /* dev */
  PP_Resource (*Create)(PP_Instance instance);
  PP_Bool (*IsVideoEncoder)(PP_Resource resource);
  int32_t (*GetSupportedProfiles)(PP_Resource video_encoder,
                                  struct PP_ArrayOutput output,
                                  struct PP_CompletionCallback callback);
  int32_t (*Initialize)(PP_Resource video_encoder,
                        PP_VideoFrame_Format input_format,
                        const struct PP_Size* input_visible_size,
                        PP_VideoProfile output_profile,
                        uint32_t initial_bitrate,
                        PP_HardwareAcceleration acceleration,
                        struct PP_CompletionCallback callback);
  int32_t (*GetFramesRequired)(PP_Resource video_encoder);
  int32_t (*GetFrameCodedSize)(PP_Resource video_encoder,
                               struct PP_Size* coded_size);
  int32_t (*GetVideoFrame)(PP_Resource video_encoder,
                           PP_Resource* video_frame,
                           struct PP_CompletionCallback callback);
  int32_t (*Encode)(PP_Resource video_encoder,
                    PP_Resource video_frame,
                    PP_Bool force_keyframe,
                    struct PP_CompletionCallback callback);
  int32_t (*GetBitstreamBuffer)(PP_Resource video_encoder,
                                struct PP_BitstreamBuffer* bitstream_buffer,
                                struct PP_CompletionCallback callback);
  void (*RecycleBitstreamBuffer)(
      PP_Resource video_encoder,
      const struct PP_BitstreamBuffer* bitstream_buffer);
  void (*RequestEncodingParametersChange)(PP_Resource video_encoder,
                                          uint32_t bitrate,
                                          uint32_t framerate);
  void (*Close)(PP_Resource video_encoder);
};
/**
 * @}
 */

#endif  /* PPAPI_C_PPB_VIDEO_ENCODER_H_ */

