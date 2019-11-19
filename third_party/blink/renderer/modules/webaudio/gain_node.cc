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

#include "third_party/blink/renderer/modules/webaudio/audio_node_input.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_output.h"
#include "third_party/blink/renderer/modules/webaudio/gain_node.h"
#include "third_party/blink/renderer/modules/webaudio/gain_options.h"
#include "third_party/blink/renderer/platform/audio/audio_bus.h"
#include "third_party/blink/renderer/platform/audio/audio_utilities.h"

namespace blink {

GainHandler::GainHandler(AudioNode& node,
                         float sample_rate,
                         AudioParamHandler& gain)
    : AudioHandler(kNodeTypeGain, node, sample_rate),
      gain_(&gain),
      sample_accurate_gain_values_(
          audio_utilities::kRenderQuantumFrames)  // FIXME: can probably
                                                  // share temp buffer
                                                  // in context
{
  AddInput();
  AddOutput(1);

  Initialize();
}

scoped_refptr<GainHandler> GainHandler::Create(AudioNode& node,
                                               float sample_rate,
                                               AudioParamHandler& gain) {
  return base::AdoptRef(new GainHandler(node, sample_rate, gain));
}

void GainHandler::Process(uint32_t frames_to_process) {
  // FIXME: for some cases there is a nice optimization to avoid processing
  // here, and let the gain change happen in the summing junction input of the
  // AudioNode we're connected to.  Then we can avoid all of the following:

  AudioBus* output_bus = Output(0).Bus();
  DCHECK(output_bus);

  if (!IsInitialized() || !Input(0).IsConnected()) {
    output_bus->Zero();
  } else {
    AudioBus* input_bus = Input(0).Bus();

    if (gain_->HasSampleAccurateValues()) {
      // Apply sample-accurate gain scaling for precise envelopes, grain
      // windows, etc.
      DCHECK_LE(frames_to_process, sample_accurate_gain_values_.size());
      float* gain_values = sample_accurate_gain_values_.Data();
      gain_->CalculateSampleAccurateValues(gain_values, frames_to_process);
      output_bus->CopyWithSampleAccurateGainValuesFrom(*input_bus, gain_values,
                                                       frames_to_process);
    } else {
      // Apply the gain.
      if (gain_->Value() == 0) {
        // If the gain is 0, just zero the bus and set the silence hint.
        output_bus->Zero();
      } else {
        output_bus->CopyWithGainFrom(*input_bus, gain_->Value());
      }
    }
  }
}

void GainHandler::ProcessOnlyAudioParams(uint32_t frames_to_process) {
  DCHECK(Context()->IsAudioThread());
  DCHECK_LE(frames_to_process, audio_utilities::kRenderQuantumFrames);

  float values[audio_utilities::kRenderQuantumFrames];

  gain_->CalculateSampleAccurateValues(values, frames_to_process);
}

// FIXME: this can go away when we do mixing with gain directly in summing
// junction of AudioNodeInput
//
// As soon as we know the channel count of our input, we can lazily initialize.
// Sometimes this may be called more than once with different channel counts, in
// which case we must safely uninitialize and then re-initialize with the new
// channel count.
void GainHandler::CheckNumberOfChannelsForInput(AudioNodeInput* input) {
  DCHECK(Context()->IsAudioThread());
  Context()->AssertGraphOwner();

  DCHECK(input);
  DCHECK_EQ(input, &this->Input(0));

  unsigned number_of_channels = input->NumberOfChannels();

  if (IsInitialized() && number_of_channels != Output(0).NumberOfChannels()) {
    // We're already initialized but the channel count has changed.
    Uninitialize();
  }

  if (!IsInitialized()) {
    // This will propagate the channel count to any nodes connected further
    // downstream in the graph.
    Output(0).SetNumberOfChannels(number_of_channels);
    Initialize();
  }

  AudioHandler::CheckNumberOfChannelsForInput(input);
}

// ----------------------------------------------------------------

GainNode::GainNode(BaseAudioContext& context)
    : AudioNode(context),
      gain_(AudioParam::Create(
          context,
          Uuid(),
          AudioParamHandler::kParamTypeGainGain,
          1.0,
          AudioParamHandler::AutomationRate::kAudio,
          AudioParamHandler::AutomationRateMode::kVariable)) {
  SetHandler(
      GainHandler::Create(*this, context.sampleRate(), gain_->Handler()));
}

GainNode* GainNode::Create(BaseAudioContext& context,
                           ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  return MakeGarbageCollected<GainNode>(context);
}

GainNode* GainNode::Create(BaseAudioContext* context,
                           const GainOptions* options,
                           ExceptionState& exception_state) {
  GainNode* node = Create(*context, exception_state);

  if (!node)
    return nullptr;

  node->HandleChannelOptions(options, exception_state);

  node->gain()->setValue(options->gain());

  return node;
}

AudioParam* GainNode::gain() const {
  return gain_;
}

void GainNode::Trace(blink::Visitor* visitor) {
  visitor->Trace(gain_);
  AudioNode::Trace(visitor);
}

void GainNode::ReportDidCreate() {
  GraphTracer().DidCreateAudioNode(this);
  GraphTracer().DidCreateAudioParam(gain_);
}

void GainNode::ReportWillBeDestroyed() {
  GraphTracer().WillDestroyAudioParam(gain_);
  GraphTracer().WillDestroyAudioNode(this);
}

}  // namespace blink
