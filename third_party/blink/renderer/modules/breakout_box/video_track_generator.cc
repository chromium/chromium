// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/breakout_box/video_track_generator.h"

#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_track_generator.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/breakout_box/media_stream_track_generator.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_component.h"
#include "third_party/blink/renderer/platform/wtf/uuid.h"

namespace blink {

VideoTrackGenerator* VideoTrackGenerator::Create(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Invalid context");
    return nullptr;
  }
  // The implementation of VideoTrackGenerator in worker is a work in
  // progress. It is known to have security issues at the moment, so
  // don't allow it - developers will have to remove this check when
  // the project is resumed.
  if (!ExecutionContext::From(script_state)->IsWindow()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "VideoTrackGenerator in worker does not work yet");
  }

  return MakeGarbageCollected<VideoTrackGenerator>(script_state,
                                                   exception_state);
}

VideoTrackGenerator::VideoTrackGenerator(ScriptState* script_state,
                                         ExceptionState& exception_state) {
  wrapped_generator_ = MakeGarbageCollected<MediaStreamTrackGenerator>(
      script_state, MediaStreamSource::kTypeVideo);
}

WritableStream* VideoTrackGenerator::writable(ScriptState* script_state) {
  return wrapped_generator_->writable(script_state);
}

bool VideoTrackGenerator::muted() {
  return wrapped_generator_->PushableVideoSource()->GetBroker()->IsMuted();
}

void VideoTrackGenerator::setMuted(bool muted) {
  wrapped_generator_->PushableVideoSource()->GetBroker()->SetMuted(muted);
}

MediaStreamTrack* VideoTrackGenerator::track() {
  return wrapped_generator_.Get();
}

void VideoTrackGenerator::Trace(Visitor* visitor) const {
  visitor->Trace(wrapped_generator_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
