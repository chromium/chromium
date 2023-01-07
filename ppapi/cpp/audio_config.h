// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_AUDIO_CONFIG_H_
#define PPAPI_CPP_AUDIO_CONFIG_H_

#include "ppapi/c/ppb_audio_config.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/cpp/resource.h"

/// @file
/// This file defines the interface for establishing an
/// audio configuration resource within the browser.

namespace pp {

class InstanceHandle;

/// A 16 bit stereo AudioConfig resource. Refer to the
/// <a href="/native-client/devguide/coding/audio.html">Audio
/// </a>chapter in the Developer's Guide for information on using this
/// interface.
///
/// A single sample frame on a stereo device means one value for the left
/// channel and one value for the right channel.
///
/// Buffer layout for a stereo int16 configuration:
///
/// <code>int16_t *buffer16;</code>
/// <code>buffer16[0]</code> is the first left channel sample.
/// <code>buffer16[1]</code> is the first right channel sample.
/// <code>buffer16[2]</code> is the second left channel sample.
/// <code>buffer16[3]</code> is the second right channel sample.
/// <code>...</code>
/// <code>buffer16[2 * (sample_frame_count - 1)]</code> is the last left
/// channel sample.
/// <code>buffer16[2 * (sample_frame_count - 1) + 1]</code> is the last right
/// channel sample.
/// Data will always be in the native endian format of the platform.
///
/// <strong>Example:</strong>
/// @code
///
/// // Create an audio config with a supported frame count.
/// uint32_t sample_frame_count = AudioConfig::RecommendSampleFrameCount(
///    PP_AUDIOSAMPLERATE_44100, 4096);
///  AudioConfig config(PP_AUDIOSAMPLERATE_44100, sample_frame_count);
///  if (config.is_null())
///    return false;  // Couldn't configure audio.
///
///   // Then use the config to create your audio resource.
///  Audio audio(instance, config, callback, user_data);
///   if (audio.is_null())
///     return false;  // Couldn't create audio.
/// @endcode
class AudioConfig : public Resource {
 public:
  /// An empty constructor for an <code>AudioConfig</code> resource.
  AudioConfig();

  /// A constructor that creates an audio config based on the given sample rate
  /// and frame count. If the rate and frame count aren't supported, the
  /// resulting resource will be is_null(). You can pass the result of
  /// RecommendSampleFrameCount() as the sample frame count.
  ///
  /// @param[in] instance The instance associated with this resource.
  ///
  /// @param[in] sample_rate A <code>PP_AudioSampleRate</code> which is either
  /// <code>PP_AUDIOSAMPLERATE_44100</code> or
  /// <code>PP_AUDIOSAMPLERATE_48000</code>.
  ///
  /// @param[in] sample_frame_count A uint32_t frame count returned from the
  /// <code>RecommendSampleFrameCount</code> function.
  AudioConfig(const InstanceHandle& instance,
              PP_AudioSampleRate sample_rate,
              uint32_t sample_frame_count);

  /// RecommendSampleRate() returns the native sample rate used by the
  /// audio system.  Applications that use the recommended sample rate might
  /// obtain lower latency and higher fidelity output.
  ///
  /// @param[in] instance The instance associated with this resource.
  static PP_AudioSampleRate RecommendSampleRate(
      const InstanceHandle& instance);

  /// RecommendSampleFrameCount() returns a supported frame count closest to
  /// the requested count. The sample frame count determines the overall
  /// latency of audio. Smaller frame counts will yield lower latency, but
  /// higher CPU utilization. Supported sample frame counts will vary by
  /// hardware and system (consider that the local system might be anywhere
  /// from a cell phone or a high-end audio workstation). Sample counts less
  /// than <code>PP_AUDIOMINSAMPLEFRAMECOUNT</code> and greater than
  /// <code>PP_AUDIOMAXSAMPLEFRAMECOUNT</code> are never supported on any
  /// system, but values in between aren't necessarily valid. This function
  /// will return a supported count closest to the requested value for use in
  /// the constructor.
  ///
  /// @param[in] instance The instance associated with this resource.
  /// @param[in] sample_rate A <code>PP_AudioSampleRate</code> which is either
  /// <code>PP_AUDIOSAMPLERATE_44100</code> or
  /// <code>PP_AUDIOSAMPLERATE_48000</code>.
  /// @param[in] requested_sample_frame_count A uint32_t requested frame count.
  ///
  /// @return A uint32_t containing the recommended sample frame count if
  /// successful. If the sample frame count or bit rate is not supported,
  /// this function will fail and return 0.
  static uint32_t RecommendSampleFrameCount(
      const InstanceHandle& instance,
      PP_AudioSampleRate sample_rate,
      uint32_t requested_sample_frame_count);

  /// Getter function for returning the internal
  /// <code>PP_AudioSampleRate</code> enum.
  ///
  /// @return The <code>PP_AudioSampleRate</code> enum.
  PP_AudioSampleRate sample_rate() const { return sample_rate_; }

  /// Getter function for returning the internal sample frame count.
  ///
  /// @return A uint32_t containing the sample frame count.
  uint32_t sample_frame_count() const { return sample_frame_count_; }

 private:
  PP_AudioSampleRate sample_rate_;
  uint32_t sample_frame_count_;
};

}  // namespace pp

#endif  // PPAPI_CPP_AUDIO_CONFIG_H_

