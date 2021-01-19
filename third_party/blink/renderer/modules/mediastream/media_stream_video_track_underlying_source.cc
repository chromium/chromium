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
    MediaStreamComponent* track,
    wtf_size_t max_queue_size)
    : UnderlyingSourceBase(script_state),
      main_task_runner_(ExecutionContext::From(script_state)
                            ->GetTaskRunner(TaskType::kInternalMediaRealTime)),
      track_(track),
      max_queue_size_(std::max(1u, max_queue_size)) {
  DCHECK(track_);
}

ScriptPromise MediaStreamVideoTrackUnderlyingSource::pull(
    ScriptState* script_state) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  if (!queue_.empty()) {
    ProcessPullRequest();
  } else {
    is_pending_pull_ = true;
  }

  DCHECK_LT(queue_.size(), max_queue_size_);
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

double MediaStreamVideoTrackUnderlyingSource::DesiredSizeForTesting() const {
  return Controller()->DesiredSize();
}

void MediaStreamVideoTrackUnderlyingSource::Close() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DisconnectFromTrack();
  if (Controller())
    Controller()->Close();
  queue_.clear();
}

void MediaStreamVideoTrackUnderlyingSource::OnFrameFromTrack(
    scoped_refptr<media::VideoFrame> media_frame,
    std::vector<scoped_refptr<media::VideoFrame>> /*scaled_media_frames*/,
    base::TimeTicks estimated_capture_time) {
  // The scaled video frames are currently ignored.
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
  DCHECK_LE(queue_.size(), max_queue_size_);

  // If the |queue_| is empty and the consumer has signaled a pull, bypass
  // |queue_| and send the frame directly to the stream controller.
  if (queue_.empty() && is_pending_pull_) {
    SendFrameToStream(std::move(media_frame));
    return;
  }

  if (queue_.size() == max_queue_size_)
    queue_.pop_front();

  queue_.push_back(std::move(media_frame));
  if (is_pending_pull_) {
    ProcessPullRequest();
  }
}

void MediaStreamVideoTrackUnderlyingSource::ProcessPullRequest() {
  DCHECK(!queue_.empty());
  SendFrameToStream(std::move(queue_.front()));
  queue_.pop_front();
}

void MediaStreamVideoTrackUnderlyingSource::SendFrameToStream(
    scoped_refptr<media::VideoFrame> media_frame) {
  DCHECK(media_frame);
  DCHECK(Controller());
  VideoFrame* video_frame = MakeGarbageCollected<VideoFrame>(
      std::move(media_frame), GetExecutionContext());
  Controller()->Enqueue(video_frame);
  is_pending_pull_ = false;
}

}  // namespace blink
