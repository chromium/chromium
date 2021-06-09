// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/breakout_box/frame_queue_underlying_source.h"

#include "base/bind_post_task.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/streams/readable_stream_default_controller_with_script_scope.h"
#include "third_party/blink/renderer/modules/webcodecs/audio_data.h"
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
      frame_queue_handle_(
          base::MakeRefCounted<FrameQueue<NativeFrameType>>(max_queue_size)) {}

template <typename NativeFrameType>
FrameQueueUnderlyingSource<NativeFrameType>::FrameQueueUnderlyingSource(
    ScriptState* script_state,
    FrameQueueUnderlyingSource<NativeFrameType>* other_source)
    : UnderlyingSourceBase(script_state),
      realm_task_runner_(ExecutionContext::From(script_state)
                             ->GetTaskRunner(TaskType::kInternalMediaRealTime)),
      frame_queue_handle_(other_source->frame_queue_handle_.Queue()) {}

template <typename NativeFrameType>
ScriptPromise FrameQueueUnderlyingSource<NativeFrameType>::pull(
    ScriptState* script_state) {
  DCHECK(realm_task_runner_->RunsTasksInCurrentSequence());
  auto frame_queue = frame_queue_handle_.Queue();
  if (!frame_queue)
    return ScriptPromise::CastUndefined(script_state);
  if (frame_queue->IsEmpty()) {
    MutexLocker locker(mutex_);
    is_pending_pull_ = true;
    return ScriptPromise::CastUndefined(script_state);
  }
  // Enqueuing the frame in the stream controller synchronously can lead to a
  // state where the JS code issuing and handling the read requests keeps
  // executing and prevents other tasks from executing. To avoid this, enqueue
  // the frame on another task. See https://crbug.com/1216445#c1
  realm_task_runner_->PostTask(
      FROM_HERE,
      WTF::Bind(&FrameQueueUnderlyingSource<
                    NativeFrameType>::MaybeSendFrameFromQueueToStream,
                WrapPersistent(this)));
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
  if (is_pending_close_) {
    realm_task_runner_->PostTask(
        FROM_HERE,
        WTF::Bind(&FrameQueueUnderlyingSource<NativeFrameType>::Close,
                  WrapWeakPersistent(this)));
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
bool FrameQueueUnderlyingSource<NativeFrameType>::HasPendingActivity() const {
  MutexLocker locker(mutex_);
  return is_pending_pull_ && Controller();
}

template <typename NativeFrameType>
void FrameQueueUnderlyingSource<NativeFrameType>::ContextDestroyed() {
  DCHECK(realm_task_runner_->RunsTasksInCurrentSequence());
  UnderlyingSourceBase::ContextDestroyed();
  frame_queue_handle_.Invalidate();
}

template <typename NativeFrameType>
wtf_size_t FrameQueueUnderlyingSource<NativeFrameType>::MaxQueueSize() const {
  auto queue = frame_queue_handle_.Queue();
  return queue ? queue->MaxSize() : 0;
}

template <typename NativeFrameType>
void FrameQueueUnderlyingSource<NativeFrameType>::Close() {
  DCHECK(realm_task_runner_->RunsTasksInCurrentSequence());
  if (is_closed_)
    return;

  // The source has not started. Postpone close until it starts.
  if (!Controller()) {
    is_pending_close_ = true;
    return;
  }

  StopFrameDelivery();
  CloseController();
  frame_queue_handle_.Invalidate();
  is_closed_ = true;
  {
    MutexLocker locker(mutex_);
    is_pending_pull_ = false;
    if (transferred_source_) {
      PostCrossThreadTask(
          *transferred_source_->GetRealmRunner(), FROM_HERE,
          CrossThreadBindOnce(
              &FrameQueueUnderlyingSource<NativeFrameType>::Close,
              WrapCrossThreadWeakPersistent(transferred_source_.Get())));
    }
    transferred_source_.Clear();
  }
}

template <typename NativeFrameType>
void FrameQueueUnderlyingSource<NativeFrameType>::QueueFrame(
    NativeFrameType media_frame) {
  bool should_send_frame_to_stream;
  {
    MutexLocker locker(mutex_);
    if (transferred_source_) {
      transferred_source_->QueueFrame(std::move(media_frame));
      return;
    }
    should_send_frame_to_stream = is_pending_pull_;
  }

  auto frame_queue = frame_queue_handle_.Queue();
  if (!frame_queue)
    return;

  frame_queue->Push(std::move(media_frame));
  if (should_send_frame_to_stream) {
    PostCrossThreadTask(
        *realm_task_runner_, FROM_HERE,
        CrossThreadBindOnce(
            &FrameQueueUnderlyingSource<
                NativeFrameType>::MaybeSendFrameFromQueueToStream,
            WrapCrossThreadPersistent(this)));
  }
}

template <typename NativeFrameType>
void FrameQueueUnderlyingSource<NativeFrameType>::Trace(
    Visitor* visitor) const {
  UnderlyingSourceBase::Trace(visitor);
}

template <typename NativeFrameType>
bool FrameQueueUnderlyingSource<NativeFrameType>::IsPendingPullForTesting()
    const {
  MutexLocker locker(mutex_);
  return is_pending_pull_;
}

template <typename NativeFrameType>
double FrameQueueUnderlyingSource<NativeFrameType>::DesiredSizeForTesting()
    const {
  DCHECK(realm_task_runner_->RunsTasksInCurrentSequence());
  return Controller()->DesiredSize();
}

template <typename NativeFrameType>
void FrameQueueUnderlyingSource<NativeFrameType>::TransferSource(
    FrameQueueUnderlyingSource<NativeFrameType>* transferred_source) {
  DCHECK(realm_task_runner_->RunsTasksInCurrentSequence());
  MutexLocker locker(mutex_);
  DCHECK(!transferred_source_);
  transferred_source_ = transferred_source;
  CloseController();
  frame_queue_handle_.Invalidate();
}

template <typename NativeFrameType>
void FrameQueueUnderlyingSource<NativeFrameType>::CloseController() {
  DCHECK(realm_task_runner_->RunsTasksInCurrentSequence());
  if (Controller())
    Controller()->Close();
}

template <typename NativeFrameType>
void FrameQueueUnderlyingSource<
    NativeFrameType>::MaybeSendFrameFromQueueToStream() {
  DCHECK(realm_task_runner_->RunsTasksInCurrentSequence());
  auto frame_queue = frame_queue_handle_.Queue();
  if (!frame_queue)
    return;

  absl::optional<NativeFrameType> media_frame = frame_queue->Pop();
  if (!media_frame.has_value())
    return;

  Controller()->Enqueue(MakeBlinkFrame(std::move(media_frame.value())));
  {
    MutexLocker locker(mutex_);
    is_pending_pull_ = false;
  }
}

template <>
ScriptWrappable*
FrameQueueUnderlyingSource<scoped_refptr<media::VideoFrame>>::MakeBlinkFrame(
    scoped_refptr<media::VideoFrame> media_frame) {
  DCHECK(realm_task_runner_->RunsTasksInCurrentSequence());
  return MakeGarbageCollected<VideoFrame>(std::move(media_frame),
                                          GetExecutionContext());
}

template <>
ScriptWrappable*
FrameQueueUnderlyingSource<scoped_refptr<media::AudioBuffer>>::MakeBlinkFrame(
    scoped_refptr<media::AudioBuffer> media_frame) {
  DCHECK(realm_task_runner_->RunsTasksInCurrentSequence());
  return MakeGarbageCollected<AudioData>(std::move(media_frame));
}

template class MODULES_TEMPLATE_EXPORT
    FrameQueueUnderlyingSource<scoped_refptr<media::AudioBuffer>>;
template class MODULES_TEMPLATE_EXPORT
    FrameQueueUnderlyingSource<scoped_refptr<media::VideoFrame>>;

}  // namespace blink
