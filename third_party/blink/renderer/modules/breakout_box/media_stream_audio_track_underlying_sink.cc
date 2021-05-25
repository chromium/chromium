// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/breakout_box/media_stream_audio_track_underlying_sink.h"

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_data.h"
#include "third_party/blink/renderer/core/streams/writable_stream_transferring_optimizer.h"
#include "third_party/blink/renderer/modules/breakout_box/metrics.h"
#include "third_party/blink/renderer/modules/breakout_box/pushable_media_stream_audio_source.h"
#include "third_party/blink/renderer/modules/webcodecs/audio_data.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {
class PlaceholderTransferringOptimizer
    : public WritableStreamTransferringOptimizer {
  UnderlyingSinkBase* PerformInProcessOptimization(
      ScriptState* script_state) override {
    RecordBreakoutBoxUsage(BreakoutBoxUsage::kWritableAudioWorker);
    return nullptr;
  }
};
}  // namespace

MediaStreamAudioTrackUnderlyingSink::MediaStreamAudioTrackUnderlyingSink(
    PushableMediaStreamAudioSource* source)
    : source_(source->GetWeakPtr()) {
  RecordBreakoutBoxUsage(BreakoutBoxUsage::kWritableAudio);
}

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
  AudioData* audio_data = V8AudioData::ToImplWithTypeCheck(
      script_state->GetIsolate(), chunk.V8Value());
  if (!audio_data) {
    exception_state.ThrowTypeError("Null audio data.");
    return ScriptPromise();
  }

  if (!audio_data->data()) {
    exception_state.ThrowTypeError("Empty or closed audio data.");
    return ScriptPromise();
  }

  PushableMediaStreamAudioSource* pushable_source =
      static_cast<PushableMediaStreamAudioSource*>(source_.get());
  if (!pushable_source || !pushable_source->running()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Stream closed");
    return ScriptPromise();
  }

  pushable_source->PushAudioData(audio_data->data());
  audio_data->close();

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

std::unique_ptr<WritableStreamTransferringOptimizer>
MediaStreamAudioTrackUnderlyingSink::GetTransferringOptimizer() {
  return std::make_unique<PlaceholderTransferringOptimizer>();
}

}  // namespace blink
