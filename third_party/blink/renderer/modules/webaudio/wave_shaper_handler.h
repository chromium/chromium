// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_WAVE_SHAPER_HANDLER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_WAVE_SHAPER_HANDLER_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_over_sample_type.h"
#include "third_party/blink/renderer/modules/webaudio/audio_handler.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class AudioNode;
class WaveShaperKernel;

// WaveShaperHandler implements non-linear distortion effects.
class WaveShaperHandler final : public AudioHandler {
 public:
  static scoped_refptr<WaveShaperHandler> Create(AudioNode&, float sample_rate);
  ~WaveShaperHandler() override;

  void SetCurve(const float* curve_data, unsigned curve_length);
  const Vector<float>* Curve() const;
  void SetOversample(V8OverSampleType::Enum oversample);
  V8OverSampleType::Enum Oversample() const;

 private:
  WaveShaperHandler(AudioNode& iirfilter_node, float sample_rate);

  // AudioHandler
  void Process(uint32_t frames_to_process) override;
  void ProcessOnlyAudioParams(uint32_t frames_to_process) override {}
  void Initialize() override;
  void Uninitialize() override;
  void CheckNumberOfChannelsForInput(AudioNodeInput*) override;
  bool RequiresTailProcessing() const override;
  double TailTime() const override;
  double LatencyTime() const override;
  void PullInputs(uint32_t frames_to_process) override;

  void WaveShaperCurveValues(float* destination,
                             const float* source,
                             uint32_t frames_to_process,
                             const float* curve_data,
                             int curve_length);

  const float sample_rate_;
  const unsigned render_quantum_frames_;

  mutable base::Lock process_lock_;
  Vector<std::unique_ptr<WaveShaperKernel>> kernels_ GUARDED_BY(process_lock_);

  // Tail time for the WaveShaper.  This basically can have two values: 0 and
  // infinity.  It only takes the value of infinity if the wave shaper curve
  // is such that a zero input produces a non-zero output.  In this case, the
  // node has an infinite tail so that silent input continues to produce
  // non-silent output.
  double tail_time_ GUARDED_BY(process_lock_) = 0;

  double latency_time_ GUARDED_BY(process_lock_) = 0;

  // `curve_` represents the non-linear shaping curve.  It can be read on the
  // main thread without holding `process_lock_`.
  std::unique_ptr<Vector<float>> curve_;

  // Can be read on the main thread without holding `process_lock_`.
  V8OverSampleType::Enum oversample_ = V8OverSampleType::Enum::kNone;

  // Work arrays needed by `WaveShaperCurveValues()`.  There's no state or
  // anything kept here.  See `WaveShaperCurveValues()` for details on what
  // these hold.
  AudioFloatArray virtual_index_;
  AudioFloatArray index_;
  AudioFloatArray v1_;
  AudioFloatArray v2_;
  AudioFloatArray f_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_WAVE_SHAPER_HANDLER_H_
