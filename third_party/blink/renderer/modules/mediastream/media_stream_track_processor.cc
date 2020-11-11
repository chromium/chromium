// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/media_stream_track_processor.h"

#include "third_party/blink/public/mojom/web_feature/web_feature.mojom-blink.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_utils.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_video_track.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_video_track_underlying_source.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/wtf/uuid.h"

namespace blink {

MediaStreamTrackProcessor::MediaStreamTrackProcessor(
    ScriptState* script_state,
    MediaStreamComponent* input_track)
    : input_track_(input_track) {
  DCHECK(input_track_);
  UseCounter::Count(ExecutionContext::From(script_state),
                    WebFeature::kMediaStreamTrackProcessor);
}

ReadableStream* MediaStreamTrackProcessor::readable(ScriptState* script_state) {
  DCHECK_EQ(input_track_->Source()->GetType(), MediaStreamSource::kTypeVideo);
  if (!source_stream_)
    CreateVideoSourceStream(script_state);
  return source_stream_;
}

void MediaStreamTrackProcessor::CreateVideoSourceStream(
    ScriptState* script_state) {
  DCHECK(!source_stream_);
  video_underlying_source_ =
      MakeGarbageCollected<MediaStreamVideoTrackUnderlyingSource>(script_state,
                                                                  input_track_);
  source_stream_ = ReadableStream::CreateWithCountQueueingStrategy(
      script_state, video_underlying_source_, /*high_water_mark=*/0);
}

MediaStreamTrackProcessor* MediaStreamTrackProcessor::Create(
    ScriptState* script_state,
    MediaStreamTrack* track,
    ExceptionState& exception_state) {
  if (!track) {
    exception_state.ThrowDOMException(DOMExceptionCode::kOperationError,
                                      "Input track cannot be null");
    return nullptr;
  }

  if (track->kind() != "video") {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "Only video tracks are supported");
    return nullptr;
  }

  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "The context has been destroyed");

    return nullptr;
  }

  return MakeGarbageCollected<MediaStreamTrackProcessor>(script_state,
                                                         track->Component());
}

void MediaStreamTrackProcessor::Trace(Visitor* visitor) const {
  visitor->Trace(input_track_);
  visitor->Trace(video_underlying_source_);
  visitor->Trace(source_stream_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
