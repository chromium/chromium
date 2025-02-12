// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_IIR_FILTER_HANDLER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_IIR_FILTER_HANDLER_H_

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/renderer/modules/webaudio/audio_basic_processor_handler.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class AudioNode;

class IIRFilterHandler final : public AudioHandler {
 public:
  static scoped_refptr<IIRFilterHandler> Create(
      AudioNode&,
      float sample_rate,
      const Vector<double>& feedforward_coef,
      const Vector<double>& feedback_coef,
      bool is_filter_stable);
  ~IIRFilterHandler() override;

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
  void GetFrequencyResponse(int n_frequencies,
                            const float* frequency_hz,
                            float* mag_response,
                            float* phase_response);

 private:
  IIRFilterHandler(AudioNode&,
                   float sample_rate,
                   const Vector<double>& feedforward_coef,
                   const Vector<double>& feedback_coef,
                   bool is_filter_stable);

  // Returns the number of channels for both the input and the output.
  unsigned NumberOfChannels();
  AudioProcessor* Processor() { return processor_.get(); }

  // Returns true if the first output sample of any channel is non-finite.  This
  // is a proxy for determining if the filter state is bad.  For
  // BiquadFilterNodes and IIRFilterNodes, if the internal state has non-finite
  // values, the non-finite value propagates pretty much forever in the output.
  // This is because infinities and NaNs are sticky.
  bool HasNonFiniteOutput() const;

  void NotifyBadState() const;

  bool RequiresTailProcessing() const override;
  double TailTime() const override;
  double LatencyTime() const override;

  std::unique_ptr<AudioProcessor> processor_;

  // Only notify the user of the once.  No need to spam the console with
  // messages, because once we're in a bad state, it usually stays that way
  // forever.  Only accessed from audio thread.
  bool did_warn_bad_filter_state_ = false;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  base::WeakPtrFactory<IIRFilterHandler> weak_ptr_factory_{this};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_IIR_FILTER_HANDLER_H_
