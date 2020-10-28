// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this Sink code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/video_track_writer.h"

#include "media/base/video_frame.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-blink.h"
#include "third_party/blink/public/web/modules/mediastream/media_stream_video_source.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_frame.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_track_writer_parameters.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/streams/underlying_sink_base.h"
#include "third_party/blink/renderer/core/streams/writable_stream.h"
#include "third_party/blink/renderer/core/streams/writable_stream_default_controller.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_track.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_video_track.h"
#include "third_party/blink/renderer/modules/mediastream/pushable_media_stream_video_source.h"
#include "third_party/blink/renderer/modules/webcodecs/video_frame.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/uuid.h"

namespace blink {

// Implements a WritableStream's UnderlyingSinkBase by pushing frames into a
// PushableMediaStreamVideoSource.  Also optionally releases the frames.
class VideoTrackWritableStreamSink final : public UnderlyingSinkBase {
 public:
  // The source must out live the sink.
  VideoTrackWritableStreamSink(PushableMediaStreamVideoSource* source,
                               bool release_frames)
      : source_(source), release_frames_(release_frames) {}

  // UnderlyingSinkBase overrides.
  ScriptPromise start(ScriptState* script_state,
                      WritableStreamDefaultController* controller,
                      ExceptionState& exception_state) override {
    // We're ready write away
    return ScriptPromise::CastUndefined(script_state);
  }

  ScriptPromise write(ScriptState* script_state,
                      ScriptValue chunk,
                      WritableStreamDefaultController* controller,
                      ExceptionState& exception_state) override {
    VideoFrame* video_frame = V8VideoFrame::ToImplWithTypeCheck(
        script_state->GetIsolate(), chunk.V8Value());
    if (!video_frame) {
      exception_state.ThrowTypeError("Provided chunk is not a VideoFrame.");
      return ScriptPromise();
    }

    base::TimeTicks estimated_capture_time = base::TimeTicks::Now();
    source_->PushFrame(video_frame->frame(), estimated_capture_time);

    if (release_frames_)
      video_frame->destroy();

    return ScriptPromise::CastUndefined(script_state);
  }

  ScriptPromise abort(ScriptState* script_state,
                      ScriptValue reason,
                      ExceptionState& exception_state) override {
    source_->Stop();
    return ScriptPromise::CastUndefined(script_state);
  }

  ScriptPromise close(ScriptState* script_state,
                      ExceptionState& exception_state) override {
    source_->Stop();
    return ScriptPromise::CastUndefined(script_state);
  }

  PushableMediaStreamVideoSource* source_;
  bool release_frames_;
};

MediaStreamTrack* CreateVideoTrackFromSource(
    ScriptState* script_state,
    std::unique_ptr<MediaStreamVideoSource> video_source) {
  // Get "video_source.get()" before std::move(source) into owner.
  auto* video_source_ptr = video_source.get();

  String track_id = WTF::CreateCanonicalUUIDString();
  MediaStreamSource* video_source_owner =
      MakeGarbageCollected<MediaStreamSource>(
          track_id, MediaStreamSource::kTypeVideo, track_id /* name */,
          false /* remote */);
  video_source_owner->SetPlatformSource(std::move(video_source));

  return MakeGarbageCollected<MediaStreamTrack>(
      ExecutionContext::From(script_state),
      MediaStreamVideoTrack::CreateVideoTrack(
          video_source_ptr, MediaStreamVideoSource::ConstraintsOnceCallback(),
          true /* enabled */));
}

VideoTrackWriter* VideoTrackWriter::Create(
    ScriptState* script_state,
    const VideoTrackWriterParameters* params,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {  // when the context is detached
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "The context has been destroyed");

    return nullptr;
  }

  std::unique_ptr<PushableMediaStreamVideoSource> track_source =
      std::make_unique<PushableMediaStreamVideoSource>();
  VideoTrackWritableStreamSink* writable_sink =
      MakeGarbageCollected<VideoTrackWritableStreamSink>(
          track_source.get(), params->releaseFrames());

  auto* track =
      CreateVideoTrackFromSource(script_state, std::move(track_source));
  auto* writable = WritableStream::Create(
      script_state, ScriptValue::From(script_state, writable_sink),
      exception_state);
  return MakeGarbageCollected<VideoTrackWriter>(track, writable);
}

VideoTrackWriter::VideoTrackWriter(MediaStreamTrack* track,
                                   WritableStream* writable)
    : track_(track), writable_(writable) {}

WritableStream* VideoTrackWriter::writable() {
  return writable_;
}

MediaStreamTrack* VideoTrackWriter::track() {
  return track_;
}

void VideoTrackWriter::Trace(Visitor* visitor) const {
  visitor->Trace(track_);
  visitor->Trace(writable_);
}

}  // namespace blink
