/* Copyright 2014 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From pp_codecs.idl modified Fri Mar 29 17:59:41 2019. */

#ifndef PPAPI_C_PP_CODECS_H_
#define PPAPI_C_PP_CODECS_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_point.h"
#include "ppapi/c/pp_rect.h"
#include "ppapi/c/pp_size.h"
#include "ppapi/c/pp_stdint.h"

/**
 * @file
 * Video profiles.
 */


/**
 * @addtogroup Enums
 * @{
 */
typedef enum {
  PP_VIDEOPROFILE_H264BASELINE = 0,
  PP_VIDEOPROFILE_H264MAIN = 1,
  PP_VIDEOPROFILE_H264EXTENDED = 2,
  PP_VIDEOPROFILE_H264HIGH = 3,
  PP_VIDEOPROFILE_H264HIGH10PROFILE = 4,
  PP_VIDEOPROFILE_H264HIGH422PROFILE = 5,
  PP_VIDEOPROFILE_H264HIGH444PREDICTIVEPROFILE = 6,
  PP_VIDEOPROFILE_H264SCALABLEBASELINE = 7,
  PP_VIDEOPROFILE_H264SCALABLEHIGH = 8,
  PP_VIDEOPROFILE_H264STEREOHIGH = 9,
  PP_VIDEOPROFILE_H264MULTIVIEWHIGH = 10,
  PP_VIDEOPROFILE_VP8_ANY = 11,
  PP_VIDEOPROFILE_VP9_ANY = 12,
  PP_VIDEOPROFILE_MAX = PP_VIDEOPROFILE_VP9_ANY
} PP_VideoProfile;

/**
 * Hardware acceleration options.
 */
typedef enum {
  /** Create a hardware accelerated resource only. */
  PP_HARDWAREACCELERATION_ONLY = 0,
  /**
   * Create a hardware accelerated resource if possible. Otherwise, fall back
   * to the software implementation.
   */
  PP_HARDWAREACCELERATION_WITHFALLBACK = 1,
  /** Create the software implementation only. */
  PP_HARDWAREACCELERATION_NONE = 2,
  PP_HARDWAREACCELERATION_LAST = PP_HARDWAREACCELERATION_NONE
} PP_HardwareAcceleration;
/**
 * @}
 */

/**
 * @addtogroup Structs
 * @{
 */
/**
 * Struct describing a decoded video picture. The decoded picture data is stored
 * in the GL texture corresponding to |texture_id|. The plugin can determine
 * which Decode call generated the picture using |decode_id|.
 */
struct PP_VideoPicture {
  /**
   * |decode_id| parameter of the Decode call which generated this picture.
   * See the PPB_VideoDecoder function Decode() for more details.
   */
  uint32_t decode_id;
  /**
   * Texture ID in the plugin's GL context. The plugin can use this to render
   * the decoded picture.
   */
  uint32_t texture_id;
  /**
   * The GL texture target for the decoded picture. Possible values are:
   *   GL_TEXTURE_2D
   *   GL_TEXTURE_RECTANGLE_ARB
   *   GL_TEXTURE_EXTERNAL_OES
   *
   * The pixel format of the texture is GL_RGBA.
   */
  uint32_t texture_target;
  /**
   * Dimensions of the texture holding the decoded picture.
   */
  struct PP_Size texture_size;
  /**
   * The visible subrectangle of the picture. The plugin should display only
   * this part of the picture.
   */
  struct PP_Rect visible_rect;
};

/**
 * Struct describing a decoded video picture. The decoded picture data is stored
 * in the GL texture corresponding to |texture_id|. The plugin can determine
 * which Decode call generated the picture using |decode_id|.
 */
struct PP_VideoPicture_0_1 {
  /**
   * |decode_id| parameter of the Decode call which generated this picture.
   * See the PPB_VideoDecoder function Decode() for more details.
   */
  uint32_t decode_id;
  /**
   * Texture ID in the plugin's GL context. The plugin can use this to render
   * the decoded picture.
   */
  uint32_t texture_id;
  /**
   * The GL texture target for the decoded picture. Possible values are:
   *   GL_TEXTURE_2D
   *   GL_TEXTURE_RECTANGLE_ARB
   *   GL_TEXTURE_EXTERNAL_OES
   *
   * The pixel format of the texture is GL_RGBA.
   */
  uint32_t texture_target;
  /**
   * Dimensions of the texture holding the decoded picture.
   */
  struct PP_Size texture_size;
};

/**
 * Supported video profile information. See the PPB_VideoEncoder function
 * GetSupportedProfiles() for more details.
 */
struct PP_VideoProfileDescription {
  /**
   * The codec profile.
   */
  PP_VideoProfile profile;
  /**
   * Dimensions of the maximum resolution of video frames, in pixels.
   */
  struct PP_Size max_resolution;
  /**
   * The numerator of the maximum frame rate.
   */
  uint32_t max_framerate_numerator;
  /**
   * The denominator of the maximum frame rate.
   */
  uint32_t max_framerate_denominator;
  /**
   * Whether the profile is hardware accelerated.
   */
  PP_Bool hardware_accelerated;
};

/**
 * Supported video profile information. See the PPB_VideoEncoder function
 * GetSupportedProfiles() for more details.
 */
struct PP_VideoProfileDescription_0_1 {
  /**
   * The codec profile.
   */
  PP_VideoProfile profile;
  /**
   * Dimensions of the maximum resolution of video frames, in pixels.
   */
  struct PP_Size max_resolution;
  /**
   * The numerator of the maximum frame rate.
   */
  uint32_t max_framerate_numerator;
  /**
   * The denominator of the maximum frame rate.
   */
  uint32_t max_framerate_denominator;
  /**
   * A value indicating if the profile is available in hardware, software, or
   * both.
   */
  PP_HardwareAcceleration acceleration;
};

/**
 * Struct describing a bitstream buffer.
 */
struct PP_BitstreamBuffer {
  /**
   * The size, in bytes, of the bitstream data.
   */
  uint32_t size;
  /**
   * The base address of the bitstream data.
   */
  void* buffer;
  /**
   * Whether the buffer represents a key frame.
   */
  PP_Bool key_frame;
};

/**
 * Struct describing an audio bitstream buffer.
 */
struct PP_AudioBitstreamBuffer {
  /**
   * The size, in bytes, of the bitstream data.
   */
  uint32_t size;
  /**
   * The base address of the bitstream data.
   */
  void* buffer;
};
/**
 * @}
 */

#endif  /* PPAPI_C_PP_CODECS_H_ */

