// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/frame_queue_underlying_source.h"

#include "media/base/video_frame.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/streams/readable_stream_default_controller_with_script_scope.h"
#include "third_party/blink/renderer/modules/webcodecs/audio_frame.h"
#include "third_party/blink/renderer/modules/webcodecs/audio_frame_serialization_data.h"
#include "third_party/blink/renderer/modules/webcodecs/video_frame.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/webrtc/api/frame_transformer_interface.h"

namespace blink {

template <typename NativeFrameType>
FrameQueueUnderlyingSource<NativeFrameType>::FrameQueueUnderlyingSource(
    ScriptState* script_state,
    wtf_size_t max_queue_size)
    : UnderlyingSourceBase(script_state),
      realm_task_runner_(ExecutionContext::From(script_state)
                             ->GetTaskRunner(TaskType::kInternalMediaRealTime)),
      max_queue_size_(std::max(1u, max_queue_size)) {}

template <typename NativeFrameType>
ScriptPromise FrameQueueUnderlyingSource<NativeFrameType>::pull(
    ScriptState* script_state) {
  DCHECK(realm_task_runner_->RunsTasksInCurrentSequence());
  if (!queue_.empty()) {
    ProcessPullRequest();
  } else {
    is_pending_pull_ = true;
  }

  DCHECK_LT(queue_.size(), max_queue_size_);
  return ScriptPromise::CastUndefined(script_state);
}

template <typename NativeFrameType>
ScriptPromise FrameQueueUnderlyingSource<NativeFrameType>::Start(
    ScriptState* script_state) {
  DCHECK(realm_task_runner_->RunsTasksInCurrentSequence());
  if (!StartFrameDelivery()) {
    // There is only one way in which this can fail for now. Perhaps
    // implementations should return their own failure messages.
    return ScriptPromise::RejectWithDOMException(
        script_state,
        DOMException::Create(
            "Invalid track",
            DOMException::GetErrorName(DOMExceptionCode::kInvalidStateError)));
  }

  return ScriptPromise::CastUndefined(script_state);
}

template <typename NativeFrameType>
ScriptPromise FrameQueueUnderlyingSource<NativeFrameType>::Cancel(
    ScriptState* script_state,
    ScriptValue reason) {
  DCHECK(realm_task_runner_->RunsTasksInCurrentSequence());
  Close();
  return ScriptPromise::CastUndefined(script_state);
}

template <typename NativeFrameType>
void FrameQueueUnderlyingSource<NativeFrameType>::Close() {
  StopFrameDelivery();

  if (Controller())
    Controller()->Close();
  queue_.clear();
}

template <typename NativeFrameType>
void FrameQueueUnderlyingSource<NativeFrameType>::Trace(
    Visitor* visitor) const {
  UnderlyingSourceBase::Trace(visitor);
}

template <typename NativeFrameType>
double FrameQueueUnderlyingSource<NativeFrameType>::DesiredSizeForTesting()
    const {
  return Controller()->DesiredSize();
}

template <typename NativeFrameType>
void FrameQueueUnderlyingSource<NativeFrameType>::ContextDestroyed() {
  DCHECK(realm_task_runner_->RunsTasksInCurrentSequence());
  UnderlyingSourceBase::ContextDestroyed();
  queue_.clear();
}

template <typename NativeFrameType>
void FrameQueueUnderlyingSource<NativeFrameType>::QueueFrame(
    NativeFrameType media_frame) {
  if (realm_task_runner_->RunsTasksInCurrentSequence()) {
    QueueFrameOnRealmTaskRunner(std::move(media_frame));
    return;
  }

  PostCrossThreadTask(
      *realm_task_runner_, FROM_HERE,
      CrossThreadBindOnce(&FrameQueueUnderlyingSource<
                              NativeFrameType>::QueueFrameOnRealmTaskRunner,
                          WrapCrossThreadPersistent(this),
                          std::move(media_frame)));
}

template <typename NativeFrameType>
void FrameQueueUnderlyingSource<NativeFrameType>::QueueFrameOnRealmTaskRunner(
    NativeFrameType media_frame) {
  DCHECK(realm_task_runner_->RunsTasksInCurrentSequence());
  DCHECK_LE(queue_.size(), max_queue_size_);

  // The queue was stopped, and we shouldn't save frames.
  if (!Controller())
    return;

  // If the |queue_| is empty and the consumer has signaled a pull, bypass
  // |queue_| and send the frame directly to the stream controller.
  if (queue_.empty() && is_pending_pull_) {
    SendFrameToStream(std::move(media_frame));
    return;
  }

  if (queue_.size() == max_queue_size_)
    queue_.pop_front();

  queue_.push_back(std::move(media_frame));
  if (is_pending_pull_) {
    ProcessPullRequest();
  }
}

template <typename NativeFrameType>
void FrameQueueUnderlyingSource<NativeFrameType>::ProcessPullRequest() {
  DCHECK(!queue_.empty());
  SendFrameToStream(std::move(queue_.front()));
  queue_.pop_front();
}

template <typename NativeFrameType>
void FrameQueueUnderlyingSource<NativeFrameType>::SendFrameToStream(
    NativeFrameType media_frame) {
  DCHECK(Controller());
  DCHECK(media_frame);

  Controller()->Enqueue(MakeBlinkFrame(std::move(media_frame)));
  is_pending_pull_ = false;
}

template <>
ScriptWrappable*
FrameQueueUnderlyingSource<scoped_refptr<media::VideoFrame>>::MakeBlinkFrame(
    scoped_refptr<media::VideoFrame> media_frame) {
  DCHECK(realm_task_runner_->RunsTasksInCurrentSequence());
  VideoFrame* video_frame = MakeGarbageCollected<VideoFrame>(
      std::move(media_frame), GetExecutionContext());

  if (stream_was_transferred_)
    video_frame->handle()->SetCloseOnClone();

  return video_frame;
}

template <>
ScriptWrappable*
FrameQueueUnderlyingSource<std::unique_ptr<AudioFrameSerializationData>>::
    MakeBlinkFrame(std::unique_ptr<AudioFrameSerializationData> media_frame) {
  DCHECK(realm_task_runner_->RunsTasksInCurrentSequence());
  return MakeGarbageCollected<AudioFrame>(std::move(media_frame));
}

template class MODULES_TEMPLATE_EXPORT
    FrameQueueUnderlyingSource<std::unique_ptr<AudioFrameSerializationData>>;
template class MODULES_TEMPLATE_EXPORT
    FrameQueueUnderlyingSource<scoped_refptr<media::VideoFrame>>;

}  // namespace blink
