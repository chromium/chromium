/* Copyright 2014 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From ppb_audio_buffer.idl modified Tue Mar 25 18:29:27 2014. */

#ifndef PPAPI_C_PPB_AUDIO_BUFFER_H_
#define PPAPI_C_PPB_AUDIO_BUFFER_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_time.h"

#define PPB_AUDIOBUFFER_INTERFACE_0_1 "PPB_AudioBuffer;0.1"
#define PPB_AUDIOBUFFER_INTERFACE PPB_AUDIOBUFFER_INTERFACE_0_1

/**
 * @file
 * Defines the <code>PPB_AudioBuffer</code> interface.
 */


/**
 * @addtogroup Enums
 * @{
 */
/**
 * PP_AudioBuffer_SampleRate is an enumeration of the different audio sample
 * rates.
 */
typedef enum {
  PP_AUDIOBUFFER_SAMPLERATE_UNKNOWN = 0,
  PP_AUDIOBUFFER_SAMPLERATE_8000 = 8000,
  PP_AUDIOBUFFER_SAMPLERATE_16000 = 16000,
  PP_AUDIOBUFFER_SAMPLERATE_22050 = 22050,
  PP_AUDIOBUFFER_SAMPLERATE_32000 = 32000,
  PP_AUDIOBUFFER_SAMPLERATE_44100 = 44100,
  PP_AUDIOBUFFER_SAMPLERATE_48000 = 48000,
  PP_AUDIOBUFFER_SAMPLERATE_96000 = 96000,
  PP_AUDIOBUFFER_SAMPLERATE_192000 = 192000
} PP_AudioBuffer_SampleRate;

/**
 * PP_AudioBuffer_SampleSize is an enumeration of the different audio sample
 * sizes.
 */
typedef enum {
  PP_AUDIOBUFFER_SAMPLESIZE_UNKNOWN = 0,
  PP_AUDIOBUFFER_SAMPLESIZE_16_BITS = 2
} PP_AudioBuffer_SampleSize;
/**
 * @}
 */

/**
 * @addtogroup Interfaces
 * @{
 */
struct PPB_AudioBuffer_0_1 {
  /**
   * Determines if a resource is an AudioBuffer resource.
   *
   * @param[in] resource The <code>PP_Resource</code> to test.
   *
   * @return A <code>PP_Bool</code> with <code>PP_TRUE</code> if the given
   * resource is an AudioBuffer resource or <code>PP_FALSE</code> otherwise.
   */
  PP_Bool (*IsAudioBuffer)(PP_Resource resource);
  /**
   * Gets the timestamp of the audio buffer.
   *
   * @param[in] buffer A <code>PP_Resource</code> corresponding to an audio
   * buffer resource.
   *
   * @return A <code>PP_TimeDelta</code> containing the timestamp of the audio
   * buffer. Given in seconds since the start of the containing audio stream.
   */
  PP_TimeDelta (*GetTimestamp)(PP_Resource buffer);
  /**
   * Sets the timestamp of the audio buffer.
   *
   * @param[in] buffer A <code>PP_Resource</code> corresponding to an audio
   * buffer resource.
   * @param[in] timestamp A <code>PP_TimeDelta</code> containing the timestamp
   * of the audio buffer. Given in seconds since the start of the containing
   * audio stream.
   */
  void (*SetTimestamp)(PP_Resource buffer, PP_TimeDelta timestamp);
  /**
   * Gets the sample rate of the audio buffer.
   *
   * @param[in] buffer A <code>PP_Resource</code> corresponding to an audio
   * buffer resource.
   *
   * @return The sample rate of the audio buffer.
   */
  PP_AudioBuffer_SampleRate (*GetSampleRate)(PP_Resource buffer);
  /**
   * Gets the sample size of the audio buffer.
   *
   * @param[in] buffer A <code>PP_Resource</code> corresponding to an audio
   * buffer resource.
   *
   * @return The sample size of the audio buffer.
   */
  PP_AudioBuffer_SampleSize (*GetSampleSize)(PP_Resource buffer);
  /**
   * Gets the number of channels in the audio buffer.
   *
   * @param[in] buffer A <code>PP_Resource</code> corresponding to an audio
   * buffer resource.
   *
   * @return The number of channels in the audio buffer.
   */
  uint32_t (*GetNumberOfChannels)(PP_Resource buffer);
  /**
   * Gets the number of samples in the audio buffer.
   *
   * @param[in] buffer A <code>PP_Resource</code> corresponding to an audio
   * buffer resource.
   *
   * @return The number of samples in the audio buffer.
   * For example, at a sampling rate of 44,100 Hz in stereo audio, a buffer
   * containing 4410 * 2 samples would have a duration of 100 milliseconds.
   */
  uint32_t (*GetNumberOfSamples)(PP_Resource buffer);
  /**
   * Gets the data buffer containing the audio samples.
   *
   * @param[in] buffer A <code>PP_Resource</code> corresponding to an audio
   * buffer resource.
   *
   * @return A pointer to the beginning of the data buffer.
   */
  void* (*GetDataBuffer)(PP_Resource buffer);
  /**
   * Gets the size of the data buffer in bytes.
   *
   * @param[in] buffer A <code>PP_Resource</code> corresponding to an audio
   * buffer resource.
   *
   * @return The size of the data buffer in bytes.
   */
  uint32_t (*GetDataBufferSize)(PP_Resource buffer);
};

typedef struct PPB_AudioBuffer_0_1 PPB_AudioBuffer;
/**
 * @}
 */

#endif  /* PPAPI_C_PPB_AUDIO_BUFFER_H_ */

