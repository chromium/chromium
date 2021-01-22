// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/media_stream_audio_track_underlying_sink.h"

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_frame.h"
#include "third_party/blink/renderer/modules/mediastream/pushable_media_stream_audio_source.h"
#include "third_party/blink/renderer/modules/webcodecs/audio_frame.h"
#include "third_party/blink/renderer/modules/webcodecs/audio_frame_serialization_data.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

MediaStreamAudioTrackUnderlyingSink::MediaStreamAudioTrackUnderlyingSink(
    PushableMediaStreamAudioSource* source)
    : source_(source->GetWeakPtr()) {}

ScriptPromise MediaStreamAudioTrackUnderlyingSink::start(
    ScriptState* script_state,
    WritableStreamDefaultController* controller,
    ExceptionState& exception_state) {
  return ScriptPromise::CastUndefined(script_state);
}

ScriptPromise MediaStreamAudioTrackUnderlyingSink::write(
    ScriptState* script_state,
    ScriptValue chunk,
    WritableStreamDefaultController* controller,
    ExceptionState& exception_state) {
  AudioFrame* audio_frame = V8AudioFrame::ToImplWithTypeCheck(
      script_state->GetIsolate(), chunk.V8Value());
  if (!audio_frame) {
    exception_state.ThrowTypeError("Null audio frame.");
    return ScriptPromise();
  }

  if (!audio_frame->buffer()) {
    exception_state.ThrowTypeError("Empty or closed audio frame.");
    return ScriptPromise();
  }

  PushableMediaStreamAudioSource* pushable_source =
      static_cast<PushableMediaStreamAudioSource*>(source_.get());
  if (!pushable_source || !pushable_source->running()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Stream closed");
    return ScriptPromise();
  }

  pushable_source->PushAudioData(audio_frame->GetSerializationData());
  audio_frame->close();

  return ScriptPromise::CastUndefined(script_state);
}

ScriptPromise MediaStreamAudioTrackUnderlyingSink::abort(
    ScriptState* script_state,
    ScriptValue reason,
    ExceptionState& exception_state) {
  if (source_)
    source_->StopSource();
  return ScriptPromise::CastUndefined(script_state);
}

ScriptPromise MediaStreamAudioTrackUnderlyingSink::close(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  if (source_)
    source_->StopSource();
  return ScriptPromise::CastUndefined(script_state);
}

}  // namespace blink
