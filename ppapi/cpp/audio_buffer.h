// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_AUDIO_BUFFER_H_
#define PPAPI_CPP_AUDIO_BUFFER_H_

#include <stdint.h>

#include "ppapi/c/ppb_audio_buffer.h"
#include "ppapi/cpp/resource.h"

namespace pp {

class AudioBuffer : public Resource {
 public:
  /// Default constructor for creating an is_null()
  /// <code>AudioBuffer</code> object.
  AudioBuffer();

  /// The copy constructor for <code>AudioBuffer</code>.
  ///
  /// @param[in] other A reference to an <code>AudioBuffer</code>.
  AudioBuffer(const AudioBuffer& other);

  /// Constructs an <code>AudioBuffer</code> from a <code>Resource</code>.
  ///
  /// @param[in] resource A <code>PPB_AudioBuffer</code> resource.
  explicit AudioBuffer(const Resource& resource);

  /// A constructor used when you have received a <code>PP_Resource</code> as a
  /// return value that has had 1 ref added for you.
  ///
  /// @param[in] resource A <code>PPB_AudioBuffer</code> resource.
  AudioBuffer(PassRef, PP_Resource resource);

  virtual ~AudioBuffer();

  /// Gets the timestamp of the audio buffer.
  ///
  /// @return A <code>PP_TimeDelta</code> containing the timestamp of the audio
  /// buffer. Given in seconds since the start of the containing audio stream.
  PP_TimeDelta GetTimestamp() const;

  /// Sets the timestamp of the audio buffer.
  ///
  /// @param[in] timestamp A <code>PP_TimeDelta</code> containing the timestamp
  /// of the audio buffer. Given in seconds since the start of the containing
  /// audio stream.
  void SetTimestamp(PP_TimeDelta timestamp);

  /// Gets the sample rate of the audio buffer.
  ///
  /// @return The sample rate of the audio buffer.
  PP_AudioBuffer_SampleRate GetSampleRate() const;

  /// Gets the sample size of the audio buffer in bytes.
  ///
  /// @return The sample size of the audio buffer in bytes.
  PP_AudioBuffer_SampleSize GetSampleSize() const;

  /// Gets the number of channels in the audio buffer.
  ///
  /// @return The number of channels in the audio buffer.
  uint32_t GetNumberOfChannels() const;

  /// Gets the number of samples in the audio buffer.
  ///
  /// @return The number of samples in the audio buffer.
  /// For example, at a sampling rate of 44,100 Hz in stereo audio, a buffer
  /// containing 4,410 * 2 samples would have a duration of 100 milliseconds.
  uint32_t GetNumberOfSamples() const;

  /// Gets the data buffer containing the audio buffer samples.
  ///
  /// @return A pointer to the beginning of the data buffer.
  void* GetDataBuffer();

  /// Gets the size of data buffer in bytes.
  ///
  /// @return The size of the data buffer in bytes.
  uint32_t GetDataBufferSize() const;
};

}  // namespace pp

#endif  // PPAPI_CPP_AUDIO_BUFFER_H_
