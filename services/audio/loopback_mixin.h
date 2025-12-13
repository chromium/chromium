// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_LOOPBACK_MIXIN_H_
#define SERVICES_AUDIO_LOOPBACK_MIXIN_H_

#include <memory>
#include <string_view>

#include "base/feature_list.h"
#include "base/time/time.h"
#include "media/base/audio_bus.h"

namespace base {
class UnguessableToken;
}

namespace media {
struct AudioGlitchInfo;
class AudioParameters;
}  // namespace media

namespace audio {
class LoopbackCoordinator;
class LoopbackSignalProviderInterface;

BASE_DECLARE_FEATURE(kRestrictOwnAudioAddChromiumBack);

// Mixes the loopback audio with the primary audio source and
// forwards the result to a pre-configured callback.
class LoopbackMixin {
 public:
  // Callback to deliver the mixed audio data. The signature matches
  // AudioInputStream::AudioInputCallback::OnData.
  using OnDataCallback = base::RepeatingCallback<void(
      const media::AudioBus* source,
      base::TimeTicks capture_time,
      double volume,
      const media::AudioGlitchInfo& audio_glitch_info)>;

  // Helper factory callback.
  using MaybeCreateCallback = base::OnceCallback<std::unique_ptr<LoopbackMixin>(
      std::string_view device_id,
      const media::AudioParameters& params,
      OnDataCallback on_data_callback)>;

  // Creates a mixin if `device_id` is an appropriate system loopback device id.
  // Loopback signal being mixed with the source will include all output streams
  // except those belonging to `group_id` group.
  static std::unique_ptr<LoopbackMixin>
  MaybeCreateRestrictOwnAudioLoopbackMixin(
      LoopbackCoordinator* coordinator,
      const base::UnguessableToken& group_id,
      std::string_view device_id,
      const media::AudioParameters& params,
      OnDataCallback on_data_callback);

  ~LoopbackMixin();

  LoopbackMixin(const LoopbackMixin&) = delete;
  LoopbackMixin& operator=(const LoopbackMixin&) = delete;

  // Starts listening to the loopback signal. Separated from the constructor to
  // allow a better control over loopback delay.
  void Start();

  // Pulls audio from the LoopbackSignalProvider, mixes it with the `source`
  // bus, and passes the result to the `on_data_callback_`.
  void OnData(const media::AudioBus* source,
              base::TimeTicks capture_time,
              double volume,
              const media::AudioGlitchInfo& audio_glitch_info);

 protected:
  // `signal_provider` is the source of the loopback audio.
  // `params` are the audio parameters of both the main source and
  // `signal_provider`. `on_data_callback` is the callback to which the mixed
  // audio will be sent.
  LoopbackMixin(
      std::unique_ptr<LoopbackSignalProviderInterface> signal_provider,
      const media::AudioParameters& params,
      OnDataCallback on_data_callback);

 private:
  // Provides the loopback audio signal.
  const std::unique_ptr<LoopbackSignalProviderInterface> signal_provider_;

  // An intermediate AudioBus to hold the mix.
  const std::unique_ptr<media::AudioBus> mix_bus_;

  // The callback to which the mixed audio is passed.
  OnDataCallback on_data_callback_;
};

}  // namespace audio

#endif  // SERVICES_AUDIO_LOOPBACK_MIXIN_H_
