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

#include "third_party/blink/renderer/modules/webaudio/wave_shaper_node.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_wave_shaper_options.h"
#include "third_party/blink/renderer/modules/webaudio/audio_graph_tracer.h"
#include "third_party/blink/renderer/modules/webaudio/base_audio_context.h"
#include "third_party/blink/renderer/modules/webaudio/wave_shaper_handler.h"
#include "third_party/blink/renderer/modules/webaudio/wave_shaper_processor.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

WaveShaperNode::WaveShaperNode(BaseAudioContext& context) : AudioNode(context) {
  SetHandler(WaveShaperHandler::Create(*this, context.sampleRate()));
}

WaveShaperNode* WaveShaperNode::Create(BaseAudioContext& context,
                                       ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  return MakeGarbageCollected<WaveShaperNode>(context);
}

WaveShaperNode* WaveShaperNode::Create(BaseAudioContext* context,
                                       const WaveShaperOptions* options,
                                       ExceptionState& exception_state) {
  WaveShaperNode* node = Create(*context, exception_state);

  if (!node) {
    return nullptr;
  }

  node->HandleChannelOptions(options, exception_state);

  if (options->hasCurve()) {
    node->setCurve(options->curve(), exception_state);
  }

  node->setOversample(options->oversample());

  return node;
}
WaveShaperProcessor* WaveShaperNode::GetWaveShaperProcessor() const {
  return static_cast<WaveShaperProcessor*>(
      static_cast<WaveShaperHandler&>(Handler()).Processor());
}

void WaveShaperNode::SetCurveImpl(const float* curve_data,
                                  size_t curve_length,
                                  ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  unsigned length = static_cast<unsigned>(curve_length);

  if (curve_data) {
    if (!base::CheckedNumeric<unsigned>(curve_length).AssignIfValid(&length)) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kNotSupportedError,
          "The curve length exceeds the maximum supported length");
      return;
    }
    if (length < 2) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kInvalidAccessError,
          ExceptionMessages::IndexExceedsMinimumBound<unsigned>("curve length",
                                                                length, 2));
      return;
    }
  }

  // This is to synchronize with the changes made in
  // AudioBasicProcessorNode::CheckNumberOfChannelsForInput() where we can
  // Initialize() and Uninitialize(), changing the number of kernels.
  DeferredTaskHandler::GraphAutoLocker context_locker(context());

  GetWaveShaperProcessor()->SetCurve(curve_data, length);
}

void WaveShaperNode::setCurve(NotShared<DOMFloat32Array> curve,
                              ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  if (curve) {
    SetCurveImpl(curve->Data(), curve->length(), exception_state);
  } else {
    SetCurveImpl(nullptr, 0, exception_state);
  }
}

void WaveShaperNode::setCurve(const Vector<float>& curve,
                              ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  SetCurveImpl(curve.data(), curve.size(), exception_state);
}

NotShared<DOMFloat32Array> WaveShaperNode::curve() {
  Vector<float>* curve = GetWaveShaperProcessor()->Curve();
  if (!curve) {
    return NotShared<DOMFloat32Array>(nullptr);
  }

  unsigned size = curve->size();

  NotShared<DOMFloat32Array> result(DOMFloat32Array::Create(size));
  memcpy(result->Data(), curve->data(), sizeof(float) * size);

  return result;
}

void WaveShaperNode::setOversample(const V8OverSampleType& type) {
  DCHECK(IsMainThread());

  // This is to synchronize with the changes made in
  // AudioBasicProcessorNode::checkNumberOfChannelsForInput() where we can
  // initialize() and uninitialize().
  DeferredTaskHandler::GraphAutoLocker context_locker(context());

  switch (type.AsEnum()) {
    case V8OverSampleType::Enum::kNone:
      GetWaveShaperProcessor()->SetOversample(
          WaveShaperProcessor::kOverSampleNone);
      return;
    case V8OverSampleType::Enum::k2X:
      GetWaveShaperProcessor()->SetOversample(
          WaveShaperProcessor::kOverSample2x);
      return;
    case V8OverSampleType::Enum::k4X:
      GetWaveShaperProcessor()->SetOversample(
          WaveShaperProcessor::kOverSample4x);
      return;
  }
  NOTREACHED();
}

V8OverSampleType WaveShaperNode::oversample() const {
  switch (const_cast<WaveShaperNode*>(this)
              ->GetWaveShaperProcessor()
              ->Oversample()) {
    case WaveShaperProcessor::kOverSampleNone:
      return V8OverSampleType(V8OverSampleType::Enum::kNone);
    case WaveShaperProcessor::kOverSample2x:
      return V8OverSampleType(V8OverSampleType::Enum::k2X);
    case WaveShaperProcessor::kOverSample4x:
      return V8OverSampleType(V8OverSampleType::Enum::k4X);
  }
  NOTREACHED();
}

void WaveShaperNode::ReportDidCreate() {
  GraphTracer().DidCreateAudioNode(this);
}

void WaveShaperNode::ReportWillBeDestroyed() {
  GraphTracer().WillDestroyAudioNode(this);
}

}  // namespace blink
