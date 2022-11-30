/* Copyright 2012 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From dev/ppb_audio_input_dev.idl modified Thu Dec 12 15:35:39 2013. */

#ifndef PPAPI_C_DEV_PPB_AUDIO_INPUT_DEV_H_
#define PPAPI_C_DEV_PPB_AUDIO_INPUT_DEV_H_

#include "ppapi/c/dev/ppb_device_ref_dev.h"
#include "ppapi/c/pp_array_output.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_time.h"

#define PPB_AUDIO_INPUT_DEV_INTERFACE_0_3 "PPB_AudioInput(Dev);0.3"
#define PPB_AUDIO_INPUT_DEV_INTERFACE_0_4 "PPB_AudioInput(Dev);0.4"
#define PPB_AUDIO_INPUT_DEV_INTERFACE PPB_AUDIO_INPUT_DEV_INTERFACE_0_4

/**
 * @file
 * This file defines the <code>PPB_AudioInput_Dev</code> interface, which
 * provides realtime audio input capture.
 */


/**
 * @addtogroup Typedefs
 * @{
 */
/**
 * <code>PPB_AudioInput_Callback</code> defines the type of an audio callback
 * function used to provide the audio buffer with data. This callback will be
 * called on a separate thread from the creation thread.
 *
 * @param[in] sample_buffer A buffer providing audio input data.
 * @param[in] buffer_size_in_bytes The size of the buffer in bytes.
 * @param[in] latency The time that has elapsed since the data was recorded.
 * @param[inout] user_data An opaque pointer that was passed into
 * <code>PPB_AudioInput_Dev.Open()</code>.
 */
typedef void (*PPB_AudioInput_Callback)(const void* sample_buffer,
                                        uint32_t buffer_size_in_bytes,
                                        PP_TimeDelta latency,
                                        void* user_data);

typedef void (*PPB_AudioInput_Callback_0_3)(const void* sample_buffer,
                                            uint32_t buffer_size_in_bytes,
                                            void* user_data);
/**
 * @}
 */

/**
 * @addtogroup Interfaces
 * @{
 */
/**
 * The <code>PPB_AudioInput_Dev</code> interface contains pointers to several
 * functions for handling audio input resources.
 *
 * TODO(brettw) before moving out of dev, we need to resolve the issue of
 * the mismatch between the current audio config interface and this one.
 *
 * In particular, the params for input assume stereo, but this class takes
 * everything as mono. We either need to not use an audio config resource, or
 * add mono support.
 *
 * In addition, RecommendSampleFrameCount is completely wrong for audio input.
 * RecommendSampleFrameCount returns the frame count for the current
 * low-latency output device, which is likely inappropriate for a random input
 * device. We may want to move the "recommend" functions to the input or output
 * classes rather than the config.
 */
