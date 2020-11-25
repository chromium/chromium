// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/video_track_reader.h"

#include "base/threading/thread_task_runner_handle.h"
#include "media/base/video_frame.h"
#include "third_party/blink/public/mojom/web_feature/web_feature.mojom-blink.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_video_track.h"
#include "third_party/blink/renderer/modules/webcodecs/video_frame.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

VideoTrackReader::VideoTrackReader(ScriptState* script_state,
                                   MediaStreamTrack* track)
    : ExecutionContextLifecycleObserver(ExecutionContext::From(script_state)),
      started_(false),
      real_time_media_task_runner_(
          ExecutionContext::From(script_state)
              ->GetTaskRunner(TaskType::kInternalMediaRealTime)),
      track_(track) {
  UseCounter::Count(ExecutionContext::From(script_state),
                    WebFeature::kWebCodecs);
}

void VideoTrackReader::start(V8VideoFrameOutputCallback* callback,
                             ExceptionState& exception_state) {
  DCHECK(real_time_media_task_runner_->BelongsToCurrentThread());

  if (started_) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "The VideoTrackReader has already been started.");
    return;
  }

  started_ = true;
  callback_ = callback;
  ConnectToTrack(WebMediaStreamTrack(track_->Component()),
                 ConvertToBaseRepeatingCallback(CrossThreadBindRepeating(
                     &VideoTrackReader::OnFrameFromVideoTrack,
                     WrapCrossThreadPersistent(this))),
                 false /* is_sink_secure */);
}

void VideoTrackReader::stop(ExceptionState& exception_state) {
  DCHECK(real_time_media_task_runner_->BelongsToCurrentThread());

  if (!started_) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "The VideoTrackReader has already been stopped.");
    return;
  }

  StopInternal();
}

void VideoTrackReader::StopInternal() {
  DCHECK(real_time_media_task_runner_->BelongsToCurrentThread());
  started_ = false;
  callback_ = nullptr;
  DisconnectFromTrack();
}

void VideoTrackReader::OnFrameFromVideoTrack(
    scoped_refptr<media::VideoFrame> media_frame,
    base::TimeTicks estimated_capture_time) {
  // The value of estimated_capture_time here seems to almost always be the
  // system clock and most implementations of this callback ignore it.
  // So, we will also ignore it.
  DCHECK(media_frame);
  PostCrossThreadTask(
      *real_time_media_task_runner_.get(), FROM_HERE,
      CrossThreadBindOnce(&VideoTrackReader::ExecuteCallbackOnMainThread,
                          WrapCrossThreadPersistent(this),
                          std::move(media_frame)));
}

void VideoTrackReader::ExecuteCallbackOnMainThread(
    scoped_refptr<media::VideoFrame> media_frame) {
  DCHECK(real_time_media_task_runner_->BelongsToCurrentThread());

  if (!callback_) {
    // We may have already been stopped.
    return;
  }

  // If |track_|'s constraints changed (e.g. the resolution changed from a call
  // to MediaStreamTrack.applyConstraints() in JS), this |media_frame| might
  // still have the old constraints, due to the thread hop.
  // We may want to invalidate |media_frames| when constraints change, but it's
  // unclear whether this is a problem for now.

  auto* context = GetExecutionContext();
  if (!context)
    return;

  callback_->InvokeAndReportException(
      nullptr,
      MakeGarbageCollected<VideoFrame>(std::move(media_frame), context));
}

void VideoTrackReader::OnReadyStateChanged(
    WebMediaStreamSource::ReadyState state) {
  if (state == WebMediaStreamSource::kReadyStateEnded)
    StopInternal();
}

VideoTrackReader* VideoTrackReader::Create(ScriptState* script_state,
                                           MediaStreamTrack* track,
                                           ExceptionState& exception_state) {
  if (track->kind() != "video") {
    exception_state.ThrowTypeError(
        "Can only read video frames from video tracks.");
    return nullptr;
  }

  if (!script_state->ContextIsValid()) {  // when the context is detached
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "The context has been destroyed");

    return nullptr;
  }

  return MakeGarbageCollected<VideoTrackReader>(script_state, track);
}

void VideoTrackReader::Trace(Visitor* visitor) const {
  visitor->Trace(track_);
  visitor->Trace(callback_);
  ScriptWrappable::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink
