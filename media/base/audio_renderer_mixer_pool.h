// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_AUDIO_RENDERER_MIXER_POOL_H_
#define MEDIA_BASE_AUDIO_RENDERER_MIXER_POOL_H_

#include <string>

#include "media/base/audio_latency.h"
#include "media/base/output_device_info.h"

namespace base {
class UnguessableToken;
}  // namespace base

namespace media {
class AudioParameters;
class AudioRendererMixer;
class AudioRendererSink;

// Provides AudioRendererMixer instances for shared usage.
// Thread safe.
class MEDIA_EXPORT AudioRendererMixerPool {
 public:
  AudioRendererMixerPool() = default;

  AudioRendererMixerPool(const AudioRendererMixerPool&) = delete;
  AudioRendererMixerPool& operator=(const AudioRendererMixerPool&) = delete;

  virtual ~AudioRendererMixerPool() = default;

  // Obtains a pointer to mixer instance based on AudioParameters. The pointer
  // is guaranteed to be valid (at least) until it's rereleased by a call to
  // ReturnMixer().
  //
  // Ownership of |sink| must be passed to GetMixer(), it will be stopped and
  // discard if an existing mixer can be reused. Clients must have called
  // GetOutputDeviceInfoAsync() on |sink| to get |sink_info|, and it must have
  // a device_status() == OUTPUT_DEVICE_STATUS_OK.
  virtual AudioRendererMixer* GetMixer(
      const base::UnguessableToken& owner_token,
      const AudioParameters& input_params,
      AudioLatency::Type latency,
      const OutputDeviceInfo& sink_info,
      scoped_refptr<AudioRendererSink> sink) = 0;

  // Returns mixer back to the pool, must be called when the mixer is not needed
  // any more to avoid memory leakage.
  virtual void ReturnMixer(AudioRendererMixer* mixer) = 0;

  // Returns an AudioRendererSink for use with GetMixer(). Inputs must call this
  // to get a sink to use with a subsequent GetMixer()
  virtual scoped_refptr<AudioRendererSink> GetSink(
      const base::UnguessableToken& owner_token,
      const std::string& device_id) = 0;
};

}  // namespace media

#endif  // MEDIA_BASE_AUDIO_RENDERER_MIXER_POOL_H_
