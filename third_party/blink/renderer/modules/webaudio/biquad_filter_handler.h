// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_BIQUAD_FILTER_HANDLER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_BIQUAD_FILTER_HANDLER_H_

#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_biquad_filter_type.h"
#include "third_party/blink/renderer/modules/webaudio/audio_handler.h"

namespace blink {

class AudioNode;
class AudioParamHandler;
class Biquad;

class BiquadFilterHandler final : public AudioHandler {
 public:
  static scoped_refptr<BiquadFilterHandler> Create(AudioNode&,
                                                   float sample_rate,
                                                   AudioParamHandler& frequency,
                                                   AudioParamHandler& q,
                                                   AudioParamHandler& gain,
                                                   AudioParamHandler& detune);

  BiquadFilterHandler(const BiquadFilterHandler&) = delete;
  BiquadFilterHandler& operator=(const BiquadFilterHandler&) = delete;

  ~BiquadFilterHandler() override;

  // AudioHandler
  void Process(uint32_t frames_to_process) override;
  void ProcessOnlyAudioParams(uint32_t frames_to_process) override;
  void PullInputs(uint32_t frames_to_process) override;
  void Initialize() override;
  void Uninitialize() override;

  // Called in the main thread when the number of channels for the input may
  // have changed.
  void CheckNumberOfChannelsForInput(AudioNodeInput*) override;

  // Get the magnitude and phase response of the filter at the given
  // set of frequencies (in Hz). The phase response is in radians.
  void GetFrequencyResponse(base::span<const float> frequency_hz,
                            base::span<float> mag_response,
                            base::span<float> phase_response);
  V8BiquadFilterType::Enum Type() const;
  void SetType(V8BiquadFilterType::Enum type);

  // Expose HasConstantValues for unit testing
  MODULES_EXPORT static bool HasConstantValuesForTesting(
      base::span<float> values);

 private:
  BiquadFilterHandler(AudioNode&,
                      float sample_rate,
                      AudioParamHandler& frequency,
                      AudioParamHandler& q,
                      AudioParamHandler& gain,
                      AudioParamHandler& detune);

  void NotifyBadState() const;

  bool RequiresTailProcessing() const override;
  double TailTime() const override;
  double LatencyTime() const override;

  // Only notify the user once.  No need to spam the console with messages,
  // because once we're in a bad state, it usually stays that way forever.  Only
  // accessed from audio thread.
  bool did_warn_bad_filter_state_ = false;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  V8BiquadFilterType::Enum type_ = V8BiquadFilterType::Enum::kLowpass;

  scoped_refptr<AudioParamHandler> parameter_cutoff_frequency_;
  scoped_refptr<AudioParamHandler> parameter_q_;
  scoped_refptr<AudioParamHandler> parameter_gain_;
  scoped_refptr<AudioParamHandler> parameter_detune_;

  bool has_just_reset_ GUARDED_BY(process_lock_) = true;

  // Cache previous parameter values to allow us to skip recomputing filter
  // coefficients when parameters are not changing
  float previous_parameter_cutoff_frequency_ =
      std::numeric_limits<float>::quiet_NaN();
  float previous_parameter_q_ = std::numeric_limits<float>::quiet_NaN();
  float previous_parameter_gain_ = std::numeric_limits<float>::quiet_NaN();
  float previous_parameter_detune_ = std::numeric_limits<float>::quiet_NaN();

  const double sample_rate_;
  const double nyquist_;
  const unsigned render_quantum_frames_;

  double tail_time_ GUARDED_BY(process_lock_) =
      std::numeric_limits<double>::infinity();

  Vector<std::unique_ptr<Biquad>> biquads_ GUARDED_BY(process_lock_);

  // Synchronize process() with getting and setting the filter coefficients.
  mutable base::Lock process_lock_;

  base::WeakPtrFactory<BiquadFilterHandler> weak_ptr_factory_{this};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_BIQUAD_FILTER_HANDLER_H_
