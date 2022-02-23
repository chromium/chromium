// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/breakout_box/media_stream_video_track_underlying_sink.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_video_frame.h"
#include "third_party/blink/renderer/core/streams/writable_stream_transferring_optimizer.h"
#include "third_party/blink/renderer/modules/breakout_box/metrics.h"
#include "third_party/blink/renderer/modules/webcodecs/video_frame.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

namespace {

class TransferringOptimizer : public WritableStreamTransferringOptimizer {
 public:
  explicit TransferringOptimizer(
      scoped_refptr<PushableMediaStreamVideoSource::Broker> source_broker)
      : source_broker_(std::move(source_broker)) {}
  UnderlyingSinkBase* PerformInProcessOptimization(
      ScriptState* script_state) override {
    RecordBreakoutBoxUsage(BreakoutBoxUsage::kWritableVideoWorker);
    return MakeGarbageCollected<MediaStreamVideoTrackUnderlyingSink>(
        source_broker_);
  }

 private:
  const scoped_refptr<PushableMediaStreamVideoSource::Broker> source_broker_;
};

}  // namespace

MediaStreamVideoTrackUnderlyingSink::MediaStreamVideoTrackUnderlyingSink(
    scoped_refptr<PushableMediaStreamVideoSource::Broker> source_broker)
    : source_broker_(std::move(source_broker)) {
  RecordBreakoutBoxUsage(BreakoutBoxUsage::kWritableVideo);
}

ScriptPromise MediaStreamVideoTrackUnderlyingSink::start(
    ScriptState* script_state,
    WritableStreamDefaultController* controller,
    ExceptionState& exception_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  source_broker_->OnClientStarted();
  is_connected_ = true;
  return ScriptPromise::CastUndefined(script_state);
}

ScriptPromise MediaStreamVideoTrackUnderlyingSink::write(
    ScriptState* script_state,
    ScriptValue chunk,
    WritableStreamDefaultController* controller,
    ExceptionState& exception_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VideoFrame* video_frame = V8VideoFrame::ToImplWithTypeCheck(
      script_state->GetIsolate(), chunk.V8Value());
  if (!video_frame) {
    exception_state.ThrowTypeError("Null video frame.");
    return ScriptPromise();
  }

  if (!video_frame->frame()) {
    exception_state.ThrowTypeError("Empty video frame.");
    return ScriptPromise();
  }

  if (!source_broker_->IsRunning()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Stream closed");
    return ScriptPromise();
  }

  base::TimeTicks estimated_capture_time = base::TimeTicks::Now();
  source_broker_->PushFrame(video_frame->frame(), estimated_capture_time);
  // Invalidate the JS |video_frame|. Otherwise, the media frames might not be
  // released, which would leak resources and also cause some MediaStream
  // sources such as cameras to drop frames.
  video_frame->close();

  return ScriptPromise::CastUndefined(script_state);
}

ScriptPromise MediaStreamVideoTrackUnderlyingSink::abort(
    ScriptState* script_state,
    ScriptValue reason,
    ExceptionState& exception_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Disconnect();
  return ScriptPromise::CastUndefined(script_state);
}

ScriptPromise MediaStreamVideoTrackUnderlyingSink::close(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Disconnect();
  return ScriptPromise::CastUndefined(script_state);
}

std::unique_ptr<WritableStreamTransferringOptimizer>
MediaStreamVideoTrackUnderlyingSink::GetTransferringOptimizer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return std::make_unique<TransferringOptimizer>(source_broker_);
}

void MediaStreamVideoTrackUnderlyingSink::Disconnect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!is_connected_)
    return;

  source_broker_->OnClientStopped();
  is_connected_ = false;
}

}  // namespace blink
