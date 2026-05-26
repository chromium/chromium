// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/breakout_box/media_stream_audio_track_underlying_sink.h"

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_data.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/streams/writable_stream_transferring_optimizer.h"
#include "third_party/blink/renderer/core/timing/performance.h"
#include "third_party/blink/renderer/modules/breakout_box/media_stream_audio_track_underlying_source.h"
#include "third_party/blink/renderer/modules/breakout_box/metrics.h"
#include "third_party/blink/renderer/modules/breakout_box/pushable_media_stream_audio_source.h"
#include "third_party/blink/renderer/modules/webcodecs/audio_data.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

class TransferringOptimizer : public WritableStreamTransferringOptimizer {
 public:
  explicit TransferringOptimizer(
      scoped_refptr<PushableMediaStreamAudioSource::Broker> source_broker)
      : source_broker_(std::move(source_broker)) {}
  UnderlyingSinkBase* PerformInProcessOptimization(
      ScriptState* script_state) override {
    RecordBreakoutBoxUsage(BreakoutBoxUsage::kWritableAudioWorker);
    if (ExecutionContext::From(script_state)->IsWorkerGlobalScope()) {
      source_broker_->SetShouldDeliverAudioOnAudioTaskRunner(false);
    }
    return MakeGarbageCollected<MediaStreamAudioTrackUnderlyingSink>(
        script_state, source_broker_);
  }

 private:
  const scoped_refptr<PushableMediaStreamAudioSource::Broker> source_broker_;
};

base::TimeTicks GetTimeOrigin(ScriptState* script_state) {
  Performance* performance =
      MediaStreamAudioTrackUnderlyingSource::GetPerformanceFromExecutionContext(
          ExecutionContext::From(script_state));
  return performance ? performance->GetTimeOriginInternal() : base::TimeTicks();
}

}  // namespace

MediaStreamAudioTrackUnderlyingSink::MediaStreamAudioTrackUnderlyingSink(
    ScriptState* script_state,
    scoped_refptr<PushableMediaStreamAudioSource::Broker> source_broker)
    : source_broker_(std::move(source_broker)),
      time_origin_(GetTimeOrigin(script_state)),
      is_expose_page_relative_capture_time_enabled_(
          base::FeatureList::IsEnabled(
              kBreakoutBoxExposePageRelativeAudioCaptureTime)) {
  DCHECK(source_broker_);
  RecordBreakoutBoxUsage(BreakoutBoxUsage::kWritableAudio);
}

ScriptPromise<IDLUndefined> MediaStreamAudioTrackUnderlyingSink::start(
    ScriptState* script_state,
    WritableStreamDefaultController* controller,
    ExceptionState& exception_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  source_broker_->OnClientStarted();
  is_connected_ = true;
  return ToResolvedUndefinedPromise(script_state);
}

ScriptPromise<IDLUndefined> MediaStreamAudioTrackUnderlyingSink::write(
    ScriptState* script_state,
    ScriptValue chunk,
    WritableStreamDefaultController* controller,
    ExceptionState& exception_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  AudioData* audio_data =
      V8AudioData::ToWrappable(script_state->GetIsolate(), chunk.V8Value());
  if (!audio_data) {
    exception_state.ThrowTypeError("Null audio data.");
    return EmptyPromise();
  }

  if (!audio_data->data()) {
    exception_state.ThrowTypeError("Empty or closed audio data.");
    return EmptyPromise();
  }

  if (!source_broker_->IsRunning()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Stream closed");
    return EmptyPromise();
  }

  if (is_expose_page_relative_capture_time_enabled_ && time_origin_.is_null()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Context destroyed");
    return EmptyPromise();
  }

  const auto& data = audio_data->data();
  media::AudioParameters params(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::ChannelLayoutConfig::Guess(data->channel_count()),
      data->sample_rate(), data->frame_count());
  if (!params.IsValid()) {
    audio_data->close();
    exception_state.ThrowDOMException(DOMExceptionCode::kOperationError,
                                      "Invalid audio data");
    return EmptyPromise();
  }

  base::TimeTicks estimated_capture_time;
  if (is_expose_page_relative_capture_time_enabled_) {
    estimated_capture_time =
        time_origin_ + base::Microseconds(audio_data->timestamp());
  }
  source_broker_->PushAudioData(audio_data->data(), estimated_capture_time);
  audio_data->close();

  return ToResolvedUndefinedPromise(script_state);
}

ScriptPromise<IDLUndefined> MediaStreamAudioTrackUnderlyingSink::abort(
    ScriptState* script_state,
    ScriptValue reason,
    ExceptionState& exception_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Disconnect();
  return ToResolvedUndefinedPromise(script_state);
}

ScriptPromise<IDLUndefined> MediaStreamAudioTrackUnderlyingSink::close(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Disconnect();
  return ToResolvedUndefinedPromise(script_state);
}

std::unique_ptr<WritableStreamTransferringOptimizer>
MediaStreamAudioTrackUnderlyingSink::GetTransferringOptimizer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return std::make_unique<TransferringOptimizer>(source_broker_);
}

void MediaStreamAudioTrackUnderlyingSink::Disconnect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!is_connected_)
    return;

  source_broker_->OnClientStopped();
  is_connected_ = false;
}

}  // namespace blink
