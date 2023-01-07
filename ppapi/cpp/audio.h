// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_AUDIO_H_
#define PPAPI_CPP_AUDIO_H_

#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/ppb_audio.h"
#include "ppapi/cpp/audio_config.h"
#include "ppapi/cpp/resource.h"

/// @file
/// This file defines the API to create realtime stereo audio streaming
/// capabilities.

namespace pp {

class InstanceHandle;

/// An audio resource. Refer to the
/// <a href="/native-client/devguide/coding/audio.html">Audio</a>
/// chapter in the Developer's Guide for information on using this interface.
class Audio : public Resource {
 public:

  /// An empty constructor for an Audio resource.
  Audio() {}

  /// A constructor that creates an Audio resource. No sound will be heard
  /// until StartPlayback() is called. The callback is called with the buffer
  /// address and given user data whenever the buffer needs to be filled.
  /// From within the callback, you should not call <code>PPB_Audio</code>
  /// functions. The callback will be called on a different thread than the one
  /// which created the interface. For performance-critical applications (such
  /// as low-latency audio), the callback should avoid blocking or calling
  /// functions that can obtain locks, such as malloc. The layout and the size
  /// of the buffer passed to the audio callback will be determined by
  /// the device configuration and is specified in the <code>AudioConfig</code>
  /// documentation.
  ///
  /// @param[in] instance The instance with which this resource will be
  /// associated.
  /// @param[in] config An <code>AudioConfig</code> containing the audio config
  /// resource.
  /// @param[in] callback A <code>PPB_Audio_Callback</code> callback function
  /// that the browser calls when it needs more samples to play.
  /// @param[in] user_data A pointer to user data used in the callback function.
  Audio(const InstanceHandle& instance,
        const AudioConfig& config,
        PPB_Audio_Callback callback,
        void* user_data);

  /// A constructor that creates an Audio resource.
  ///
  /// @param[in] instance The instance with which this resource will be
  /// associated.
  /// @param[in] config An <code>AudioConfig</code> containing the audio config
  /// resource.
  /// @param[in] callback A <code>PPB_Audio_Callback_1_0</code> callback
  /// function that the browser calls when it needs more samples to play.
  /// @param[in] user_data A pointer to user data used in the callback function.
  Audio(const InstanceHandle& instance,
        const AudioConfig& config,
        PPB_Audio_Callback_1_0 callback,
        void* user_data);

  /// Getter function for returning the internal <code>PPB_AudioConfig</code>
  /// struct.
  ///
  /// @return A mutable reference to the PPB_AudioConfig struct.
  AudioConfig& config() { return config_; }

  /// Getter function for returning the internal <code>PPB_AudioConfig</code>
  /// struct.
  ///
  /// @return A const reference to the internal <code>PPB_AudioConfig</code>
  /// struct.
  const AudioConfig& config() const { return config_; }

  /// StartPlayback() starts playback of audio.
  ///
  /// @return true if successful, otherwise false.
  bool StartPlayback();

  /// StopPlayback stops playback of audio.
  ///
  /// @return true if successful, otherwise false.
  bool StopPlayback();

 private:
  AudioConfig config_;
  bool use_1_0_interface_;
};

}  // namespace pp

#endif  // PPAPI_CPP_AUDIO_H_

