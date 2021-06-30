// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/breakout_box/media_stream_track_generator.h"

#include "third_party/blink/public/mojom/web_feature/web_feature.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_stream_track_generator_init.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/streams/writable_stream.h"
#include "third_party/blink/renderer/core/streams/writable_stream_transferring_optimizer.h"
#include "third_party/blink/renderer/modules/breakout_box/media_stream_audio_track_underlying_sink.h"
#include "third_party/blink/renderer/modules/breakout_box/media_stream_video_track_underlying_sink.h"
#include "third_party/blink/renderer/modules/breakout_box/pushable_media_stream_audio_source.h"
#include "third_party/blink/renderer/modules/breakout_box/pushable_media_stream_video_source.h"
#include "third_party/blink/renderer/modules/breakout_box/video_track_signal_underlying_source.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_utils.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_video_track.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_track.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_component.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread.h"
#include "third_party/blink/renderer/platform/wtf/uuid.h"

namespace blink {

namespace {

const wtf_size_t kDefaultMaxSignalBufferSize = 20u;

class NullUnderlyingSource : public UnderlyingSourceBase {
 public:
  explicit NullUnderlyingSource(ScriptState* script_state)
      : UnderlyingSourceBase(script_state) {}
};

}  // namespace

MediaStreamTrackGenerator* MediaStreamTrackGenerator::Create(
    ScriptState* script_state,
    const String& kind,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
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
      /*track_id=*/WTF::CreateCanonicalUUIDString(), nullptr,
      kDefaultMaxSignalBufferSize);
}

MediaStreamTrackGenerator* MediaStreamTrackGenerator::Create(
    ScriptState* script_state,
    MediaStreamTrackGeneratorInit* init,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Invalid context");
    return nullptr;
  }

  if (!init->hasKind()) {
    exception_state.ThrowTypeError("kind must be specified");
    return nullptr;
  }

  if (init->hasSignalTarget() && init->signalTarget()->kind() != init->kind()) {
    exception_state.ThrowTypeError("kind and signalTarget.kind() do not match");
    return nullptr;
  }

  MediaStreamSource::StreamType type;
  if (init->kind() == "video") {
    type = MediaStreamSource::kTypeVideo;
  } else if (init->kind() == "audio") {
    type = MediaStreamSource::kTypeAudio;
  } else {
    exception_state.ThrowTypeError("Invalid track generator kind");
    return nullptr;
  }

  wtf_size_t max_signal_buffer_size = kDefaultMaxSignalBufferSize;
  if (init->hasMaxSignalBufferSize())
    max_signal_buffer_size = init->maxSignalBufferSize();

  return MakeGarbageCollected<MediaStreamTrackGenerator>(
      script_state, type,
      /*track_id=*/WTF::CreateCanonicalUUIDString(), init->signalTarget(),
      max_signal_buffer_size);
}

MediaStreamTrackGenerator::MediaStreamTrackGenerator(
    ScriptState* script_state,
    MediaStreamSource::StreamType type,
    const String& track_id,
    MediaStreamTrack* signal_target,
    wtf_size_t max_signal_buffer_size)
    : MediaStreamTrack(
          ExecutionContext::From(script_state),
          MakeGarbageCollected<MediaStreamComponent>(
              MakeGarbageCollected<MediaStreamSource>(track_id,
                                                      type,
                                                      track_id,
                                                      /*remote=*/false))),
      max_signal_buffer_size_(max_signal_buffer_size) {
  if (type == MediaStreamSource::kTypeVideo) {
    CreateVideoOutputPlatformTrack(signal_target);
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

ReadableStream* MediaStreamTrackGenerator::readableControl(
    ScriptState* script_state) {
  if (readable_control_)
    return readable_control_;

  if (kind() == "video")
    CreateVideoControlStream(script_state);
  else if (kind() == "audio")
    CreateAudioControlStream(script_state);

  return readable_control_;
}

PushableMediaStreamVideoSource* MediaStreamTrackGenerator::PushableVideoSource()
    const {
  DCHECK_EQ(Component()->Source()->GetType(), MediaStreamSource::kTypeVideo);
  return static_cast<PushableMediaStreamVideoSource*>(
      MediaStreamVideoSource::GetVideoSource(Component()->Source()));
}

void MediaStreamTrackGenerator::CreateVideoOutputPlatformTrack(
    MediaStreamTrack* signal_target) {
  base::WeakPtr<MediaStreamVideoSource> signal_target_upstream_source;
  if (signal_target) {
    MediaStreamVideoSource* upstream_source =
        MediaStreamVideoSource::GetVideoSource(
            signal_target->Component()->Source());
    signal_target_upstream_source = upstream_source->GetWeakPtr();
  }

  std::unique_ptr<PushableMediaStreamVideoSource> platform_source =
      std::make_unique<PushableMediaStreamVideoSource>(
          signal_target_upstream_source);
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
      MakeGarbageCollected<MediaStreamVideoTrackUnderlyingSink>(
          source->GetBroker());
  writable_ = WritableStream::CreateWithCountQueueingStrategy(
      script_state, video_underlying_sink_, /*high_water_mark=*/1,
      video_underlying_sink_->GetTransferringOptimizer());
}

void MediaStreamTrackGenerator::CreateAudioStream(ScriptState* script_state) {
  DCHECK(!writable_);
  PushableMediaStreamAudioSource* source =
      static_cast<PushableMediaStreamAudioSource*>(
          Component()->Source()->GetPlatformSource());
  audio_underlying_sink_ =
      MakeGarbageCollected<MediaStreamAudioTrackUnderlyingSink>(source);
  writable_ = WritableStream::CreateWithCountQueueingStrategy(
      script_state, audio_underlying_sink_, /*high_water_mark=*/1,
      audio_underlying_sink_->GetTransferringOptimizer());
}

void MediaStreamTrackGenerator::CreateVideoControlStream(
    ScriptState* script_state) {
  DCHECK(!readable_control_);
  // TODO(crbug.com/1142955): Make the queue size configurable from the
  // constructor.
  control_underlying_source_ =
      MakeGarbageCollected<VideoTrackSignalUnderlyingSource>(
          script_state, this, max_signal_buffer_size_);
  readable_control_ = ReadableStream::CreateWithCountQueueingStrategy(
      script_state, control_underlying_source_, /*high_water_mark=*/0);
}

void MediaStreamTrackGenerator::CreateAudioControlStream(
    ScriptState* script_state) {
  DCHECK(!readable_control_);
  // Since no signals have been defined for audio, use a null source that
  // does nothing, so that a valid stream can be returned for audio
  // MediaStreamTrackGenerators.
  control_underlying_source_ =
      MakeGarbageCollected<NullUnderlyingSource>(script_state);
  readable_control_ = ReadableStream::CreateWithCountQueueingStrategy(
      script_state, control_underlying_source_, /*high_water_mark=*/0);
}

void MediaStreamTrackGenerator::Trace(Visitor* visitor) const {
  visitor->Trace(video_underlying_sink_);
  visitor->Trace(audio_underlying_sink_);
  visitor->Trace(writable_);
  visitor->Trace(control_underlying_source_);
  visitor->Trace(readable_control_);
  MediaStreamTrack::Trace(visitor);
}

}  // namespace blink
