// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_BIQUAD_FILTER_HANDLER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_BIQUAD_FILTER_HANDLER_H_

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/renderer/modules/webaudio/audio_handler.h"

namespace blink {

class AudioNode;
class AudioParamHandler;
class BiquadDSPKernel;

class BiquadProcessor final {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class FilterType {
    kLowPass = 0,
    kHighPass = 1,
    kBandPass = 2,
    kLowShelf = 3,
    kHighShelf = 4,
    kPeaking = 5,
    kNotch = 6,
    kAllPass = 7,
    kMaxValue = kAllPass,
  };

  BiquadProcessor(float sample_rate,
                  uint32_t number_of_channels,
                  unsigned render_quantum_frames,
                  AudioParamHandler& frequency,
                  AudioParamHandler& q,
                  AudioParamHandler& gain,
                  AudioParamHandler& detune);
  ~BiquadProcessor();

  std::unique_ptr<BiquadDSPKernel> CreateKernel();

  void Initialize();
  void Uninitialize();
  void Process(const AudioBus* source,
               AudioBus* destination,
               uint32_t frames_to_process);
  void ProcessOnlyAudioParams(uint32_t frames_to_process);
  void Reset();

  bool IsInitialized() const { return is_initialized_; }

  float SampleRate() const { return sample_rate_; }

  unsigned RenderQuantumFrames() const { return render_quantum_frames_; }

  double TailTime() const;
  double LatencyTime() const;
  bool RequiresTailProcessing() const;

  void SetNumberOfChannels(unsigned);
  unsigned NumberOfChannels() const { return number_of_channels_; }

  // Get the magnitude and phase response of the filter at the given
  // set of frequencies (in Hz). The phase response is in radians.
  void GetFrequencyResponse(int n_frequencies,
                            const float* frequency_hz,
                            float* mag_response,
                            float* phase_response);

  void CheckForDirtyCoefficients();

  bool AreFilterCoefficientsDirty() const { return are_filter_coefficients_dirty_; }
  bool HasSampleAccurateValues() const { return has_sample_accurate_values_; }
  bool IsAudioRate() const { return is_audio_rate_; }

  AudioParamHandler& Parameter1() { return *parameter1_; }
  AudioParamHandler& Parameter2() { return *parameter2_; }
  AudioParamHandler& Parameter3() { return *parameter3_; }
  AudioParamHandler& Parameter4() { return *parameter4_; }

  FilterType Type() const { return type_; }
  void SetType(FilterType);

 private:
  FilterType type_ = FilterType::kLowPass;

  scoped_refptr<AudioParamHandler> parameter1_;
  scoped_refptr<AudioParamHandler> parameter2_;
  scoped_refptr<AudioParamHandler> parameter3_;
  scoped_refptr<AudioParamHandler> parameter4_;

  // so DSP kernels know when to re-compute coefficients
  bool are_filter_coefficients_dirty_ = true;

  // Set to true if any of the filter parameters are sample-accurate.
  bool has_sample_accurate_values_ = false;

  // Set to true if any of the filter parameters are a-rate.
  bool is_audio_rate_;

  bool has_just_reset_ = true;

  // Cache previous parameter values to allow us to skip recomputing filter
  // coefficients when parameters are not changing
  float previous_parameter1_ = std::numeric_limits<float>::quiet_NaN();
  float previous_parameter2_ = std::numeric_limits<float>::quiet_NaN();
  float previous_parameter3_ = std::numeric_limits<float>::quiet_NaN();
  float previous_parameter4_ = std::numeric_limits<float>::quiet_NaN();

  bool is_initialized_ = false;
  unsigned number_of_channels_;
  float sample_rate_;
  unsigned render_quantum_frames_;

  Vector<std::unique_ptr<BiquadDSPKernel>> kernels_ GUARDED_BY(process_lock_);
  mutable base::Lock process_lock_;
};

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

  ~BiquadFilterHandler() override = default;

  // AudioHandler
  void Process(uint32_t frames_to_process) override;
  void ProcessOnlyAudioParams(uint32_t frames_to_process) override;
  void PullInputs(uint32_t frames_to_process) override;
  void Initialize() override;
  void Uninitialize() override;

  // Called in the main thread when the number of channels for the input may
  // have changed.
  void CheckNumberOfChannelsForInput(AudioNodeInput*) override;

  // Returns the number of channels for both the input and the output.
  unsigned NumberOfChannels();
  BiquadProcessor* Processor() { return processor_.get(); }

  // Expose HasConstantValues for unit testing
  MODULES_EXPORT static bool HasConstantValuesForTesting(float* values,
                                                         int frames_to_process);

 private:
  BiquadFilterHandler(AudioNode&,
                      float sample_rate,
                      AudioParamHandler& frequency,
                      AudioParamHandler& q,
                      AudioParamHandler& gain,
                      AudioParamHandler& detune);

  void NotifyBadState() const;

  // Returns true if the first output sample of any channel is non-finite.  This
  // is a proxy for determining if the filter state is bad.  For
  // BiquadFilterNodes and IIRFilterNodes, if the internal state has non-finite
  // values, the non-finite value propagates pretty much forever in the output.
  // This is because infinities and NaNs are sticky.
  bool HasNonFiniteOutput() const;

  bool RequiresTailProcessing() const override;
  double TailTime() const override;
  double LatencyTime() const override;

  // Only notify the user of the once.  No need to spam the console with
  // messages, because once we're in a bad state, it usually stays that way
  // forever.  Only accessed from audio thread.
  bool did_warn_bad_filter_state_ = false;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  std::unique_ptr<BiquadProcessor> processor_;

  base::WeakPtrFactory<BiquadFilterHandler> weak_ptr_factory_{this};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_BIQUAD_FILTER_HANDLER_H_
