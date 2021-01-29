// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/media_stream_track_processor.h"

#include "third_party/blink/public/mojom/web_feature/web_feature.mojom-blink.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_audio_track_underlying_source.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_utils.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_video_track.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_video_track_underlying_source.h"
#include "third_party/blink/renderer/modules/webcodecs/audio_frame_serialization_data.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/wtf/uuid.h"

namespace blink {

MediaStreamTrackProcessor::MediaStreamTrackProcessor(
    ScriptState* script_state,
    MediaStreamComponent* input_track,
    uint16_t buffer_size)
    : input_track_(input_track), buffer_size_(buffer_size) {
  DCHECK(input_track_);
  UseCounter::Count(ExecutionContext::From(script_state),
                    WebFeature::kMediaStreamTrackProcessor);
}

ReadableStream* MediaStreamTrackProcessor::readable(ScriptState* script_state) {
  if (source_stream_)
    return source_stream_;

  if (input_track_->Source()->GetType() == MediaStreamSource::kTypeVideo)
    CreateVideoSourceStream(script_state);
  else
    CreateAudioSourceStream(script_state);

  return source_stream_;
}

void MediaStreamTrackProcessor::CreateVideoSourceStream(
    ScriptState* script_state) {
  DCHECK(!source_stream_);
  video_underlying_source_ =
      MakeGarbageCollected<MediaStreamVideoTrackUnderlyingSource>(
          script_state, input_track_, buffer_size_);
  source_stream_ = ReadableStream::CreateWithCountQueueingStrategy(
      script_state, video_underlying_source_, /*high_water_mark=*/0);
}

void MediaStreamTrackProcessor::CreateAudioSourceStream(
    ScriptState* script_state) {
  DCHECK(!source_stream_);
  audio_underlying_source_ =
      MakeGarbageCollected<MediaStreamAudioTrackUnderlyingSource>(
          script_state, input_track_, buffer_size_);
  source_stream_ = ReadableStream::CreateWithCountQueueingStrategy(
      script_state, audio_underlying_source_, /*high_water_mark=*/0);
}

MediaStreamTrackProcessor* MediaStreamTrackProcessor::Create(
    ScriptState* script_state,
    MediaStreamTrack* track,
    uint16_t buffer_size,
    ExceptionState& exception_state) {
  if (!track) {
    exception_state.ThrowTypeError("Input track cannot be null");
    return nullptr;
  }

  if (track->readyState() == "ended") {
    exception_state.ThrowTypeError("Input track cannot be ended");
    return nullptr;
  }

  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The context has been destroyed");

    return nullptr;
  }

  return MakeGarbageCollected<MediaStreamTrackProcessor>(
      script_state, track->Component(), buffer_size);
}

MediaStreamTrackProcessor* MediaStreamTrackProcessor::Create(
    ScriptState* script_state,
    MediaStreamTrack* track,
    ExceptionState& exception_state) {
  if (!track) {
    exception_state.ThrowTypeError("Input track cannot be null");
    return nullptr;
  }
  // Using 1 as default buffer size for video since by default we do not want
  // to buffer, as buffering interferes with MediaStream sources that drop
  // frames if they start to be buffered (e.g, camera sources).
  // Using 10 as default for audio, which coincides with the buffer size for
  // the Web Audio MediaStream sink.
  uint16_t buffer_size = track->kind() == "video" ? 1u : 10u;
  return Create(script_state, track, buffer_size, exception_state);
}

void MediaStreamTrackProcessor::Trace(Visitor* visitor) const {
  visitor->Trace(input_track_);
  visitor->Trace(audio_underlying_source_);
  visitor->Trace(video_underlying_source_);
  visitor->Trace(source_stream_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
