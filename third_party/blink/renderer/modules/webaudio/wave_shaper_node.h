/*
 * Copyright (C) 2011, Google Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_WAVE_SHAPER_NODE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_WAVE_SHAPER_NODE_H_

#include "third_party/blink/renderer/core/typed_arrays/array_buffer_view_helpers.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/modules/webaudio/audio_basic_processor_handler.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node.h"
#include "third_party/blink/renderer/modules/webaudio/wave_shaper_processor.h"

namespace blink {

class BaseAudioContext;
class ExceptionState;
class WaveShaperOptions;

class WaveShaperHandler : public AudioBasicProcessorHandler {
 public:
  static scoped_refptr<WaveShaperHandler> Create(AudioNode&, float sample_rate);

 private:
  WaveShaperHandler(AudioNode& iirfilter_node, float sample_rate);
};

class WaveShaperNode final : public AudioNode {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static WaveShaperNode* Create(BaseAudioContext&, ExceptionState&);
  static WaveShaperNode* Create(BaseAudioContext*,
                                const WaveShaperOptions*,
                                ExceptionState&);

  explicit WaveShaperNode(BaseAudioContext&);

  // setCurve() is called on the main thread.
  void setCurve(NotShared<DOMFloat32Array>, ExceptionState&);
  void setCurve(const Vector<float>&, ExceptionState&);
  NotShared<DOMFloat32Array> curve();

  void setOversample(const String&);
  String oversample() const;

  // InspectorHelperMixin
  void ReportDidCreate() final;
  void ReportWillBeDestroyed() final;

 private:
  void SetCurveImpl(const float* curve_data,
                    unsigned curve_length,
                    ExceptionState&);
  WaveShaperProcessor* GetWaveShaperProcessor() const;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_WAVE_SHAPER_NODE_H_
