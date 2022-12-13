// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_AUDIO_RENDERER_MIXER_H_
#define MEDIA_BASE_AUDIO_RENDERER_MIXER_H_

#include <stdint.h>

#include <map>
#include <memory>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "media/base/audio_converter.h"
#include "media/base/audio_renderer_sink.h"
#include "media/base/loopback_audio_converter.h"

namespace media {
class AudioRendererMixerInput;

// Mixes a set of AudioConverter::InputCallbacks into a single output stream
// which is funneled into a single shared AudioRendererSink; saving a bundle
// on renderer side resources.
class MEDIA_EXPORT AudioRendererMixer
    : public AudioRendererSink::RenderCallback {
 public:
  AudioRendererMixer(const AudioParameters& output_params,
                     scoped_refptr<AudioRendererSink> sink);

  AudioRendererMixer(const AudioRendererMixer&) = delete;
  AudioRendererMixer& operator=(const AudioRendererMixer&) = delete;

  ~AudioRendererMixer() override;

  // Add or remove a mixer input from mixing; called by AudioRendererMixerInput.
  void AddMixerInput(const AudioParameters& input_params,
                     AudioConverter::InputCallback* input);
  void RemoveMixerInput(const AudioParameters& input_params,
                        AudioConverter::InputCallback* input);

  // Since errors may occur even when no inputs are playing, an error callback
  // must be registered separately from adding a mixer input.
  void AddErrorCallback(AudioRendererMixerInput* input);
  void RemoveErrorCallback(AudioRendererMixerInput* input);

  // Returns true if called on rendering thread, otherwise false.
  bool CurrentThreadIsRenderingThread();

  void SetPauseDelayForTesting(base::TimeDelta delay);
  const AudioParameters& get_output_params_for_testing() const {
    return output_params_;
  }

 private:
  // AudioRendererSink::RenderCallback implementation.
  int Render(base::TimeDelta delay,
             base::TimeTicks delay_timestamp,
             const AudioGlitchInfo& glitch_info,
             AudioBus* audio_bus) override;
  void OnRenderError() override;

  bool can_passthrough(int sample_rate) const {
    return sample_rate == output_params_.sample_rate();
  }

  // Output parameters for this mixer.
  const AudioParameters output_params_;

  // Output sink for this mixer.
  const scoped_refptr<AudioRendererSink> audio_sink_;

  // ---------------[ All variables below protected by |lock_| ]---------------
  base::Lock lock_;

  // List of error callbacks used by this mixer.
  base::flat_set<AudioRendererMixerInput*> error_callbacks_ GUARDED_BY(lock_);

  // Maps input sample rate to the dedicated converter.
  using AudioConvertersMap =
      base::flat_map<int, std::unique_ptr<LoopbackAudioConverter>>;

  // Each of these converters mixes inputs with a given sample rate and
  // resamples them to the output sample rate. Inputs not requiring resampling
  // go directly to |aggregate_converter_|.
  AudioConvertersMap converters_ GUARDED_BY(lock_);

  // Aggregate converter which mixes all the outputs from |converters_| as well
  // as mixer inputs that are in the output sample rate.
  AudioConverter aggregate_converter_ GUARDED_BY(lock_);

  // Handles physical stream pause when no inputs are playing.  For latency
  // reasons we don't want to immediately pause the physical stream.
  base::TimeDelta pause_delay_ GUARDED_BY(lock_);
  base::TimeTicks last_play_time_ GUARDED_BY(lock_);
  bool playing_ GUARDED_BY(lock_);
};

}  // namespace media

#endif  // MEDIA_BASE_AUDIO_RENDERER_MIXER_H_
