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

#include "third_party/blink/renderer/modules/webaudio/audio_node_output.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_input.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_wiring.h"
#include "third_party/blink/renderer/modules/webaudio/base_audio_context.h"
#include "third_party/blink/renderer/platform/wtf/threading.h"

namespace blink {

AudioNodeOutput::AudioNodeOutput(AudioHandler* handler,
                                 unsigned number_of_channels)
    : handler_(*handler),
      number_of_channels_(number_of_channels),
      desired_number_of_channels_(number_of_channels) {
  DCHECK_LE(number_of_channels, BaseAudioContext::MaxNumberOfChannels());

  internal_bus_ = AudioBus::Create(
      number_of_channels, GetDeferredTaskHandler().RenderQuantumFrames());
}

void AudioNodeOutput::Dispose() {
  did_call_dispose_ = true;

  GetDeferredTaskHandler().RemoveMarkedAudioNodeOutput(this);
  DisconnectAll();
  DCHECK(inputs_.empty());
  DCHECK(params_.empty());
}

void AudioNodeOutput::SetNumberOfChannels(unsigned number_of_channels) {
  DCHECK_LE(number_of_channels, BaseAudioContext::MaxNumberOfChannels());
  GetDeferredTaskHandler().AssertGraphOwner();

  desired_number_of_channels_ = number_of_channels;

  if (GetDeferredTaskHandler().IsAudioThread()) {
    // If we're in the audio thread then we can take care of it right away (we
    // should be at the very start or end of a rendering quantum).
    UpdateNumberOfChannels();
  } else {
    DCHECK(!did_call_dispose_);
    // Let the context take care of it in the audio thread in the pre and post
    // render tasks.
    GetDeferredTaskHandler().MarkAudioNodeOutputDirty(this);
  }
}

void AudioNodeOutput::UpdateInternalBus() {
  if (NumberOfChannels() == internal_bus_->NumberOfChannels()) {
    return;
  }

  internal_bus_ = AudioBus::Create(
      NumberOfChannels(), GetDeferredTaskHandler().RenderQuantumFrames());
}

void AudioNodeOutput::UpdateRenderingState() {
  UpdateNumberOfChannels();
  rendering_fan_out_count_ = FanOutCount();
  rendering_param_fan_out_count_ = ParamFanOutCount();
}

void AudioNodeOutput::UpdateNumberOfChannels() {
  DCHECK(GetDeferredTaskHandler().IsAudioThread());
  GetDeferredTaskHandler().AssertGraphOwner();

  if (number_of_channels_ != desired_number_of_channels_) {
    number_of_channels_ = desired_number_of_channels_;
    UpdateInternalBus();
    PropagateChannelCount();
  }
}

void AudioNodeOutput::PropagateChannelCount() {
  DCHECK(GetDeferredTaskHandler().IsAudioThread());
  GetDeferredTaskHandler().AssertGraphOwner();

  if (IsChannelCountKnown()) {
    // Announce to any nodes we're connected to that we changed our channel
    // count for its input.
    for (AudioNodeInput* i : inputs_) {
      i->Handler().CheckNumberOfChannelsForInput(i);
    }
  }
}

AudioBus* AudioNodeOutput::Pull(AudioBus* in_place_bus,
                                uint32_t frames_to_process) {
  DCHECK(GetDeferredTaskHandler().IsAudioThread());
  DCHECK(rendering_fan_out_count_ > 0 || rendering_param_fan_out_count_ > 0);

  // Causes our AudioNode to process if it hasn't already for this render
  // quantum.  We try to do in-place processing (using inPlaceBus) if at all
  // possible, but we can't process in-place if we're connected to more than one
  // input (fan-out > 1).  In this case pull() is called multiple times per
  // rendering quantum, and the processIfNecessary() call below will cause our
  // node to process() only the first time, caching the output in
  // m_internalOutputBus for subsequent calls.

  is_in_place_ =
      in_place_bus && in_place_bus->NumberOfChannels() == NumberOfChannels() &&
      (rendering_fan_out_count_ + rendering_param_fan_out_count_) == 1;

  in_place_bus_ = is_in_place_ ? in_place_bus : nullptr;

  Handler().ProcessIfNecessary(frames_to_process);
  return Bus();
}

AudioBus* AudioNodeOutput::Bus() const {
  DCHECK(GetDeferredTaskHandler().IsAudioThread());
  return is_in_place_ ? in_place_bus_.get() : internal_bus_.get();
}

unsigned AudioNodeOutput::FanOutCount() {
  GetDeferredTaskHandler().AssertGraphOwner();
  return inputs_.size();
}

unsigned AudioNodeOutput::ParamFanOutCount() {
  GetDeferredTaskHandler().AssertGraphOwner();
  return params_.size();
}

unsigned AudioNodeOutput::RenderingFanOutCount() const {
  return rendering_fan_out_count_;
}

unsigned AudioNodeOutput::RenderingParamFanOutCount() const {
  DCHECK(GetDeferredTaskHandler().IsAudioThread());
  return rendering_param_fan_out_count_;
}

bool AudioNodeOutput::IsConnectedDuringRendering() const {
  DCHECK(GetDeferredTaskHandler().IsAudioThread());
  return RenderingFanOutCount() > 0 || RenderingParamFanOutCount() > 0;
}

void AudioNodeOutput::DisconnectAllInputs() {
  GetDeferredTaskHandler().AssertGraphOwner();

  // Disconnect changes inputs_, so we can't iterate directly over the hash set.
  Vector<AudioNodeInput*, 4> inputs(inputs_);
  for (AudioNodeInput* input : inputs) {
    AudioNodeWiring::Disconnect(*this, *input);
  }
  DCHECK(inputs_.empty());
}

void AudioNodeOutput::DisconnectAllParams() {
  GetDeferredTaskHandler().AssertGraphOwner();

  // Disconnect changes params_, so we can't iterate directly over the hash set.
  Vector<AudioParamHandler*, 4> params(params_);
  for (AudioParamHandler* param : params) {
    AudioNodeWiring::Disconnect(*this, *param);
  }
  DCHECK(params_.empty());
}

void AudioNodeOutput::DisconnectAll() {
  DisconnectAllInputs();
  DisconnectAllParams();
}

void AudioNodeOutput::Disable() {
  GetDeferredTaskHandler().AssertGraphOwner();

  if (is_enabled_) {
    is_enabled_ = false;
    for (AudioNodeInput* input : inputs_) {
      AudioNodeWiring::Disable(*this, *input);
    }
  }
}

void AudioNodeOutput::Enable() {
  GetDeferredTaskHandler().AssertGraphOwner();

  if (!is_enabled_) {
    is_enabled_ = true;
    for (AudioNodeInput* input : inputs_) {
      AudioNodeWiring::Enable(*this, *input);
    }
  }
}

}  // namespace blink
