// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_audio_underlying_source.h"

#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/core/streams/readable_stream_default_controller_with_script_scope.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_audio_frame.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_audio_frame_delegate.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/cross_thread_persistent.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/webrtc/api/frame_transformer_interface.h"

namespace blink {

// Frames should not be queued at all. We allow queuing a few frames to deal
// with transient slowdowns. Specified as a negative number of frames since
// queuing is reported by the stream controller as a negative desired size.
const int RTCEncodedAudioUnderlyingSource::kMinQueueDesiredSize = -60;

RTCEncodedAudioUnderlyingSource::RTCEncodedAudioUnderlyingSource(
    ScriptState* script_state,
    WTF::CrossThreadOnceClosure disconnect_callback)
    : UnderlyingSourceBase(script_state),
      script_state_(script_state),
      disconnect_callback_(std::move(disconnect_callback)) {
  DCHECK(disconnect_callback_);

  ExecutionContext* context = ExecutionContext::From(script_state);
  task_runner_ = context->GetTaskRunner(TaskType::kInternalMediaRealTime);
}

ScriptPromise RTCEncodedAudioUnderlyingSource::pull(ScriptState* script_state) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  // WebRTC is a push source without backpressure support, so nothing to do
  // here.
  return ScriptPromise::CastUndefined(script_state);
}

ScriptPromise RTCEncodedAudioUnderlyingSource::Cancel(ScriptState* script_state,
                                                      ScriptValue reason) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  if (disconnect_callback_)
    std::move(disconnect_callback_).Run();
  return ScriptPromise::CastUndefined(script_state);
}

void RTCEncodedAudioUnderlyingSource::Trace(Visitor* visitor) const {
  visitor->Trace(script_state_);
  UnderlyingSourceBase::Trace(visitor);
}

void RTCEncodedAudioUnderlyingSource::OnFrameFromSource(
    std::unique_ptr<webrtc::TransformableAudioFrameInterface> webrtc_frame) {
  DCHECK(task_runner_->BelongsToCurrentThread());
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
        << " encoded audio frames due to too many already being queued.";
    return;
  }

  Controller()->Enqueue(
      MakeGarbageCollected<RTCEncodedAudioFrame>(std::move(webrtc_frame)));
}

void RTCEncodedAudioUnderlyingSource::Close() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  if (disconnect_callback_)
    std::move(disconnect_callback_).Run();

  if (Controller())
    Controller()->Close();
}

void RTCEncodedAudioUnderlyingSource::OnSourceTransferStartedOnTaskRunner() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  // This can potentially be called before the stream is constructed and so
  // Controller() is still unset.
  if (Controller())
    Controller()->Close();
}

void RTCEncodedAudioUnderlyingSource::OnSourceTransferStarted() {
  PostCrossThreadTask(
      *task_runner_, FROM_HERE,
      CrossThreadBindOnce(
          &RTCEncodedAudioUnderlyingSource::OnSourceTransferStartedOnTaskRunner,
          WrapCrossThreadPersistent(this)));
}

}  // namespace blink
