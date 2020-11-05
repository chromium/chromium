// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/media_stream_video_track_underlying_source.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/streams/readable_stream_default_controller_with_script_scope.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_track.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_video_track.h"
#include "third_party/blink/renderer/modules/webcodecs/video_frame.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/webrtc/api/frame_transformer_interface.h"

namespace blink {

MediaStreamVideoTrackUnderlyingSource::MediaStreamVideoTrackUnderlyingSource(
    ScriptState* script_state,
    MediaStreamComponent* track)
    : UnderlyingSourceBase(script_state),
      main_task_runner_(ExecutionContext::From(script_state)
                            ->GetTaskRunner(TaskType::kInternalMediaRealTime)),
      track_(track) {
  DCHECK(track_);
}

ScriptPromise MediaStreamVideoTrackUnderlyingSource::pull(
    ScriptState* script_state) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  // No backpressure support, so nothing to do here.
  return ScriptPromise::CastUndefined(script_state);
}

ScriptPromise MediaStreamVideoTrackUnderlyingSource::Start(
    ScriptState* script_state) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  MediaStreamVideoTrack* video_track = MediaStreamVideoTrack::From(track_);
  if (!video_track) {
    return ScriptPromise::RejectWithDOMException(
        script_state,
        DOMException::Create(
            "No input track",
            DOMException::GetErrorName(DOMExceptionCode::kInvalidStateError)));
  }
  ConnectToTrack(WebMediaStreamTrack(track_),
                 ConvertToBaseRepeatingCallback(CrossThreadBindRepeating(
                     &MediaStreamVideoTrackUnderlyingSource::OnFrameFromTrack,
                     WrapCrossThreadPersistent(this))),
                 /*is_sink_secure=*/false);
  return ScriptPromise::CastUndefined(script_state);
}

ScriptPromise MediaStreamVideoTrackUnderlyingSource::Cancel(
    ScriptState* script_state,
    ScriptValue reason) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DisconnectFromTrack();
  return ScriptPromise::CastUndefined(script_state);
}

void MediaStreamVideoTrackUnderlyingSource::Trace(Visitor* visitor) const {
  visitor->Trace(track_);
  UnderlyingSourceBase::Trace(visitor);
}

void MediaStreamVideoTrackUnderlyingSource::Close() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DisconnectFromTrack();

  if (Controller())
    Controller()->Close();
}

void MediaStreamVideoTrackUnderlyingSource::OnFrameFromTrack(
    scoped_refptr<media::VideoFrame> media_frame,
    base::TimeTicks estimated_capture_time) {
  PostCrossThreadTask(
      *main_task_runner_, FROM_HERE,
      CrossThreadBindOnce(
          &MediaStreamVideoTrackUnderlyingSource::OnFrameFromTrackOnMainThread,
          WrapCrossThreadPersistent(this), std::move(media_frame),
          estimated_capture_time));
}

void MediaStreamVideoTrackUnderlyingSource::OnFrameFromTrackOnMainThread(
    scoped_refptr<media::VideoFrame> media_frame,
    base::TimeTicks /*estimated_capture_time*/) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  // Drop the frame if there is already a queued frame in the controller.
  // Queueing even a small number of frames can result in significant
  // performance issues, so do not allow queueing more than one frame.
  if (!Controller() || Controller()->DesiredSize() < 0)
    return;

  VideoFrame* video_frame =
      MakeGarbageCollected<VideoFrame>(std::move(media_frame));
  Controller()->Enqueue(video_frame);
}

}  // namespace blink
