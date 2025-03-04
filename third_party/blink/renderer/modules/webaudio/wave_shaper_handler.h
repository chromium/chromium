// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_WAVE_SHAPER_HANDLER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_WAVE_SHAPER_HANDLER_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/modules/webaudio/audio_basic_processor_handler.h"

namespace blink {

class AudioNode;
class WaveShaperProcessor;

class WaveShaperHandler : public AudioBasicProcessorHandler {
 public:
  enum OverSampleType { kOverSampleNone, kOverSample2x, kOverSample4x };

  static scoped_refptr<WaveShaperHandler> Create(AudioNode&, float sample_rate);

  void SetCurve(const float* curve_data, unsigned curve_length);
  Vector<float>* Curve();
  void SetOversample(OverSampleType oversample);
  OverSampleType Oversample();

 private:
  WaveShaperHandler(AudioNode& iirfilter_node, float sample_rate);

  WaveShaperProcessor* GetWaveShaperProcessor();
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_WAVE_SHAPER_HANDLER_H_
