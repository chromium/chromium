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

#include "third_party/blink/renderer/modules/webaudio/dynamics_compressor_node.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_dynamics_compressor_options.h"
#include "third_party/blink/renderer/modules/webaudio/audio_graph_tracer.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_input.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_output.h"
#include "third_party/blink/renderer/platform/audio/audio_utilities.h"
#include "third_party/blink/renderer/platform/audio/dynamics_compressor.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"

namespace blink {

namespace {

constexpr double kDefaultThresholdValue = -24.0;
constexpr float kMinThresholdValue = -100.0f;
constexpr float kMaxThresholdValue = 0.0f;

constexpr double kDefaultKneeValue = 30.0;
constexpr float kMinKneeValue = 0.0f;
constexpr float kMaxKneeValue = 40.0f;

constexpr double kDefaultRatioValue = 12.0;
constexpr float kMinRatioValue = 1.0f;
constexpr float kMaxRatioValue = 20.0f;

constexpr double kDefaultAttackValue = 0.003;
constexpr float kMinAttackValue = 0.0f;
constexpr float kMaxAttackValue = 1.0f;

constexpr double kDefaultReleaseValue = 0.250;
constexpr float kMinReleaseValue = 0.0f;
constexpr float kMaxReleaseValue = 1.0f;

}  // namespace

DynamicsCompressorNode::DynamicsCompressorNode(BaseAudioContext& context)
    : AudioNode(context),
      threshold_(AudioParam::Create(
          context,
          Uuid(),
          AudioParamHandler::kParamTypeDynamicsCompressorThreshold,
          kDefaultThresholdValue,
          AudioParamHandler::AutomationRate::kControl,
          AudioParamHandler::AutomationRateMode::kFixed,
          kMinThresholdValue,
          kMaxThresholdValue)),
      knee_(AudioParam::Create(
          context,
          Uuid(),
          AudioParamHandler::kParamTypeDynamicsCompressorKnee,
          kDefaultKneeValue,
          AudioParamHandler::AutomationRate::kControl,
          AudioParamHandler::AutomationRateMode::kFixed,
          kMinKneeValue,
          kMaxKneeValue)),
      ratio_(AudioParam::Create(
          context,
          Uuid(),
          AudioParamHandler::kParamTypeDynamicsCompressorRatio,
          kDefaultRatioValue,
          AudioParamHandler::AutomationRate::kControl,
          AudioParamHandler::AutomationRateMode::kFixed,
          kMinRatioValue,
          kMaxRatioValue)),
      attack_(AudioParam::Create(
          context,
          Uuid(),
          AudioParamHandler::kParamTypeDynamicsCompressorAttack,
          kDefaultAttackValue,
          AudioParamHandler::AutomationRate::kControl,
          AudioParamHandler::AutomationRateMode::kFixed,
          kMinAttackValue,
          kMaxAttackValue)),
      release_(AudioParam::Create(
          context,
          Uuid(),
          AudioParamHandler::kParamTypeDynamicsCompressorRelease,
          kDefaultReleaseValue,
          AudioParamHandler::AutomationRate::kControl,
          AudioParamHandler::AutomationRateMode::kFixed,
          kMinReleaseValue,
          kMaxReleaseValue)) {
  SetHandler(DynamicsCompressorHandler::Create(
      *this, context.sampleRate(), threshold_->Handler(), knee_->Handler(),
      ratio_->Handler(), attack_->Handler(), release_->Handler()));
}

DynamicsCompressorNode* DynamicsCompressorNode::Create(
    BaseAudioContext& context,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  return MakeGarbageCollected<DynamicsCompressorNode>(context);
}

DynamicsCompressorNode* DynamicsCompressorNode::Create(
    BaseAudioContext* context,
    const DynamicsCompressorOptions* options,
    ExceptionState& exception_state) {
  DynamicsCompressorNode* node = Create(*context, exception_state);

  if (!node) {
    return nullptr;
  }

  node->HandleChannelOptions(options, exception_state);

  node->attack()->setValue(options->attack());
  node->knee()->setValue(options->knee());
  node->ratio()->setValue(options->ratio());
  node->release()->setValue(options->release());
  node->threshold()->setValue(options->threshold());

  return node;
}

void DynamicsCompressorNode::Trace(Visitor* visitor) const {
  visitor->Trace(threshold_);
  visitor->Trace(knee_);
  visitor->Trace(ratio_);
  visitor->Trace(attack_);
  visitor->Trace(release_);
  AudioNode::Trace(visitor);
}

DynamicsCompressorHandler&
DynamicsCompressorNode::GetDynamicsCompressorHandler() const {
  return static_cast<DynamicsCompressorHandler&>(Handler());
}

AudioParam* DynamicsCompressorNode::threshold() const {
  return threshold_.Get();
}

AudioParam* DynamicsCompressorNode::knee() const {
  return knee_.Get();
}

AudioParam* DynamicsCompressorNode::ratio() const {
  return ratio_.Get();
}

float DynamicsCompressorNode::reduction() const {
  return GetDynamicsCompressorHandler().ReductionValue();
}

AudioParam* DynamicsCompressorNode::attack() const {
  return attack_.Get();
}

AudioParam* DynamicsCompressorNode::release() const {
  return release_.Get();
}

void DynamicsCompressorNode::ReportDidCreate() {
  GraphTracer().DidCreateAudioNode(this);
  GraphTracer().DidCreateAudioParam(attack_);
  GraphTracer().DidCreateAudioParam(knee_);
  GraphTracer().DidCreateAudioParam(ratio_);
  GraphTracer().DidCreateAudioParam(release_);
  GraphTracer().DidCreateAudioParam(threshold_);
}

void DynamicsCompressorNode::ReportWillBeDestroyed() {
  GraphTracer().WillDestroyAudioParam(attack_);
  GraphTracer().WillDestroyAudioParam(knee_);
  GraphTracer().WillDestroyAudioParam(ratio_);
  GraphTracer().WillDestroyAudioParam(release_);
  GraphTracer().WillDestroyAudioParam(threshold_);
  GraphTracer().WillDestroyAudioNode(this);
}

}  // namespace blink
