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

#include "third_party/blink/renderer/modules/webaudio/audio_node_input.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_output.h"
#include "third_party/blink/renderer/modules/webaudio/dynamics_compressor_options.h"
#include "third_party/blink/renderer/platform/audio/audio_utilities.h"
#include "third_party/blink/renderer/platform/audio/dynamics_compressor.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

// Set output to stereo by default.
static const unsigned defaultNumberOfOutputChannels = 2;

namespace blink {

DynamicsCompressorHandler::DynamicsCompressorHandler(
    AudioNode& node,
    float sample_rate,
    AudioParamHandler& threshold,
    AudioParamHandler& knee,
    AudioParamHandler& ratio,
    AudioParamHandler& attack,
    AudioParamHandler& release)
    : AudioHandler(kNodeTypeDynamicsCompressor, node, sample_rate),
      threshold_(&threshold),
      knee_(&knee),
      ratio_(&ratio),
      reduction_(0),
      attack_(&attack),
      release_(&release) {
  AddInput();
  AddOutput(defaultNumberOfOutputChannels);

  SetInternalChannelCountMode(kClampedMax);

  Initialize();
}

scoped_refptr<DynamicsCompressorHandler> DynamicsCompressorHandler::Create(
    AudioNode& node,
    float sample_rate,
    AudioParamHandler& threshold,
    AudioParamHandler& knee,
    AudioParamHandler& ratio,
    AudioParamHandler& attack,
    AudioParamHandler& release) {
  return base::AdoptRef(new DynamicsCompressorHandler(
      node, sample_rate, threshold, knee, ratio, attack, release));
}

DynamicsCompressorHandler::~DynamicsCompressorHandler() {
  Uninitialize();
}

void DynamicsCompressorHandler::Process(uint32_t frames_to_process) {
  AudioBus* output_bus = Output(0).Bus();
  DCHECK(output_bus);

  float threshold = threshold_->Value();
  float knee = knee_->Value();
  float ratio = ratio_->Value();
  float attack = attack_->Value();
  float release = release_->Value();

  dynamics_compressor_->SetParameterValue(DynamicsCompressor::kParamThreshold,
                                          threshold);
  dynamics_compressor_->SetParameterValue(DynamicsCompressor::kParamKnee, knee);
  dynamics_compressor_->SetParameterValue(DynamicsCompressor::kParamRatio,
                                          ratio);
  dynamics_compressor_->SetParameterValue(DynamicsCompressor::kParamAttack,
                                          attack);
  dynamics_compressor_->SetParameterValue(DynamicsCompressor::kParamRelease,
                                          release);

  dynamics_compressor_->Process(Input(0).Bus(), output_bus, frames_to_process);

  float reduction =
      dynamics_compressor_->ParameterValue(DynamicsCompressor::kParamReduction);
  reduction_.store(reduction, std::memory_order_relaxed);
}

void DynamicsCompressorHandler::ProcessOnlyAudioParams(
    uint32_t frames_to_process) {
  DCHECK(Context()->IsAudioThread());
  DCHECK_LE(frames_to_process, audio_utilities::kRenderQuantumFrames);

  float values[audio_utilities::kRenderQuantumFrames];

  threshold_->CalculateSampleAccurateValues(values, frames_to_process);
  knee_->CalculateSampleAccurateValues(values, frames_to_process);
  ratio_->CalculateSampleAccurateValues(values, frames_to_process);
  attack_->CalculateSampleAccurateValues(values, frames_to_process);
  release_->CalculateSampleAccurateValues(values, frames_to_process);
}

void DynamicsCompressorHandler::Initialize() {
  if (IsInitialized())
    return;

  AudioHandler::Initialize();
  dynamics_compressor_ = std::make_unique<DynamicsCompressor>(
      Context()->sampleRate(), defaultNumberOfOutputChannels);
}

bool DynamicsCompressorHandler::RequiresTailProcessing() const {
  // Always return true even if the tail time and latency might both be zero.
  return true;
}

double DynamicsCompressorHandler::TailTime() const {
  return dynamics_compressor_->TailTime();
}

double DynamicsCompressorHandler::LatencyTime() const {
  return dynamics_compressor_->LatencyTime();
}

void DynamicsCompressorHandler::SetChannelCount(
    unsigned channel_count,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());
  BaseAudioContext::GraphAutoLocker locker(Context());

