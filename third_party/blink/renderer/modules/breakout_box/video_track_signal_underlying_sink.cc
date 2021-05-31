// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/breakout_box/video_track_signal_underlying_sink.h"

#include "third_party/blink/public/web/modules/mediastream/media_stream_video_source.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_stream_track_signal.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_stream_track_signal_type.h"
#include "third_party/blink/renderer/modules/breakout_box/metrics.h"
#include "third_party/blink/renderer/modules/breakout_box/pushable_media_stream_video_source.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_track.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_video_track.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_component.h"

namespace blink {

VideoTrackSignalUnderlyingSink::VideoTrackSignalUnderlyingSink(
    MediaStreamTrack* track) {
  if (!MediaStreamVideoTrack::From(track->Component()))
    return;
  track_ = track;
  MediaStreamVideoSource* video_source =
      MediaStreamVideoSource::GetVideoSource(track->Component()->Source());
  if (video_source)
    source_ = video_source->GetWeakPtr();

  RecordBreakoutBoxUsage(BreakoutBoxUsage::kWritableControlVideo);
}

ScriptPromise VideoTrackSignalUnderlyingSink::start(
    ScriptState* script_state,
    WritableStreamDefaultController* controller,
    ExceptionState& exception_state) {
  return ScriptPromise::CastUndefined(script_state);
}

ScriptPromise VideoTrackSignalUnderlyingSink::write(
    ScriptState* script_state,
    ScriptValue chunk,
    WritableStreamDefaultController* controller,
    ExceptionState& exception_state) {
  MediaStreamTrackSignal* signal =
      NativeValueTraits<MediaStreamTrackSignal>::NativeValue(
          script_state->GetIsolate(), chunk.V8Value(), exception_state);
  if (!signal) {
    exception_state.ThrowTypeError("Null signal.");
    return ScriptPromise();
  }

  if (!track_ || track_->Ended()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "No live track");
    return ScriptPromise();
  }

  if (signal->signalType() == "request-frame") {
    if (!source_ || !source_->IsRunning()) {
      exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                        "No active source");
      return ScriptPromise();
    }
    source_->RequestRefreshFrame();
    return ScriptPromise::CastUndefined(script_state);
  } else if (signal->signalType() == "set-min-frame-rate") {
    if (!signal->hasFrameRate()) {
      exception_state.ThrowTypeError(
          "A non-negative frameRate is required for set-min-frame-rate.");
      return ScriptPromise();
    }
    if (auto* video_track = MediaStreamVideoTrack::From(track_->Component())) {
      video_track->SetMinimumFrameRate(signal->frameRate());
      return ScriptPromise::CastUndefined(script_state);
    } else {
      exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                        "No active track");
      return ScriptPromise();
    }
  }

  exception_state.ThrowTypeError("Invalid signal.");
  return ScriptPromise();
}

ScriptPromise VideoTrackSignalUnderlyingSink::abort(
    ScriptState* script_state,
    ScriptValue reason,
    ExceptionState& exception_state) {
  track_.Clear();
  source_.reset();
  return ScriptPromise::CastUndefined(script_state);
}

ScriptPromise VideoTrackSignalUnderlyingSink::close(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  track_.Clear();
  source_.reset();
  return ScriptPromise::CastUndefined(script_state);
}

void VideoTrackSignalUnderlyingSink::Trace(Visitor* visitor) const {
  visitor->Trace(track_);
  UnderlyingSinkBase::Trace(visitor);
}

}  // namespace blink
