/* Copyright 2017 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From dev/ppb_audio_output_dev.idl modified Fri Apr  7 07:07:59 2017. */

#ifndef PPAPI_C_DEV_PPB_AUDIO_OUTPUT_DEV_H_
#define PPAPI_C_DEV_PPB_AUDIO_OUTPUT_DEV_H_

#include "ppapi/c/dev/ppb_device_ref_dev.h"
#include "ppapi/c/pp_array_output.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_time.h"

#define PPB_AUDIO_OUTPUT_DEV_INTERFACE_0_1 "PPB_AudioOutput(Dev);0.1"
#define PPB_AUDIO_OUTPUT_DEV_INTERFACE PPB_AUDIO_OUTPUT_DEV_INTERFACE_0_1

/**
 * @file
 * This file defines the <code>PPB_AudioOutput_dev</code> interface, which
 * provides realtime stereo audio streaming capabilities.
 */


/**
 * @addtogroup Typedefs
 * @{
 */
/**
 * <code>PPB_AudioOutput_Callback</code> defines the type of an audio callback
 * function used to fill the audio buffer with data. Please see the
 * Create() function in the <code>PPB_AudioOutput</code> interface for
 * more details on this callback.
 *
 * @param[out] sample_buffer A buffer to fill with audio data.
 * @param[in] buffer_size_in_bytes The size of the buffer in bytes.
 * @param[in] latency How long before the audio data is to be presented.
 * @param[inout] user_data An opaque pointer that was passed into
 * <code>PPB_AudioOutput.Create()</code>.
 */
typedef void (*PPB_AudioOutput_Callback)(void* sample_buffer,
                                         uint32_t buffer_size_in_bytes,
                                         PP_TimeDelta latency,
                                         void* user_data);
/**
 * @}
 */

/**
 * @addtogroup Interfaces
 * @{
 */
/**
 * The <code>PPB_AudioOutput</code> interface contains pointers to several
 * functions for handling audio resources.
 * Please see descriptions for each <code>PPB_AudioOutput</code> and
 * <code>PPB_AudioConfig</code> function for more details. A C example using
 * <code>PPB_AudioOutput</code> and <code>PPB_AudioConfig</code> follows.
 *
 * <strong>Example: </strong>
 *
 * @code
 * void audio_output_callback(void* sample_buffer,
 *                            uint32_t buffer_size_in_bytes,
 *                            PP_TimeDelta latency,
 *                            void* user_data) {
 *   ... quickly fill in the buffer with samples and return to caller ...
 *  }
 *
 * ...Assume the application has cached the audio configuration interface in
 * audio_config_interface and the audio interface in
 * audio_output_interface...
 *
 * uint32_t count = audio_config_interface->RecommendSampleFrameCount(
 *     PP_AUDIOSAMPLERATE_44100, 4096);
 * PP_Resource pp_audio_config = audio_config_interface->CreateStereo16Bit(
 *     pp_instance, PP_AUDIOSAMPLERATE_44100, count);
 * PP_Resource pp_audio_output = audio_interface->Create(pp_instance,
 *     pp_audio_config, audio_callback, NULL);
 * audio_interface->EnumerateDevices(pp_audio_output, output_device_list,
 *     callback);
 * audio_interface->Open(pp_audio_output, device_ref, pp_audio_config,
 *     audio_output_callback, user_data, callback);
 * audio_output_interface->StartPlayback(pp_audio_output);
 *
 * ...audio_output_callback() will now be periodically invoked on a separate
 * thread...
 * @endcode
 */
