// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/media_stream_track_generator.h"

#include "third_party/blink/public/mojom/web_feature/web_feature.mojom-blink.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/streams/writable_stream.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_utils.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_video_track.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_video_track_underlying_sink.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_video_track_underlying_source.h"
#include "third_party/blink/renderer/modules/mediastream/pushable_media_stream_video_source.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/wtf/uuid.h"

namespace blink {

MediaStreamTrackGenerator::MediaStreamTrackGenerator(
    ScriptState* script_state,
    MediaStreamSource::StreamType type,
    const String& track_id)
    : MediaStreamTrack(
          ExecutionContext::From(script_state),
          MakeGarbageCollected<MediaStreamComponent>(
              MakeGarbageCollected<MediaStreamSource>(track_id,
                                                      type,
                                                      track_id,
                                                      /*remote=*/false))) {
  CreateOutputPlatformTrack();
  UseCounter::Count(ExecutionContext::From(script_state),
                    WebFeature::kMediaStreamTrackGenerator);
}

WritableStream* MediaStreamTrackGenerator::writable(ScriptState* script_state) {
  DCHECK_EQ(kind(), "video");
  if (!writable_)
    CreateVideoStream(script_state);
  return writable_;
}

void MediaStreamTrackGenerator::CreateOutputPlatformTrack() {
  std::unique_ptr<PushableMediaStreamVideoSource> platform_source =
      std::make_unique<PushableMediaStreamVideoSource>();
  PushableMediaStreamVideoSource* platform_source_ptr = platform_source.get();
  Component()->Source()->SetPlatformSource(std::move(platform_source));
  std::unique_ptr<MediaStreamVideoTrack> platform_track =
      std::make_unique<MediaStreamVideoTrack>(
          platform_source_ptr,
          MediaStreamVideoSource::ConstraintsOnceCallback(),
          /*enabled=*/true);
  Component()->SetPlatformTrack(std::move(platform_track));
}

void MediaStreamTrackGenerator::CreateVideoStream(ScriptState* script_state) {
  DCHECK(!writable_);
  PushableMediaStreamVideoSource* source =
      static_cast<PushableMediaStreamVideoSource*>(
          Component()->Source()->GetPlatformSource());
  video_underlying_sink_ =
      MakeGarbageCollected<MediaStreamVideoTrackUnderlyingSink>(source);
  writable_ = WritableStream::CreateWithCountQueueingStrategy(
      script_state, video_underlying_sink_, /*high_water_mark=*/1);
}

MediaStreamTrackGenerator* MediaStreamTrackGenerator::Create(
    ScriptState* script_state,
    const String& kind,
    ExceptionState& exception_state) {
  if (kind != "video") {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "Only video tracks are supported");
    return nullptr;
  }

  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "Invalid context");

    return nullptr;
  }

  return MakeGarbageCollected<MediaStreamTrackGenerator>(
      script_state, MediaStreamSource::kTypeVideo,
      /*track_id=*/WTF::CreateCanonicalUUIDString());
}

void MediaStreamTrackGenerator::Trace(Visitor* visitor) const {
  visitor->Trace(video_underlying_sink_);
  visitor->Trace(writable_);
  MediaStreamTrack::Trace(visitor);
}

}  // namespace blink
