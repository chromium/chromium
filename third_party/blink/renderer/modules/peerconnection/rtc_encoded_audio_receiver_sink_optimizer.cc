// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_audio_receiver_sink_optimizer.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

RtcEncodedAudioReceiverSinkOptimizer::RtcEncodedAudioReceiverSinkOptimizer(
    UnderlyingSinkSetter set_underlying_sink,
    scoped_refptr<blink::RTCEncodedAudioStreamTransformer::Broker> transformer)
    : set_underlying_sink_(std::move(set_underlying_sink)),
      transformer_(std::move(transformer)) {}

UnderlyingSinkBase*
RtcEncodedAudioReceiverSinkOptimizer::PerformInProcessOptimization(
    ScriptState* script_state) {
  auto* new_sink = MakeGarbageCollected<RTCEncodedAudioUnderlyingSink>(
      script_state, std::move(transformer_),
      /*detach_frame_data_on_write=*/false);

  std::move(set_underlying_sink_).Run(new_sink);

  return new_sink;
}

}  // namespace blink
