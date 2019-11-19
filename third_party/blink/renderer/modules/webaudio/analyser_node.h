/*
 * Copyright (C) 2010, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_ANALYSER_NODE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_ANALYSER_NODE_H_

#include "third_party/blink/renderer/core/typed_arrays/array_buffer_view_helpers.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/modules/webaudio/audio_basic_inspector_node.h"
#include "third_party/blink/renderer/modules/webaudio/realtime_analyser.h"

namespace blink {

class BaseAudioContext;
class AnalyserOptions;
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
    // An AnalyserNode does actually propogate silence, but to get the
    // time and FFT data updated correctly, process() needs to be
    // called even if all the inputs are silent.
    return false;
  }

  RealtimeAnalyser analyser_;
};

class AnalyserNode final : public AudioBasicInspectorNode {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static AnalyserNode* Create(BaseAudioContext&, ExceptionState&);
  static AnalyserNode* Create(BaseAudioContext*,
                              const AnalyserOptions*,
                              ExceptionState&);

  AnalyserNode(BaseAudioContext&);

  unsigned fftSize() const;
  void setFftSize(unsigned size, ExceptionState&);
  unsigned frequencyBinCount() const;
  void setMinDecibels(double, ExceptionState&);
  double minDecibels() const;
  void setMaxDecibels(double, ExceptionState&);
  double maxDecibels() const;
  void setSmoothingTimeConstant(double, ExceptionState&);
  double smoothingTimeConstant() const;
  void getFloatFrequencyData(NotShared<DOMFloat32Array>);
  void getByteFrequencyData(NotShared<DOMUint8Array>);
  void getFloatTimeDomainData(NotShared<DOMFloat32Array>);
  void getByteTimeDomainData(NotShared<DOMUint8Array>);

  // InspectorHelperMixin
  void ReportDidCreate() final;
  void ReportWillBeDestroyed() final;

 private:
  AnalyserHandler& GetAnalyserHandler() const;

  void SetMinMaxDecibels(double min, double max, ExceptionState&);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_ANALYSER_NODE_H_
