// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/media_stream_audio_track_underlying_source.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/streams/readable_stream_default_controller_with_script_scope.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_track.h"
#include "third_party/blink/renderer/modules/webcodecs/audio_frame_serialization_data.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_track.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/webrtc/api/frame_transformer_interface.h"

namespace blink {

MediaStreamAudioTrackUnderlyingSource::MediaStreamAudioTrackUnderlyingSource(
    ScriptState* script_state,
    MediaStreamComponent* track,
    wtf_size_t max_queue_size)
    : UnderlyingSourceBase(script_state),
      main_task_runner_(ExecutionContext::From(script_state)
                            ->GetTaskRunner(TaskType::kInternalMediaRealTime)),
      track_(track),
      max_queue_size_(std::max(1u, max_queue_size)) {
  DCHECK(track_);
}

ScriptPromise MediaStreamAudioTrackUnderlyingSource::pull(
    ScriptState* script_state) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  if (!queue_.empty()) {
    ProcessPullRequest();
  } else {
    is_pending_pull_ = true;
  }

  DCHECK_LT(queue_.size(), max_queue_size_);
  return ScriptPromise::CastUndefined(script_state);
}

ScriptPromise MediaStreamAudioTrackUnderlyingSource::Start(
    ScriptState* script_state) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  MediaStreamAudioTrack* audio_track = MediaStreamAudioTrack::From(track_);
  if (!audio_track) {
    return ScriptPromise::RejectWithDOMException(
        script_state,
        DOMException::Create(
            "No input track",
            DOMException::GetErrorName(DOMExceptionCode::kInvalidStateError)));
  }
  WebMediaStreamAudioSink::AddToAudioTrack(this, WebMediaStreamTrack(track_));

  return ScriptPromise::CastUndefined(script_state);
}

ScriptPromise MediaStreamAudioTrackUnderlyingSource::Cancel(
    ScriptState* script_state,
    ScriptValue reason) {
  DisconnectFromTrack();
  return ScriptPromise::CastUndefined(script_state);
}

void MediaStreamAudioTrackUnderlyingSource::DisconnectFromTrack() {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  if (!track_)
    return;

  WebMediaStreamAudioSink::RemoveFromAudioTrack(this,
                                                WebMediaStreamTrack(track_));

  track_.Clear();
}

void MediaStreamAudioTrackUnderlyingSource::Trace(Visitor* visitor) const {
  visitor->Trace(track_);
  UnderlyingSourceBase::Trace(visitor);
}

double MediaStreamAudioTrackUnderlyingSource::DesiredSizeForTesting() const {
  return Controller()->DesiredSize();
}

void MediaStreamAudioTrackUnderlyingSource::Close() {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  DisconnectFromTrack();

  // Check for Controller(), as the context might have been destroyed.
  if (Controller())
    Controller()->Close();
  queue_.clear();
}

void MediaStreamAudioTrackUnderlyingSource::OnData(
    const media::AudioBus& audio_bus,
    base::TimeTicks estimated_capture_time) {
  DCHECK(audio_parameters_.IsValid());

  auto data_copy =
      media::AudioBus::Create(audio_bus.channels(), audio_bus.frames());
  audio_bus.CopyTo(data_copy.get());

  auto queue_data = AudioFrameSerializationData::Wrap(
      std::move(data_copy), audio_parameters_.sample_rate(),
      estimated_capture_time - base::TimeTicks());

  PostCrossThreadTask(
      *main_task_runner_, FROM_HERE,
      CrossThreadBindOnce(
          &MediaStreamAudioTrackUnderlyingSource::OnDataOnMainThread,
          WrapCrossThreadPersistent(this), std::move(queue_data)));
}

void MediaStreamAudioTrackUnderlyingSource::OnDataOnMainThread(
    std::unique_ptr<AudioFrameSerializationData> queue_data) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  DCHECK_LE(queue_.size(), max_queue_size_);

  // If the |queue_| is empty and the consumer has signaled a pull, bypass
  // |queue_| and send the frame directly to the stream controller.
  if (queue_.empty() && is_pending_pull_) {
    SendFrameToStream(std::move(queue_data));
    return;
  }

  if (queue_.size() == max_queue_size_)
    queue_.pop_front();

  queue_.emplace_back(std::move(queue_data));
  if (is_pending_pull_) {
    ProcessPullRequest();
  }
}

void MediaStreamAudioTrackUnderlyingSource::OnSetFormat(
    const media::AudioParameters& params) {
  DCHECK(params.IsValid());
  audio_parameters_ = params;
}

void MediaStreamAudioTrackUnderlyingSource::ProcessPullRequest() {
  DCHECK(!queue_.empty());
  SendFrameToStream(std::move(queue_.front()));
  queue_.pop_front();
}

void MediaStreamAudioTrackUnderlyingSource::SendFrameToStream(
    std::unique_ptr<AudioFrameSerializationData> queue_data) {
  DCHECK(Controller());
  AudioFrame* audio_frame =
      MakeGarbageCollected<AudioFrame>(std::move(queue_data));
  Controller()->Enqueue(audio_frame);
  is_pending_pull_ = false;
}

}  // namespace blink
