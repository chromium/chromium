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

#include "third_party/blink/renderer/modules/webaudio/realtime_audio_destination_node.h"

#include "base/feature_list.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/web_audio_latency_hint.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/renderer/modules/webaudio/audio_context.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_input.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_output.h"
#include "third_party/blink/renderer/modules/webaudio/audio_worklet.h"
#include "third_party/blink/renderer/modules/webaudio/audio_worklet_messaging_proxy.h"
#include "third_party/blink/renderer/modules/webaudio/cross_thread_audio_worklet_processor_info.h"
#include "third_party/blink/renderer/platform/audio/audio_utilities.h"
#include "third_party/blink/renderer/platform/audio/denormal_disabler.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"

namespace blink {

RealtimeAudioDestinationNode::RealtimeAudioDestinationNode(
    AudioContext& context,
    const WebAudioSinkDescriptor& sink_descriptor,
    const WebAudioLatencyHint& latency_hint,
    std::optional<float> sample_rate,
    bool update_echo_cancellation_on_first_start)
    : AudioDestinationNode(context) {
  SetHandler(RealtimeAudioDestinationHandler::Create(
      *this, sink_descriptor, latency_hint, sample_rate,
      update_echo_cancellation_on_first_start));
}

RealtimeAudioDestinationNode* RealtimeAudioDestinationNode::Create(
    AudioContext* context,
    const WebAudioSinkDescriptor& sink_descriptor,
    const WebAudioLatencyHint& latency_hint,
    std::optional<float> sample_rate,
    bool update_echo_cancellation_on_first_start) {
  return MakeGarbageCollected<RealtimeAudioDestinationNode>(
      *context, sink_descriptor, latency_hint, sample_rate,
      update_echo_cancellation_on_first_start);
}

RealtimeAudioDestinationHandler& RealtimeAudioDestinationNode::GetOwnHandler()
    const {
  return static_cast<RealtimeAudioDestinationHandler&>(Handler());
}

void RealtimeAudioDestinationNode::SetSinkDescriptor(
    const WebAudioSinkDescriptor& sink_descriptor,
    media::OutputDeviceStatusCB callback) {
  DCHECK(IsMainThread());

  static_cast<RealtimeAudioDestinationHandler&>(Handler())
      .SetSinkDescriptor(sink_descriptor, std::move(callback));
}

}  // namespace blink
