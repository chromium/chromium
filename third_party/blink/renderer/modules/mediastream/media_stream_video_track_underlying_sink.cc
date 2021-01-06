// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/media_stream_video_track_underlying_sink.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_video_frame.h"
#include "third_party/blink/renderer/modules/mediastream/pushable_media_stream_video_source.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

MediaStreamVideoTrackUnderlyingSink::MediaStreamVideoTrackUnderlyingSink(
    PushableMediaStreamVideoSource* source)
    : source_(source->GetWeakPtr()) {}

ScriptPromise MediaStreamVideoTrackUnderlyingSink::start(
    ScriptState* script_state,
    WritableStreamDefaultController* controller,
    ExceptionState& exception_state) {
  return ScriptPromise::CastUndefined(script_state);
}

ScriptPromise MediaStreamVideoTrackUnderlyingSink::write(
    ScriptState* script_state,
    ScriptValue chunk,
    WritableStreamDefaultController* controller,
    ExceptionState& exception_state) {
  VideoFrame* video_frame = V8VideoFrame::ToImplWithTypeCheck(
      script_state->GetIsolate(), chunk.V8Value());
  if (!video_frame) {
    exception_state.ThrowTypeError("Null video frame.");
    return ScriptPromise();
  }

  if (!video_frame->frame()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kOperationError,
                                      "Empty video frame.");
    return ScriptPromise();
  }

  PushableMediaStreamVideoSource* pushable_source =
      static_cast<PushableMediaStreamVideoSource*>(source_.get());
  if (!pushable_source || !pushable_source->running()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Stream closed");
    return ScriptPromise();
  }

  base::TimeTicks estimated_capture_time = base::TimeTicks::Now();
  pushable_source->PushFrame(video_frame->frame(), estimated_capture_time);
  // Invalidate the JS |video_frame|. Otherwise, the media frames might not be
  // released, which would leak resources and also cause some MediaStream
  // sources such as cameras to drop frames.
  video_frame->destroy();

  return ScriptPromise::CastUndefined(script_state);
}

ScriptPromise MediaStreamVideoTrackUnderlyingSink::abort(
    ScriptState* script_state,
    ScriptValue reason,
    ExceptionState& exception_state) {
  if (source_)
    source_->StopSource();
  return ScriptPromise::CastUndefined(script_state);
}

ScriptPromise MediaStreamVideoTrackUnderlyingSink::close(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  if (source_)
    source_->StopSource();
  return ScriptPromise::CastUndefined(script_state);
}

}  // namespace blink
