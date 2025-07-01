/* Copyright 2012 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From ppb_audio_config.idl modified Mon Oct 23 15:24:19 2017. */

#ifndef PPAPI_C_PPB_AUDIO_CONFIG_H_
#define PPAPI_C_PPB_AUDIO_CONFIG_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"

#define PPB_AUDIO_CONFIG_INTERFACE_1_0 "PPB_AudioConfig;1.0"
#define PPB_AUDIO_CONFIG_INTERFACE_1_1 "PPB_AudioConfig;1.1"
#define PPB_AUDIO_CONFIG_INTERFACE PPB_AUDIO_CONFIG_INTERFACE_1_1

/**
 * @file
 * This file defines the PPB_AudioConfig interface for establishing an
 * audio configuration resource within the browser.
 */


/**
 * @addtogroup Enums
 * @{
 */
/**
 * This enumeration contains audio frame count constants.
 * <code>PP_AUDIOMINSAMPLEFRAMECOUNT</code> is the minimum possible frame
 * count. <code>PP_AUDIOMAXSAMPLEFRAMECOUNT</code> is the maximum possible
 * frame count.
 */
enum {
  PP_AUDIOMINSAMPLEFRAMECOUNT = 64,
  PP_AUDIOMAXSAMPLEFRAMECOUNT = 32768
};

/**
 * PP_AudioSampleRate is an enumeration of the different audio sampling rates.
 * <code>PP_AUDIOSAMPLERATE_44100</code> is the sample rate used on CDs and
 * <code>PP_AUDIOSAMPLERATE_48000</code> is the sample rate used on DVDs and
 * Digital Audio Tapes.
 */
typedef enum {
  PP_AUDIOSAMPLERATE_NONE = 0,
  PP_AUDIOSAMPLERATE_44100 = 44100,
  PP_AUDIOSAMPLERATE_48000 = 48000,
  PP_AUDIOSAMPLERATE_LAST = PP_AUDIOSAMPLERATE_48000
} PP_AudioSampleRate;
PP_COMPILE_ASSERT_SIZE_IN_BYTES(PP_AudioSampleRate, 4);
/**
 * @}
 */

/**
 * @addtogroup Interfaces
 * @{
 */
/**
 * The <code>PPB_AudioConfig</code> interface contains pointers to several
 * functions for establishing your audio configuration within the browser.
 * This interface only supports 16-bit stereo output.
 *
 * Refer to the
 * <a href="/native-client/devguide/coding/audio.html">Audio
 * </a> chapter in the Developer's Guide for information on using this
 * interface.
 */