struct PPB_AudioInput_Dev_0_4 {
  /**
   * Creates an audio input resource.
   *
   * @param[in] instance A <code>PP_Instance</code> identifying one instance of
   * a module.
   *
   * @return A <code>PP_Resource</code> corresponding to an audio input resource
   * if successful, 0 if failed.
   */
  PP_Resource (*Create)(PP_Instance instance);
  /**
   * Determines if the given resource is an audio input resource.
   *
   * @param[in] resource A <code>PP_Resource</code> containing a resource.
   *
   * @return A <code>PP_Bool</code> containing <code>PP_TRUE</code> if the given
   * resource is an audio input resource, otherwise <code>PP_FALSE</code>.
   */
  PP_Bool (*IsAudioInput)(PP_Resource resource);
  /**
   * Enumerates audio input devices.
   *
   * @param[in] audio_input A <code>PP_Resource</code> corresponding to an audio
   * input resource.
   * @param[in] output An output array which will receive
   * <code>PPB_DeviceRef_Dev</code> resources on success. Please note that the
   * ref count of those resources has already been increased by 1 for the
   * caller.
   * @param[in] callback A <code>PP_CompletionCallback</code> to run on
   * completion.
   *
   * @return An error code from <code>pp_errors.h</code>.
   */
  int32_t (*EnumerateDevices)(PP_Resource audio_input,
                              struct PP_ArrayOutput output,
                              struct PP_CompletionCallback callback);
  /**
   * Requests device change notifications.
   *
   * @param[in] audio_input A <code>PP_Resource</code> corresponding to an audio
   * input resource.
   * @param[in] callback The callback to receive notifications. If not NULL, it
   * will be called once for the currently available devices, and then every
   * time the list of available devices changes. All calls will happen on the
   * same thread as the one on which MonitorDeviceChange() is called. It will
   * receive notifications until <code>audio_input</code> is destroyed or
   * <code>MonitorDeviceChange()</code> is called to set a new callback for
   * <code>audio_input</code>. You can pass NULL to cancel sending
   * notifications.
   * @param[inout] user_data An opaque pointer that will be passed to
   * <code>callback</code>.
   *
   * @return An error code from <code>pp_errors.h</code>.
   */
  int32_t (*MonitorDeviceChange)(PP_Resource audio_input,
                                 PP_MonitorDeviceChangeCallback callback,
                                 void* user_data);
  /**
   * Opens an audio input device. No sound will be captured until
   * StartCapture() is called.
   *
   * @param[in] audio_input A <code>PP_Resource</code> corresponding to an audio
   * input resource.
   * @param[in] device_ref Identifies an audio input device. It could be one of
   * the resource in the array returned by EnumerateDevices(), or 0 which means
   * the default device.
   * @param[in] config A <code>PPB_AudioConfig</code> audio configuration
   * resource.
   * @param[in] audio_input_callback A <code>PPB_AudioInput_Callback</code>
   * function that will be called when data is available.
   * @param[inout] user_data An opaque pointer that will be passed into
   * <code>audio_input_callback</code>.
   * @param[in] callback A <code>PP_CompletionCallback</code> to run when this
   * open operation is completed.
   *
   * @return An error code from <code>pp_errors.h</code>.
   */
  int32_t (*Open)(PP_Resource audio_input,
                  PP_Resource device_ref,
                  PP_Resource config,
                  PPB_AudioInput_Callback audio_input_callback,
                  void* user_data,
                  struct PP_CompletionCallback callback);
  /**
   * Returns an audio config resource for the given audio input resource.
   *
   * @param[in] audio_input A <code>PP_Resource</code> corresponding to an audio
   * input resource.
   *
   * @return A <code>PP_Resource</code> containing the audio config resource if
   * successful.
   */
  PP_Resource (*GetCurrentConfig)(PP_Resource audio_input);
  /**
   * Starts the capture of the audio input resource and begins periodically
   * calling the callback.
   *
   * @param[in] audio_input A <code>PP_Resource</code> corresponding to an audio
   * input resource.
   *
   * @return A <code>PP_Bool</code> containing <code>PP_TRUE</code> if
   * successful, otherwise <code>PP_FALSE</code>.
   * Also returns <code>PP_TRUE</code> (and is a no-op) if called while capture
   * is already started.
   */
  PP_Bool (*StartCapture)(PP_Resource audio_input);
  /**
   * Stops the capture of the audio input resource.
   *
   * @param[in] audio_input A PP_Resource containing the audio input resource.
   *
   * @return A <code>PP_Bool</code> containing <code>PP_TRUE</code> if
   * successful, otherwise <code>PP_FALSE</code>.
   * Also returns <code>PP_TRUE</code> (and is a no-op) if called while capture
   * is already stopped. If a buffer is being captured, StopCapture will block
   * until the call completes.
   */
  PP_Bool (*StopCapture)(PP_Resource audio_input);
  /**
   * Closes the audio input device, and stops capturing if necessary. It is
   * not valid to call Open() again after a call to this method.
   * If an audio input resource is destroyed while a device is still open, then
   * it will be implicitly closed, so you are not required to call this method.
   *
   * @param[in] audio_input A <code>PP_Resource</code> corresponding to an audio
   * input resource.
   */
  void (*Close)(PP_Resource audio_input);
};

typedef struct PPB_AudioInput_Dev_0_4 PPB_AudioInput_Dev;

struct PPB_AudioInput_Dev_0_3 {
  PP_Resource (*Create)(PP_Instance instance);
  PP_Bool (*IsAudioInput)(PP_Resource resource);
  int32_t (*EnumerateDevices)(PP_Resource audio_input,
                              struct PP_ArrayOutput output,
                              struct PP_CompletionCallback callback);
  int32_t (*MonitorDeviceChange)(PP_Resource audio_input,
                                 PP_MonitorDeviceChangeCallback callback,
                                 void* user_data);
  int32_t (*Open)(PP_Resource audio_input,
                  PP_Resource device_ref,
                  PP_Resource config,
                  PPB_AudioInput_Callback_0_3 audio_input_callback,
                  void* user_data,
                  struct PP_CompletionCallback callback);
  PP_Resource (*GetCurrentConfig)(PP_Resource audio_input);
  PP_Bool (*StartCapture)(PP_Resource audio_input);
  PP_Bool (*StopCapture)(PP_Resource audio_input);
  void (*Close)(PP_Resource audio_input);
};
/**
 * @}
 */

#endif  /* PPAPI_C_DEV_PPB_AUDIO_INPUT_DEV_H_ */

