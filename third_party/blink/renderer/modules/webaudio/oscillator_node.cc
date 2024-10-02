/*
 * Copyright (C) 2012, Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/modules/webaudio/oscillator_node.h"

#include <algorithm>
#include <limits>

#include "build/build_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_oscillator_type.h"
#include "third_party/blink/renderer/modules/webaudio/audio_graph_tracer.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_output.h"
#include "third_party/blink/renderer/modules/webaudio/periodic_wave.h"
#include "third_party/blink/renderer/platform/audio/audio_utilities.h"
#include "third_party/blink/renderer/platform/audio/vector_math.h"
#include "third_party/blink/renderer/platform/bindings/enumeration_base.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace blink {

namespace {

constexpr double kDefaultFrequencyValue = 440.0;
constexpr double kDefaultDetuneValue = 0.0;

}  // namespace

OscillatorNode::OscillatorNode(BaseAudioContext& context,
                               const String& oscillator_type,
                               PeriodicWave* wave_table)
    : AudioScheduledSourceNode(context),
      // Use musical pitch standard A440 as a default.
      frequency_(
          AudioParam::Create(context,
                             Uuid(),
                             AudioParamHandler::kParamTypeOscillatorFrequency,
                             kDefaultFrequencyValue,
                             AudioParamHandler::AutomationRate::kAudio,
                             AudioParamHandler::AutomationRateMode::kVariable,
                             /*min_value=*/-context.sampleRate() / 2,
                             /*max_value=*/context.sampleRate() / 2)),
      // Default to no detuning.
      detune_(AudioParam::Create(
          context,
          Uuid(),
          AudioParamHandler::kParamTypeOscillatorDetune,
          kDefaultDetuneValue,
          AudioParamHandler::AutomationRate::kAudio,
          AudioParamHandler::AutomationRateMode::kVariable,
          /*min_value=*/-1200 * log2f(std::numeric_limits<float>::max()),
          /*max_value=*/1200 * log2f(std::numeric_limits<float>::max()))) {
  SetHandler(
      OscillatorHandler::Create(*this, context.sampleRate(), oscillator_type,
                                wave_table ? wave_table->impl() : nullptr,
                                frequency_->Handler(), detune_->Handler()));
}

OscillatorNode* OscillatorNode::Create(BaseAudioContext& context,
                                       const String& oscillator_type,
                                       PeriodicWave* wave_table,
                                       ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  return MakeGarbageCollected<OscillatorNode>(context, oscillator_type,
                                              wave_table);
}

OscillatorNode* OscillatorNode::Create(BaseAudioContext* context,
                                       const OscillatorOptions* options,
                                       ExceptionState& exception_state) {
  if (options->type() == "custom" && !options->hasPeriodicWave()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "A PeriodicWave must be specified if the type is set to \"custom\"");
    return nullptr;
  }

  // TODO(crbug.com/1070871): Use periodicWaveOr(nullptr).
  OscillatorNode* node =
      Create(*context, IDLEnumAsString(options->type()),
             options->hasPeriodicWave() ? options->periodicWave() : nullptr,
             exception_state);

  if (!node) {
    return nullptr;
  }

  node->HandleChannelOptions(options, exception_state);

  node->detune()->setValue(options->detune());
  node->frequency()->setValue(options->frequency());

  return node;
}

void OscillatorNode::Trace(Visitor* visitor) const {
  visitor->Trace(frequency_);
  visitor->Trace(detune_);
  AudioScheduledSourceNode::Trace(visitor);
}

OscillatorHandler& OscillatorNode::GetOscillatorHandler() const {
  return static_cast<OscillatorHandler&>(Handler());
}

V8OscillatorType OscillatorNode::type() const {
  return V8OscillatorType(GetOscillatorHandler().GetType());
}

void OscillatorNode::setType(const V8OscillatorType& type,
                             ExceptionState& exception_state) {
  GetOscillatorHandler().SetType(type.AsEnum(), exception_state);
}

AudioParam* OscillatorNode::frequency() {
  return frequency_.Get();
}

AudioParam* OscillatorNode::detune() {
  return detune_.Get();
}

void OscillatorNode::setPeriodicWave(PeriodicWave* wave) {
  GetOscillatorHandler().SetPeriodicWave(wave->impl());
}

void OscillatorNode::ReportDidCreate() {
  GraphTracer().DidCreateAudioNode(this);
  GraphTracer().DidCreateAudioParam(detune_);
  GraphTracer().DidCreateAudioParam(frequency_);
}

void OscillatorNode::ReportWillBeDestroyed() {
  GraphTracer().WillDestroyAudioParam(detune_);
  GraphTracer().WillDestroyAudioParam(frequency_);
  GraphTracer().WillDestroyAudioNode(this);
}

}  // namespace blink
