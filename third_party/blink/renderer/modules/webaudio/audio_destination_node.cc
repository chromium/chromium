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

#include "third_party/blink/renderer/modules/webaudio/audio_destination_node.h"

#include "third_party/blink/renderer/modules/webaudio/audio_destination_handler.h"
#include "third_party/blink/renderer/modules/webaudio/audio_graph_tracer.h"

namespace blink {

AudioDestinationNode::AudioDestinationNode(BaseAudioContext& context)
    : AudioNode(context) {}

AudioDestinationHandler& AudioDestinationNode::GetAudioDestinationHandler()
    const {
  return static_cast<AudioDestinationHandler&>(Handler());
}

uint32_t AudioDestinationNode::maxChannelCount() const {
  return GetAudioDestinationHandler().MaxChannelCount();
}

void AudioDestinationNode::ReportDidCreate() {
  GraphTracer().DidCreateAudioNode(this);
}

void AudioDestinationNode::ReportWillBeDestroyed() {
  GraphTracer().WillDestroyAudioNode(this);
}

void AudioDestinationNode::SetSinkId(const String& sink_id,
                                     media::OutputDeviceStatusCB callback) {
  // TODO: Here is just temporary code for testing, we will bridge it to
  // RendererWebAudioDeviceImpl::SwitchOutputDevice once that change is finished

  if (sink_id == "1") {
    std::move(callback).Run(
        media::OutputDeviceStatus::OUTPUT_DEVICE_STATUS_ERROR_NOT_FOUND);
  } else if (sink_id == "2") {
    std::move(callback).Run(
        media::OutputDeviceStatus::OUTPUT_DEVICE_STATUS_ERROR_NOT_AUTHORIZED);
  } else if (sink_id == "3") {
    std::move(callback).Run(
        media::OutputDeviceStatus::OUTPUT_DEVICE_STATUS_ERROR_TIMED_OUT);
  } else {
    std::move(callback).Run(media::OutputDeviceStatus::OUTPUT_DEVICE_STATUS_OK);
  }
}

}  // namespace blink