struct PPB_AudioConfig_1_1 {
  /**
   * CreateStereo16bit() creates a 16 bit audio configuration resource. The
   * <code>sample_rate</code> should be the result of calling
   * <code>RecommendSampleRate</code> and <code>sample_frame_count</code> should
   * be the result of calling <code>RecommendSampleFrameCount</code>. If the
   * sample frame count or bit rate isn't supported, this function will fail and
   * return a null resource.
   *
   * A single sample frame on a stereo device means one value for the left
   * channel and one value for the right channel.
   *
   * Buffer layout for a stereo int16 configuration:
   * <code>int16_t *buffer16;</code>
   * <code>buffer16[0]</code> is the first left channel sample.
   * <code>buffer16[1]</code> is the first right channel sample.
   * <code>buffer16[2]</code> is the second left channel sample.
   * <code>buffer16[3]</code> is the second right channel sample.
   * ...
   * <code>buffer16[2 * (sample_frame_count - 1)]</code> is the last left
   * channel sample.
   * <code>buffer16[2 * (sample_frame_count - 1) + 1]</code> is the last
   * right channel sample.
   * Data will always be in the native endian format of the platform.
   *
   * @param[in] instance A <code>PP_Instance</code> identifying one instance
   * of a module.
   * @param[in] sample_rate A <code>PP_AudioSampleRate</code> which is either
   * <code>PP_AUDIOSAMPLERATE_44100</code> or
   * <code>PP_AUDIOSAMPLERATE_48000</code>.
   * @param[in] sample_frame_count A <code>uint32_t</code> frame count returned
   * from the <code>RecommendSampleFrameCount</code> function.
   *
   * @return A <code>PP_Resource</code> containing the
   * <code>PPB_Audio_Config</code> if successful or a null resource if the
   * sample frame count or bit rate are not supported.
   */
  PP_Resource (*CreateStereo16Bit)(PP_Instance instance,
                                   PP_AudioSampleRate sample_rate,
                                   uint32_t sample_frame_count);
  /**
   * RecommendSampleFrameCount() returns the supported sample frame count
   * closest to the requested count. The sample frame count determines the
   * overall latency of audio. Since one "frame" is always buffered in advance,
   * smaller frame counts will yield lower latency, but higher CPU utilization.
   *
   * Supported sample frame counts will vary by hardware and system (consider
   * that the local system might be anywhere from a cell phone or a high-end
   * audio workstation). Sample counts less than
   * <code>PP_AUDIOMINSAMPLEFRAMECOUNT</code> and greater than
   * <code>PP_AUDIOMAXSAMPLEFRAMECOUNT</code> are never supported on any
   * system, but values in between aren't necessarily valid. This function
   * will return a supported count closest to the requested frame count.
   *
   * RecommendSampleFrameCount() result is intended for audio output devices.
   *
   * @param[in] instance
   * @param[in] sample_rate A <code>PP_AudioSampleRate</code> which is either
   * <code>PP_AUDIOSAMPLERATE_44100</code> or
   * <code>PP_AUDIOSAMPLERATE_48000.</code>
   * @param[in] requested_sample_frame_count A <code>uint_32t</code> requested
   * frame count.
   *
   * @return A <code>uint32_t</code> containing the recommended sample frame
   * count if successful.
   */
  uint32_t (*RecommendSampleFrameCount)(
      PP_Instance instance,
      PP_AudioSampleRate sample_rate,
      uint32_t requested_sample_frame_count);
  /**
   * IsAudioConfig() determines if the given resource is a
   * <code>PPB_Audio_Config</code>.
   *
   * @param[in] resource A <code>PP_Resource</code> corresponding to an audio
   * config resource.
   *
   * @return A <code>PP_Bool</code> containing <code>PP_TRUE</code> if the given
   * resource is an <code>AudioConfig</code> resource, otherwise
   * <code>PP_FALSE</code>.
   */
  PP_Bool (*IsAudioConfig)(PP_Resource resource);
  /**
   * GetSampleRate() returns the sample rate for the given
   * <code>PPB_Audio_Config</code>.
   *
   * @param[in] config A <code>PP_Resource</code> corresponding to a
   * <code>PPB_Audio_Config</code>.
   *
   * @return A <code>PP_AudioSampleRate</code> containing sample rate or
   * <code>PP_AUDIOSAMPLERATE_NONE</code> if the resource is invalid.
   */
  PP_AudioSampleRate (*GetSampleRate)(PP_Resource config);
  /**
   * GetSampleFrameCount() returns the sample frame count for the given
   * <code>PPB_Audio_Config</code>.
   *
   * @param[in] config A <code>PP_Resource</code> corresponding to an audio
   * config resource.
   *
   * @return A <code>uint32_t</code> containing sample frame count or
   * 0 if the resource is invalid. Refer to
   * RecommendSampleFrameCount() for more on sample frame counts.
   */
  uint32_t (*GetSampleFrameCount)(PP_Resource config);
  /**
   * RecommendSampleRate() returns the native sample rate that the browser
   * is using in the backend.  Applications that use the recommended sample
   * rate will have potentially better latency and fidelity.  The return value
   * is intended for audio output devices.  If the output sample rate cannot be
   * determined, this function can return PP_AUDIOSAMPLERATE_NONE.
   *
   * @param[in] instance
   *
   * @return A <code>uint32_t</code> containing the recommended sample frame
   * count if successful.
   */
  PP_AudioSampleRate (*RecommendSampleRate)(PP_Instance instance);
};

typedef struct PPB_AudioConfig_1_1 PPB_AudioConfig;

struct PPB_AudioConfig_1_0 {
  PP_Resource (*CreateStereo16Bit)(PP_Instance instance,
                                   PP_AudioSampleRate sample_rate,
                                   uint32_t sample_frame_count);
  uint32_t (*RecommendSampleFrameCount)(
      PP_AudioSampleRate sample_rate,
      uint32_t requested_sample_frame_count);
  PP_Bool (*IsAudioConfig)(PP_Resource resource);
  PP_AudioSampleRate (*GetSampleRate)(PP_Resource config);
  uint32_t (*GetSampleFrameCount)(PP_Resource config);
};
/**
 * @}
 */

#endif  /* PPAPI_C_PPB_AUDIO_CONFIG_H_ */

