// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_audio_underlying_source.h"

#include "base/memory/ptr_util.h"
#include "base/unguessable_token.h"
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
    : blink::RTCEncodedAudioUnderlyingSource(
          script_state,
          std::move(disconnect_callback),
          /*enable_frame_restrictions=*/false,
          base::UnguessableToken::Null(),
          /*controller_override=*/nullptr) {}

RTCEncodedAudioUnderlyingSource::RTCEncodedAudioUnderlyingSource(
    ScriptState* script_state,
    WTF::CrossThreadOnceClosure disconnect_callback,
    bool enable_frame_restrictions,
    base::UnguessableToken owner_id,
    ReadableStreamDefaultControllerWithScriptScope* override_controller)
    : UnderlyingSourceBase(script_state),
      script_state_(script_state),
      disconnect_callback_(std::move(disconnect_callback)),
      override_controller_(override_controller),
      enable_frame_restrictions_(enable_frame_restrictions),
      owner_id_(owner_id) {
  DCHECK(disconnect_callback_);

  ExecutionContext* context = ExecutionContext::From(script_state);
  task_runner_ = context->GetTaskRunner(TaskType::kInternalMediaRealTime);
}

ReadableStreamDefaultControllerWithScriptScope*
RTCEncodedAudioUnderlyingSource::GetController() {
  if (override_controller_) {
    return override_controller_;
  }
  return Controller();
}

ScriptPromiseUntyped RTCEncodedAudioUnderlyingSource::Pull(
    ScriptState* script_state,
    ExceptionState&) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  // WebRTC is a push source without backpressure support, so nothing to do
  // here.
  return ToResolvedUndefinedPromise(script_state);
}

ScriptPromiseUntyped RTCEncodedAudioUnderlyingSource::Cancel(
    ScriptState* script_state,
    ScriptValue reason,
    ExceptionState&) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  if (disconnect_callback_)
    std::move(disconnect_callback_).Run();
  return ToResolvedUndefinedPromise(script_state);
}

void RTCEncodedAudioUnderlyingSource::Trace(Visitor* visitor) const {
  visitor->Trace(script_state_);
  visitor->Trace(override_controller_);
  UnderlyingSourceBase::Trace(visitor);
}

void RTCEncodedAudioUnderlyingSource::OnFrameFromSource(
    std::unique_ptr<webrtc::TransformableAudioFrameInterface> webrtc_frame) {
  // It can happen that a frame is posted to the task runner of the old
  // execution context during a stream transfer to a new context.
  // TODO(https://crbug.com/1506631): Make the state updates related to the
  // transfer atomic and turn this into a DCHECK.
  if (!task_runner_->BelongsToCurrentThread()) {
    DVLOG(1) << "Dropped frame posted to incorrect task runner. This can "
                "happen during transfer.";
    return;
  }
  // If the source is canceled or there are too many queued frames,
  // drop the new frame.
  if (!disconnect_callback_ || !GetExecutionContext()) {
    return;
  }
  if (!GetController()) {
    // TODO(ricea): Maybe avoid dropping frames during transfer?
    DVLOG(1) << "Dropped frame due to null Controller(). This can happen "
                "during transfer.";
    return;
  }
  if (GetController()->DesiredSize() <= kMinQueueDesiredSize) {
    dropped_frames_++;
    VLOG_IF(2, (dropped_frames_ % 20 == 0))
        << "Dropped total of " << dropped_frames_
        << " encoded audio frames due to too many already being queued.";
    return;
  }
  RTCEncodedAudioFrame* encoded_frame;
  if (enable_frame_restrictions_) {
    encoded_frame = MakeGarbageCollected<RTCEncodedAudioFrame>(
        std::move(webrtc_frame), owner_id_, ++last_enqueued_frame_counter_);
  } else {
    encoded_frame =
        MakeGarbageCollected<RTCEncodedAudioFrame>(std::move(webrtc_frame));
  }
  GetController()->Enqueue(encoded_frame);
}

void RTCEncodedAudioUnderlyingSource::Close() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  if (disconnect_callback_)
    std::move(disconnect_callback_).Run();

  if (GetController()) {
    GetController()->Close();
  }
}

void RTCEncodedAudioUnderlyingSource::OnSourceTransferStartedOnTaskRunner() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  // This can potentially be called before the stream is constructed and so
  // Controller() is still unset.
  if (GetController()) {
    GetController()->Close();
  }
}

void RTCEncodedAudioUnderlyingSource::OnSourceTransferStarted() {
  PostCrossThreadTask(
      *task_runner_, FROM_HERE,
      CrossThreadBindOnce(
          &RTCEncodedAudioUnderlyingSource::OnSourceTransferStartedOnTaskRunner,
          WrapCrossThreadPersistent(this)));
}

}  // namespace blink
