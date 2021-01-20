// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/media_stream_track_generator.h"

#include "third_party/blink/public/mojom/web_feature/web_feature.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/streams/writable_stream.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_audio_track_underlying_sink.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_utils.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_video_track.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_video_track_underlying_sink.h"
#include "third_party/blink/renderer/modules/mediastream/pushable_media_stream_audio_source.h"
#include "third_party/blink/renderer/modules/mediastream/pushable_media_stream_video_source.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_track.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread.h"
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
  if (type == MediaStreamSource::kTypeVideo) {
    CreateVideoOutputPlatformTrack();
  } else {
    DCHECK_EQ(type, MediaStreamSource::kTypeAudio);
    CreateAudioOutputPlatformTrack();
  }
  UseCounter::Count(ExecutionContext::From(script_state),
                    WebFeature::kMediaStreamTrackGenerator);
}

WritableStream* MediaStreamTrackGenerator::writable(ScriptState* script_state) {
  if (writable_)
    return writable_;

  if (kind() == "video")
    CreateVideoStream(script_state);
  else if (kind() == "audio")
    CreateAudioStream(script_state);

  return writable_;
}

void MediaStreamTrackGenerator::CreateVideoOutputPlatformTrack() {
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

void MediaStreamTrackGenerator::CreateAudioOutputPlatformTrack() {
  // TODO(https:/crbug.com/1168281): use a different thread than the IO thread
  // to deliver Audio.
  std::unique_ptr<PushableMediaStreamAudioSource> platform_source =
      std::make_unique<PushableMediaStreamAudioSource>(
          GetExecutionContext()->GetTaskRunner(
              TaskType::kInternalMediaRealTime),
          Platform::Current()->GetIOTaskRunner());

  platform_source->ConnectToTrack(Component());

  Component()->Source()->SetPlatformSource(std::move(platform_source));
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

void MediaStreamTrackGenerator::CreateAudioStream(ScriptState* script_state) {
  DCHECK(!writable_);
  PushableMediaStreamAudioSource* source =
      static_cast<PushableMediaStreamAudioSource*>(
          Component()->Source()->GetPlatformSource());
  audio_underlying_sink_ =
      MakeGarbageCollected<MediaStreamAudioTrackUnderlyingSink>(source);
  writable_ = WritableStream::CreateWithCountQueueingStrategy(
      script_state, audio_underlying_sink_, /*high_water_mark=*/1);
}

MediaStreamTrackGenerator* MediaStreamTrackGenerator::Create(
    ScriptState* script_state,
    const String& kind,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "Invalid context");
    return nullptr;
  }

  MediaStreamSource::StreamType type;

  if (kind == "video") {
    type = MediaStreamSource::kTypeVideo;
  } else if (kind == "audio") {
    type = MediaStreamSource::kTypeAudio;
  } else {
    exception_state.ThrowTypeError("Invalid track generator kind");
    return nullptr;
  }

  return MakeGarbageCollected<MediaStreamTrackGenerator>(
      script_state, type,
      /*track_id=*/WTF::CreateCanonicalUUIDString());
}

void MediaStreamTrackGenerator::Trace(Visitor* visitor) const {
  visitor->Trace(video_underlying_sink_);
  visitor->Trace(audio_underlying_sink_);
  visitor->Trace(writable_);
  MediaStreamTrack::Trace(visitor);
}

}  // namespace blink