struct PPB_AudioOutput_Dev_0_1 {
  /**
   * Creates an audio output resource.
   *
   * @param[in] instance A <code>PP_Instance</code> identifying one instance of
   * a module.
   *
   * @return A <code>PP_Resource</code> corresponding to an audio output
    resource
   * if successful, 0 if failed.
   */
  PP_Resource (*Create)(PP_Instance instance);
  /**
   * Determines if the given resource is an audio output resource.
   *
   * @param[in] resource A <code>PP_Resource</code> containing a resource.
   *
   * @return A <code>PP_Bool</code> containing <code>PP_TRUE</code> if the given
   * resource is an audio output resource, otherwise <code>PP_FALSE</code>.
   */
  PP_Bool (*IsAudioOutput)(PP_Resource resource);
  /**
   * Enumerates audio output devices.
   *
   * @param[in] audio_output A <code>PP_Resource</code> corresponding to an
    audio
   * output resource.
   * @param[in] output An output array which will receive
   * <code>PPB_DeviceRef_Dev</code> resources on success. Please note that the
   * ref count of those resources has already been increased by 1 for the
   * caller.
   * @param[in] callback A <code>PP_CompletionCallback</code> to run on
   * completion.
   *
   * @return An error code from <code>pp_errors.h</code>.
   */
  int32_t (*EnumerateDevices)(PP_Resource audio_output,
                              struct PP_ArrayOutput output,
                              struct PP_CompletionCallback callback);
  /**
   * Requests device change notifications.
   *
   * @param[in] audio_output A <code>PP_Resource</code> corresponding to an
    audio
   * output resource.
   * @param[in] callback The callback to receive notifications. If not NULL, it
   * will be called once for the currently available devices, and then every
   * time the list of available devices changes. All calls will happen on the
   * same thread as the one on which MonitorDeviceChange() is called. It will
   * receive notifications until <code>audio_output</code> is destroyed or
   * <code>MonitorDeviceChange()</code> is called to set a new callback for
   * <code>audio_output</code>. You can pass NULL to cancel sending
   * notifications.
   * @param[inout] user_data An opaque pointer that will be passed to
   * <code>callback</code>.
   *
   * @return An error code from <code>pp_errors.h</code>.
   */
  int32_t (*MonitorDeviceChange)(PP_Resource audio_output,
                                 PP_MonitorDeviceChangeCallback callback,
                                 void* user_data);
  /**
   * Open() opens an audio output device. No sound will be heard until
   * StartPlayback() is called. The callback is called with the buffer address
   * and given user data whenever the buffer needs to be filled. From within the
   * callback, you should not call <code>PPB_AudioOutput</code> functions. The
   * callback will be called on a different thread than the one which created
   * the interface. For performance-critical applications (i.e. low-latency
   * audio), the callback should avoid blocking or calling functions that can
   * obtain locks, such as malloc. The layout and the size of the buffer passed
   * to the audio callback will be determined by the device configuration and is
   * specified in the <code>AudioConfig</code> documentation.
   *
   * @param[in] audio_output A <code>PP_Resource</code> corresponding to an
    audio
   * output resource.
   * @param[in] device_ref Identifies an audio output device. It could be one of
   * the resource in the array returned by EnumerateDevices(), or 0 which means
   * the default device.
   * @param[in] config A <code>PPB_AudioConfig</code> audio configuration
   * resource.
   * @param[in] audio_output_callback A <code>PPB_AudioOutput_Callback</code>
   * function that will be called when audio buffer needs to be filled.
   * @param[inout] user_data An opaque pointer that will be passed into
   * <code>audio_output_callback</code>.
   * @param[in] callback A <code>PP_CompletionCallback</code> to run when this
   * open operation is completed.
   *
   * @return An error code from <code>pp_errors.h</code>.
   */
  int32_t (*Open)(PP_Resource audio_output,
                  PP_Resource device_ref,
                  PP_Resource config,
                  PPB_AudioOutput_Callback audio_output_callback,
                  void* user_data,
                  struct PP_CompletionCallback callback);
  /**
   * GetCurrrentConfig() returns an audio config resource for the given audio
   * output resource.
   *
   * @param[in] config A <code>PP_Resource</code> corresponding to an audio
   * output resource.
   *
   * @return A <code>PP_Resource</code> containing the audio config resource if
   * successful.
   */
  PP_Resource (*GetCurrentConfig)(PP_Resource audio_output);
  /**
   * StartPlayback() starts the playback of the audio output resource and begins
   * periodically calling the callback.
   *
   * @param[in] config A <code>PP_Resource</code> corresponding to an audio
   * output resource.
   *
   * @return A <code>PP_Bool</code> containing <code>PP_TRUE</code> if
   * successful, otherwise <code>PP_FALSE</code>. Also returns
   * <code>PP_TRUE</code> (and be a no-op) if called while playback is already
   * in progress.
   */
  PP_Bool (*StartPlayback)(PP_Resource audio_output);
  /**
   * StopPlayback() stops the playback of the audio resource.
   *
   * @param[in] config A <code>PP_Resource</code> corresponding to an audio
   * output resource.
   *
   * @return A <code>PP_Bool</code> containing <code>PP_TRUE</code> if
   * successful, otherwise <code>PP_FALSE</code>. Also returns
   * <code>PP_TRUE</code> (and is a no-op) if called while playback is already
   * stopped. If a callback is in progress, StopPlayback() will block until the
   * callback completes.
   */
  PP_Bool (*StopPlayback)(PP_Resource audio_output);
  /**
   * Close() closes the audio output device,
           and stops playback if necessary. It is
   * not valid to call Open() again after a call to this method.
   * If an audio output resource is destroyed while a device is still open, then
   * it will be implicitly closed, so you are not required to call this method.
   *
   * @param[in] audio_output A <code>PP_Resource</code> corresponding to an
    audio
   * output resource.
   */
  void (*Close)(PP_Resource audio_output);
};

typedef struct PPB_AudioOutput_Dev_0_1 PPB_AudioOutput_Dev;
/**
 * @}
 */

#endif  /* PPAPI_C_DEV_PPB_AUDIO_OUTPUT_DEV_H_ */

