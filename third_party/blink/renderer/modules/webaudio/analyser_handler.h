// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_ANALYSER_HANDLER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_ANALYSER_HANDLER_H_

#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/modules/webaudio/audio_basic_inspector_handler.h"
#include "third_party/blink/renderer/modules/webaudio/realtime_analyser.h"

namespace blink {

class AudioNode;
class ExceptionState;

class AnalyserHandler final : public AudioBasicInspectorHandler {
 public:
  static scoped_refptr<AnalyserHandler> Create(AudioNode&, float sample_rate);
  ~AnalyserHandler() override;

  // AudioHandler
  void Process(uint32_t frames_to_process) override;

  unsigned FftSize() const { return analyser_.FftSize(); }
  void SetFftSize(unsigned size, ExceptionState&);

  unsigned FrequencyBinCount() const { return analyser_.FrequencyBinCount(); }

  void SetMinDecibels(double k, ExceptionState&);
  double MinDecibels() const { return analyser_.MinDecibels(); }

  void SetMaxDecibels(double k, ExceptionState&);
  double MaxDecibels() const { return analyser_.MaxDecibels(); }

  void SetMinMaxDecibels(double min, double max, ExceptionState&);

  void SetSmoothingTimeConstant(double k, ExceptionState&);
  double SmoothingTimeConstant() const {
    return analyser_.SmoothingTimeConstant();
  }

  void GetFloatFrequencyData(DOMFloat32Array* array, double current_time) {
    analyser_.GetFloatFrequencyData(array, current_time);
  }
  void GetByteFrequencyData(DOMUint8Array* array, double current_time) {
    analyser_.GetByteFrequencyData(array, current_time);
  }
  void GetFloatTimeDomainData(DOMFloat32Array* array) {
    analyser_.GetFloatTimeDomainData(array);
  }
  void GetByteTimeDomainData(DOMUint8Array* array) {
    analyser_.GetByteTimeDomainData(array);
  }

  // AnalyserNode needs special handling when updating the pull status
  // because the node must get pulled even if there are no inputs or
  // outputs so that the internal state is properly updated with the
  // correct time data.
  void UpdatePullStatusIfNeeded() override;

  bool RequiresTailProcessing() const final;
  double TailTime() const final;

 private:
  AnalyserHandler(AudioNode&, float sample_rate);
  bool PropagatesSilence() const override {
    // An AnalyserNode does actually propagate silence, but to get the
    // time and FFT data updated correctly, process() needs to be
    // called even if all the inputs are silent.
    return false;
  }

  RealtimeAnalyser analyser_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_ANALYSER_HANDLER_H_
