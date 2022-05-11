// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_video_underlying_source.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/core/streams/readable_stream_default_controller_with_script_scope.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_video_frame.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_video_frame_delegate.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/webrtc/api/frame_transformer_interface.h"

namespace blink {

// Frames should not be queued at all. We allow queuing a few frames to deal
// with transient slowdowns. Specified as a negative number of frames since
// queuing is reported by the stream controller as a negative desired size.
const int RTCEncodedVideoUnderlyingSource::kMinQueueDesiredSize = -60;

RTCEncodedVideoUnderlyingSource::RTCEncodedVideoUnderlyingSource(
    ScriptState* script_state,
    base::OnceClosure disconnect_callback)
    : UnderlyingSourceBase(script_state),
      script_state_(script_state),
      disconnect_callback_(std::move(disconnect_callback)) {
  DCHECK(disconnect_callback_);
}

ScriptPromise RTCEncodedVideoUnderlyingSource::pull(ScriptState* script_state) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // WebRTC is a push source without backpressure support, so nothing to do
  // here.
  return ScriptPromise::CastUndefined(script_state);
}

ScriptPromise RTCEncodedVideoUnderlyingSource::Cancel(ScriptState* script_state,
                                                      ScriptValue reason) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (disconnect_callback_)
    std::move(disconnect_callback_).Run();
  return ScriptPromise::CastUndefined(script_state);
}

void RTCEncodedVideoUnderlyingSource::Trace(Visitor* visitor) const {
  visitor->Trace(script_state_);
  UnderlyingSourceBase::Trace(visitor);
}

void RTCEncodedVideoUnderlyingSource::OnFrameFromSource(
    std::unique_ptr<webrtc::TransformableVideoFrameInterface> webrtc_frame) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // If the source is canceled or there are too many queued frames,
  // drop the new frame.
  if (!disconnect_callback_ || !GetExecutionContext()) {
    return;
  }
  if (!Controller()) {
    // TODO(ricea): Maybe avoid dropping frames during transfer?
    DVLOG(1) << "Dropped frame due to null Controller(). This can happen "
                "during transfer.";
    return;
  }
  if (Controller()->DesiredSize() <= kMinQueueDesiredSize) {
    dropped_frames_++;
    VLOG_IF(2, (dropped_frames_ % 20 == 0))
        << "Dropped total of " << dropped_frames_
        << " encoded video frames due to too many already being queued.";
    return;
  }

  RTCEncodedVideoFrame* encoded_frame =
      MakeGarbageCollected<RTCEncodedVideoFrame>(std::move(webrtc_frame));
  Controller()->Enqueue(encoded_frame);
}

void RTCEncodedVideoUnderlyingSource::Close() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (disconnect_callback_)
    std::move(disconnect_callback_).Run();

  Controller()->Close();
}

}  // namespace blink
