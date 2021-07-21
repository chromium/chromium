// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/breakout_box/video_track_signal_underlying_source.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_stream_track_signal.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/streams/readable_stream_default_controller_with_script_scope.h"
#include "third_party/blink/renderer/modules/breakout_box/media_stream_track_generator.h"
#include "third_party/blink/renderer/modules/breakout_box/metrics.h"
#include "third_party/blink/renderer/modules/breakout_box/pushable_media_stream_video_source.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_track.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_video_track.h"
#include "third_party/blink/renderer/modules/webcodecs/video_frame.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/webrtc/api/frame_transformer_interface.h"

namespace blink {

VideoTrackSignalUnderlyingSource::VideoTrackSignalUnderlyingSource(
    ScriptState* script_state,
    MediaStreamTrackGenerator* generator,
    wtf_size_t max_queue_size)
    : UnderlyingSourceBase(script_state),
      main_task_runner_(ExecutionContext::From(script_state)
                            ->GetTaskRunner(TaskType::kInternalMediaRealTime)),
      generator_(generator),
      max_queue_size_(std::max(1u, max_queue_size)) {
  DCHECK(generator_);
  RecordBreakoutBoxUsage(BreakoutBoxUsage::kReadableControlVideo);
}

ScriptPromise VideoTrackSignalUnderlyingSource::pull(
    ScriptState* script_state) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  if (!queue_.empty()) {
    PullFromQueue();
  } else {
    is_pending_pull_ = true;
  }

  DCHECK_LT(queue_.size(), max_queue_size_);
  return ScriptPromise::CastUndefined(script_state);
}

ScriptPromise VideoTrackSignalUnderlyingSource::Start(
    ScriptState* script_state) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  auto* video_track = MediaStreamVideoTrack::From(generator_->Component());
  if (!video_track) {
    return ScriptPromise::RejectWithDOMException(
        script_state,
        DOMException::Create(
            "No input track",
            DOMException::GetErrorName(DOMExceptionCode::kInvalidStateError)));
  }
  video_track->SetSignalObserver(this);

  auto* video_source = generator_->PushableVideoSource();
  video_source->SetSignalObserver(this);
  return ScriptPromise::CastUndefined(script_state);
}

ScriptPromise VideoTrackSignalUnderlyingSource::Cancel(
    ScriptState* script_state,
    ScriptValue reason) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  Stop();
  return ScriptPromise::CastUndefined(script_state);
}

void VideoTrackSignalUnderlyingSource::Trace(Visitor* visitor) const {
  visitor->Trace(generator_);
  visitor->Trace(queue_);
  UnderlyingSourceBase::Trace(visitor);
}

double VideoTrackSignalUnderlyingSource::DesiredSizeForTesting() const {
  return Controller()->DesiredSize();
}

void VideoTrackSignalUnderlyingSource::ContextDestroyed() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  UnderlyingSourceBase::ContextDestroyed();
  Stop();
}

void VideoTrackSignalUnderlyingSource::Close() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  Stop();
  if (Controller())
    Controller()->Close();
}

void VideoTrackSignalUnderlyingSource::SetMinimumFrameRate(double frame_rate) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  MediaStreamTrackSignal* signal = MediaStreamTrackSignal::Create();
  signal->setSignalType("set-min-frame-rate");
  signal->setFrameRate(frame_rate);
  ProcessNewSignal(signal);
}

void VideoTrackSignalUnderlyingSource::RequestFrame() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  MediaStreamTrackSignal* signal = MediaStreamTrackSignal::Create();
  signal->setSignalType("request-frame");
  ProcessNewSignal(signal);
}

void VideoTrackSignalUnderlyingSource::ProcessNewSignal(
    MediaStreamTrackSignal* signal) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DCHECK_LE(queue_.size(), max_queue_size_);
  if (!is_active_)
    return;

  // If the |queue_| is empty and the consumer has signaled a pull, bypass
  // |queue_| and send the frame directly to the stream controller.
  if (queue_.empty() && is_pending_pull_) {
    SendSignalToStream(signal);
    return;
  }

  if (queue_.size() == max_queue_size_)
    queue_.pop_front();

  queue_.push_back(signal);
  if (is_pending_pull_) {
    PullFromQueue();
  }
}

void VideoTrackSignalUnderlyingSource::PullFromQueue() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DCHECK(!queue_.empty());
  DCHECK(is_active_);
  SendSignalToStream(std::move(queue_.front()));
  queue_.pop_front();
}

void VideoTrackSignalUnderlyingSource::SendSignalToStream(
    MediaStreamTrackSignal* signal) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DCHECK(signal);
  DCHECK(is_active_);
  if (!Controller())
    return;

  Controller()->Enqueue(signal);
  is_pending_pull_ = false;
}

void VideoTrackSignalUnderlyingSource::Stop() {
  is_active_ = false;
  queue_.clear();
}

}  // namespace blink