  // A DynamicsCompressorNode only supports 1 or 2 channels
  if (channel_count > 0 && channel_count <= 2) {
    if (channel_count_ != channel_count) {
      channel_count_ = channel_count;
      if (InternalChannelCountMode() != kMax)
        UpdateChannelsForInputs();
    }
  } else {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        ExceptionMessages::IndexOutsideRange<uint32_t>(
            "channelCount", channel_count, 1,
            ExceptionMessages::kInclusiveBound, 2,
            ExceptionMessages::kInclusiveBound));
  }
}

void DynamicsCompressorHandler::SetChannelCountMode(
    const String& mode,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());
  BaseAudioContext::GraphAutoLocker locker(Context());

  ChannelCountMode old_mode = InternalChannelCountMode();

  if (mode == "clamped-max") {
    new_channel_count_mode_ = kClampedMax;
  } else if (mode == "explicit") {
    new_channel_count_mode_ = kExplicit;
  } else if (mode == "max") {
    // This is not supported for a DynamicsCompressorNode, which can
    // only handle 1 or 2 channels.
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "The provided value 'max' is not an "
                                      "allowed value for ChannelCountMode");
    new_channel_count_mode_ = old_mode;
  } else {
    // Do nothing for other invalid values.
    new_channel_count_mode_ = old_mode;
  }

  if (new_channel_count_mode_ != old_mode)
    Context()->GetDeferredTaskHandler().AddChangedChannelCountMode(this);
}
// ----------------------------------------------------------------

DynamicsCompressorNode::DynamicsCompressorNode(BaseAudioContext& context)
    : AudioNode(context),
      threshold_(AudioParam::Create(
          context,
          Uuid(),
          AudioParamHandler::kParamTypeDynamicsCompressorThreshold,
          -24,
          AudioParamHandler::AutomationRate::kControl,
          AudioParamHandler::AutomationRateMode::kFixed,
          -100,
          0)),
      knee_(AudioParam::Create(
          context,
          Uuid(),
          AudioParamHandler::kParamTypeDynamicsCompressorKnee,
          30,
          AudioParamHandler::AutomationRate::kControl,
          AudioParamHandler::AutomationRateMode::kFixed,
          0,
          40)),
      ratio_(AudioParam::Create(
          context,
          Uuid(),
          AudioParamHandler::kParamTypeDynamicsCompressorRatio,
          12,
          AudioParamHandler::AutomationRate::kControl,
          AudioParamHandler::AutomationRateMode::kFixed,
          1,
          20)),
      attack_(AudioParam::Create(
          context,
          Uuid(),
          AudioParamHandler::kParamTypeDynamicsCompressorAttack,
          0.003,
          AudioParamHandler::AutomationRate::kControl,
          AudioParamHandler::AutomationRateMode::kFixed,
          0,
          1)),
      release_(AudioParam::Create(
          context,
          Uuid(),
          AudioParamHandler::kParamTypeDynamicsCompressorRelease,
          0.250,
          AudioParamHandler::AutomationRate::kControl,
          AudioParamHandler::AutomationRateMode::kFixed,
          0,
          1)) {
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

  if (!node)
    return nullptr;

  node->HandleChannelOptions(options, exception_state);

  node->attack()->setValue(options->attack());
  node->knee()->setValue(options->knee());
  node->ratio()->setValue(options->ratio());
  node->release()->setValue(options->release());
  node->threshold()->setValue(options->threshold());

  return node;
}

void DynamicsCompressorNode::Trace(blink::Visitor* visitor) {
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
  return threshold_;
}

AudioParam* DynamicsCompressorNode::knee() const {
  return knee_;
}

AudioParam* DynamicsCompressorNode::ratio() const {
  return ratio_;
}

float DynamicsCompressorNode::reduction() const {
  return GetDynamicsCompressorHandler().ReductionValue();
}

AudioParam* DynamicsCompressorNode::attack() const {
  return attack_;
}

AudioParam* DynamicsCompressorNode::release() const {
  return release_;
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
