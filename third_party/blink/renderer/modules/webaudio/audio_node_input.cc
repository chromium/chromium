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

#include <algorithm>
#include <memory>

#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_output.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_wiring.h"

namespace blink {

AudioNodeInput::AudioNodeInput(AudioHandler& handler)
    : AudioSummingJunction(handler.Context()->GetDeferredTaskHandler()),
      handler_(handler) {
  // Set to mono by default.
  internal_summing_bus_ =
      AudioBus::Create(1, GetDeferredTaskHandler().RenderQuantumFrames());
}

AudioNodeInput::~AudioNodeInput() {
  AudioNodeWiring::WillBeDestroyed(*this);
}

void AudioNodeInput::DidUpdate() {
  Handler().CheckNumberOfChannelsForInput(this);
}

void AudioNodeInput::UpdateInternalBus() {
  DCHECK(GetDeferredTaskHandler().IsAudioThread());
  GetDeferredTaskHandler().AssertGraphOwner();

  unsigned number_of_input_channels = NumberOfChannels();

  if (number_of_input_channels == internal_summing_bus_->NumberOfChannels()) {
    return;
  }

  internal_summing_bus_ = AudioBus::Create(
      number_of_input_channels, GetDeferredTaskHandler().RenderQuantumFrames());
}

unsigned AudioNodeInput::NumberOfChannels() const {
  auto mode = Handler().InternalChannelCountMode();
  if (mode == V8ChannelCountMode::Enum::kExplicit) {
    return Handler().ChannelCount();
  }

  // Find the number of channels of the connection with the largest number of
  // channels.
  unsigned max_channels = 1;  // one channel is the minimum allowed

  for (AudioNodeOutput* output : outputs_) {
    // Use output()->numberOfChannels() instead of
    // output->bus()->numberOfChannels(), because the calling of
    // AudioNodeOutput::bus() is not safe here.
    max_channels = std::max(max_channels, output->NumberOfChannels());
  }

  if (mode == V8ChannelCountMode::Enum::kClampedMax) {
    max_channels =
        std::min(max_channels, static_cast<unsigned>(Handler().ChannelCount()));
  }

  return max_channels;
}

scoped_refptr<AudioBus> AudioNodeInput::Bus() {
  DCHECK(GetDeferredTaskHandler().IsAudioThread());

  // Handle single connection specially to allow for in-place processing.
  if (NumberOfRenderingConnections() == 1 &&
      Handler().InternalChannelCountMode() == V8ChannelCountMode::Enum::kMax) {
    return RenderingOutput(0)->Bus();
  }

  // Multiple connections case or complex ChannelCountMode (or no connections).
  return InternalSummingBus();
}

scoped_refptr<AudioBus> AudioNodeInput::InternalSummingBus() {
  DCHECK(GetDeferredTaskHandler().IsAudioThread());

  return internal_summing_bus_;
}

void AudioNodeInput::SumAllConnections(scoped_refptr<AudioBus> summing_bus,
                                       uint32_t frames_to_process) {
  DCHECK(GetDeferredTaskHandler().IsAudioThread());

  // We shouldn't be calling this method if there's only one connection, since
  // it's less efficient.
  //    DCHECK(numberOfRenderingConnections() > 1 ||
  //        handler().internalChannelCountMode() != AudioHandler::Max);

  DCHECK(summing_bus);

  summing_bus->Zero();

  AudioBus::ChannelInterpretation interpretation =
      Handler().InternalChannelInterpretation();

  for (unsigned i = 0; i < NumberOfRenderingConnections(); ++i) {
    AudioNodeOutput* output = RenderingOutput(i);
    DCHECK(output);

    // Render audio from this output.
    AudioBus* connection_bus = output->Pull(nullptr, frames_to_process);

    // Sum, with unity-gain.
    summing_bus->SumFrom(*connection_bus, interpretation);
  }
}

scoped_refptr<AudioBus> AudioNodeInput::Pull(AudioBus* in_place_bus,
                                             uint32_t frames_to_process) {
  DCHECK(GetDeferredTaskHandler().IsAudioThread());

  // Handle single connection case.
  if (NumberOfRenderingConnections() == 1 &&
      Handler().InternalChannelCountMode() == V8ChannelCountMode::Enum::kMax) {
    // The output will optimize processing using inPlaceBus if it's able.
    AudioNodeOutput* output = RenderingOutput(0);
    return output->Pull(in_place_bus, frames_to_process);
  }

  scoped_refptr<AudioBus> internal_summing_bus = InternalSummingBus();

  if (!NumberOfRenderingConnections()) {
    // At least, generate silence if we're not connected to anything.
    // FIXME: if we wanted to get fancy, we could propagate a 'silent hint' here
    // to optimize the downstream graph processing.
    internal_summing_bus->Zero();
    return internal_summing_bus;
  }

  // Handle multiple connections case.
  SumAllConnections(internal_summing_bus, frames_to_process);

  return internal_summing_bus;
}

}  // namespace blink
