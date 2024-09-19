// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/breakout_box/frame_queue_underlying_source.h"

#include "base/feature_list.h"
#include "base/task/bind_post_task.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/streams/readable_stream_default_controller_with_script_scope.h"
#include "third_party/blink/renderer/modules/webcodecs/audio_data.h"
#include "third_party/blink/renderer/modules/webcodecs/video_frame.h"
#include "third_party/blink/renderer/modules/webcodecs/video_frame_monitor.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/webrtc/api/frame_transformer_interface.h"

namespace blink {

BASE_FEATURE(kBreakoutBoxEnqueueInSeparateTask,
             "BreakoutBoxEnqueueInSeparateTask",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kBreakoutBoxPreferCaptureTimestampInVideoFrames,
             "BreakoutBoxPreferCaptureTimestampInVideoFrames",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kBreakoutBoxInsertVideoCaptureTimestamp,
             "BreakoutBoxInsertVideoCaptureTimestamp",
             base::FEATURE_DISABLED_BY_DEFAULT);

namespace {

media::VideoFrame::ID GetFrameId(
    const scoped_refptr<media::VideoFrame>& video_frame) {
  return video_frame->unique_id();
}

media::VideoFrame::ID GetFrameId(const scoped_refptr<media::AudioBuffer>&) {
  NOTREACHED_IN_MIGRATION();
  return media::VideoFrame::ID();
}

}  // namespace

template <typename NativeFrameType>
FrameQueueUnderlyingSource<NativeFrameType>::FrameQueueUnderlyingSource(
    ScriptState* script_state,
    wtf_size_t max_queue_size,
    std::string device_id,
    wtf_size_t frame_pool_size)
    : UnderlyingSourceBase(script_state),
      realm_task_runner_(ExecutionContext::From(script_state)
                             ->GetTaskRunner(TaskType::kInternalMediaRealTime)),
      frame_queue_handle_(
          base::MakeRefCounted<FrameQueue<NativeFrameType>>(max_queue_size)),
      device_id_(std::move(device_id)),
      frame_pool_size_(frame_pool_size) {
  DCHECK(device_id_.empty() || frame_pool_size_ > 0);
}

template <typename NativeFrameType>
FrameQueueUnderlyingSource<NativeFrameType>::FrameQueueUnderlyingSource(
    ScriptState* script_state,
    wtf_size_t max_queue_size)
    : FrameQueueUnderlyingSource(script_state,
                                 max_queue_size,
                                 std::string(),
                                 /*frame_pool_size=*/0) {}

template <typename NativeFrameType>
FrameQueueUnderlyingSource<NativeFrameType>::FrameQueueUnderlyingSource(
    ScriptState* script_state,
    FrameQueueUnderlyingSource<NativeFrameType>* other_source)
    : UnderlyingSourceBase(script_state),
      realm_task_runner_(ExecutionContext::From(script_state)
                             ->GetTaskRunner(TaskType::kInternalMediaRealTime)),
      frame_queue_handle_(other_source->frame_queue_handle_.Queue()),
      device_id_(other_source->device_id_),
      frame_pool_size_(other_source->frame_pool_size_) {
  DCHECK(device_id_.empty() || frame_pool_size_ > 0);
}

template <typename NativeFrameType>
ScriptPromiseUntyped FrameQueueUnderlyingSource<NativeFrameType>::Pull(
    ScriptState* script_state,
    ExceptionState&) {
  DCHECK(realm_task_runner_->RunsTasksInCurrentSequence());
  {
    base::AutoLock locker(lock_);
    num_pending_pulls_++;
  }
  auto frame_queue = frame_queue_handle_.Queue();
  if (!frame_queue)
    return ToResolvedUndefinedPromise(script_state);

  if (!frame_queue->IsEmpty()) {
    // Enqueuing the frame in the stream controller synchronously can lead to a
    // state where the JS code issuing and handling the read requests keeps
    // executing and prevents other tasks from executing. To avoid this, enqueue
    // the frame on another task. See https://crbug.com/1216445#c1
    realm_task_runner_->PostTask(
        FROM_HERE,
        WTF::BindOnce(&FrameQueueUnderlyingSource<
                          NativeFrameType>::MaybeSendFrameFromQueueToStream,
                      WrapPersistent(this)));
  }
  return ToResolvedUndefinedPromise(script_state);
}

template <typename NativeFrameType>
ScriptPromiseUntyped FrameQueueUnderlyingSource<NativeFrameType>::Start(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  DCHECK(realm_task_runner_->RunsTasksInCurrentSequence());
  if (is_closed_) {
    // This was intended to be closed before Start() was called.
    CloseController();
  } else {
    if (!StartFrameDelivery()) {
      // There is only one way in which this can fail for now. Perhaps
      // implementations should return their own failure messages.
      exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                        "Invalid track");
      return ScriptPromiseUntyped();
    }
  }

  return ToResolvedUndefinedPromise(script_state);
}

template <typename NativeFrameType>
ScriptPromiseUntyped FrameQueueUnderlyingSource<NativeFrameType>::Cancel(
    ScriptState* script_state,
    ScriptValue reason,
    ExceptionState&) {
  DCHECK(realm_task_runner_->RunsTasksInCurrentSequence());
  Close();
  return ToResolvedUndefinedPromise(script_state);
}

template <typename NativeFrameType>
void FrameQueueUnderlyingSource<NativeFrameType>::ContextDestroyed() {
  DCHECK(realm_task_runner_->RunsTasksInCurrentSequence());
  Close();
  UnderlyingSourceBase::ContextDestroyed();
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

  is_closed_ = true;
  if (GetExecutionContext()) {
    StopFrameDelivery();
    CloseController();
  }
  bool should_clear_queue = true;
  {
    base::AutoLock locker(lock_);
    num_pending_pulls_ = 0;
    if (transferred_source_) {
      PostCrossThreadTask(
          *transferred_source_->GetRealmRunner(), FROM_HERE,
          CrossThreadBindOnce(
              &FrameQueueUnderlyingSource<NativeFrameType>::Close,
              WrapCrossThreadWeakPersistent(transferred_source_.Get())));
      // The queue will be cleared by |transferred_source_|.
      should_clear_queue = false;
    }
    transferred_source_.Clear();
  }
  auto frame_queue = frame_queue_handle_.Queue();
  if (frame_queue && should_clear_queue && MustUseMonitor()) {
    while (!frame_queue->IsEmpty()) {
      std::optional<NativeFrameType> popped_frame = frame_queue->Pop();
      base::AutoLock monitor_locker(GetMonitorLock());
      MonitorPopFrameLocked(popped_frame.value());
    }
  }
  // Invalidating will clear the queue in the non-monitoring case if there is
  // no transferred source.
  frame_queue_handle_.Invalidate();
}

template <typename NativeFrameType>
void FrameQueueUnderlyingSource<NativeFrameType>::QueueFrame(
    NativeFrameType media_frame) {
  bool should_send_frame_to_stream;
  {
    base::AutoLock locker(lock_);
    if (transferred_source_) {
      transferred_source_->QueueFrame(std::move(media_frame));
      return;
    }
    should_send_frame_to_stream = num_pending_pulls_ > 0;
  }

  auto frame_queue = frame_queue_handle_.Queue();
  if (!frame_queue)
    return;

  if (MustUseMonitor()) {
    base::AutoLock queue_locker(frame_queue->GetLock());
    base::AutoLock monitor_locker(GetMonitorLock());
    std::optional<NativeFrameType> oldest_frame = frame_queue->PeekLocked();
    NewFrameAction action = AnalyzeNewFrameLocked(media_frame, oldest_frame);
    switch (action) {
      case NewFrameAction::kPush: {
        MonitorPushFrameLocked(media_frame);
        std::optional<NativeFrameType> replaced_frame =
            frame_queue->PushLocked(std::move(media_frame));
        if (replaced_frame.has_value())
          MonitorPopFrameLocked(replaced_frame.value());
        break;
      }
      case NewFrameAction::kReplace:
        MonitorPushFrameLocked(media_frame);
        if (oldest_frame.has_value())
          MonitorPopFrameLocked(oldest_frame.value());
        // Explicitly pop the old frame and push the new one since the
        // |frame_pool_size_| limit has been reached and it may be smaller
        // than the maximum size of |frame_queue|.
        frame_queue->PopLocked();
        frame_queue->PushLocked(std::move(media_frame));
        break;
      case NewFrameAction::kDrop:
        // Drop |media_frame| by retuning without doing anything with it.
        return;
    }
  } else {
    frame_queue->Push(std::move(media_frame));
  }
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
int FrameQueueUnderlyingSource<NativeFrameType>::NumPendingPullsForTesting()
    const {
  base::AutoLock locker(lock_);
  return num_pending_pulls_;
}

template <typename NativeFrameType>
double FrameQueueUnderlyingSource<NativeFrameType>::DesiredSizeForTesting()
    const {
  DCHECK(realm_task_runner_->RunsTasksInCurrentSequence());
  return Controller()->DesiredSize();
}

template <typename NativeFrameType>
void FrameQueueUnderlyingSource<NativeFrameType>::TransferSource(
    CrossThreadPersistent<FrameQueueUnderlyingSource<NativeFrameType>>
        transferred_source) {
  DCHECK(realm_task_runner_->RunsTasksInCurrentSequence());
  base::AutoLock locker(lock_);
  DCHECK(!transferred_source_);
  transferred_source_ = std::move(transferred_source);
  CloseController();
  frame_queue_handle_.Invalidate();
}

template <typename NativeFrameType>
void FrameQueueUnderlyingSource<NativeFrameType>::ClearTransferredSource() {
  base::AutoLock locker(lock_);
  transferred_source_.Clear();
}

template <typename NativeFrameType>
void FrameQueueUnderlyingSource<NativeFrameType>::CloseController() {
  DCHECK(realm_task_runner_->RunsTasksInCurrentSequence());
  // This can be called during stream construction while Controller() is still
  // false.
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

  {
    base::AutoLock locker(lock_);
    if (num_pending_pulls_ == 0)
      return;
  }
  while (true) {
    std::optional<NativeFrameType> media_frame = frame_queue->Pop();
    if (!media_frame.has_value())
      return;

    media::VideoFrame::ID frame_id = MustUseMonitor()
                                         ? GetFrameId(media_frame.value())
                                         : media::VideoFrame::ID();
    if (base::FeatureList::IsEnabled(kBreakoutBoxEnqueueInSeparateTask)) {
      // It has been observed that if the time between JS read() operations
      // is longer than the time between new frames, other tasks get delayed
      // and the page freezes. Enqueuing in a separate task avoids this problem.
      // See https://crbug.com/1490501
      realm_task_runner_->PostTask(
          FROM_HERE,
          WTF::BindOnce(
              &FrameQueueUnderlyingSource::EnqueueBlinkFrame,
              WrapPersistent(this),
              WrapPersistent(MakeBlinkFrame(std::move(media_frame.value())))));
    } else {
      Controller()->Enqueue(MakeBlinkFrame(std::move(media_frame.value())));
    }
    // Update the monitor after creating the Blink VideoFrame to avoid
    // temporarily removing the frame from the monitor.
    MaybeMonitorPopFrameId(frame_id);
    {
      base::AutoLock locker(lock_);
      if (--num_pending_pulls_ == 0)
        return;
    }
  }
}

template <typename NativeFrameType>
void FrameQueueUnderlyingSource<NativeFrameType>::EnqueueBlinkFrame(
    ScriptWrappable* blink_frame) const {
  DCHECK(realm_task_runner_->RunsTasksInCurrentSequence());
  if (GetExecutionContext() && !GetExecutionContext()->IsContextDestroyed()) {
    Controller()->Enqueue(blink_frame);
  }
}

template <typename NativeFrameType>
bool FrameQueueUnderlyingSource<NativeFrameType>::MustUseMonitor() const {
  return !device_id_.empty();
}

template <typename NativeFrameType>
base::Lock& FrameQueueUnderlyingSource<NativeFrameType>::GetMonitorLock() {
  DCHECK(MustUseMonitor());
  return VideoFrameMonitor::Instance().GetLock();
}

template <typename NativeFrameType>
void FrameQueueUnderlyingSource<NativeFrameType>::MaybeMonitorPopFrameId(
    media::VideoFrame::ID frame_id) {
  if (!MustUseMonitor())
    return;
  VideoFrameMonitor::Instance().OnCloseFrame(device_id_, frame_id);
}

template <typename NativeFrameType>
void FrameQueueUnderlyingSource<NativeFrameType>::MonitorPopFrameLocked(
    const NativeFrameType& media_frame) {
  DCHECK(MustUseMonitor());
  media::VideoFrame::ID frame_id = GetFrameId(media_frame);
  // Note: This is GetMonitorLock(), which is required, but the static checker
  // doesn't figure it out.
  VideoFrameMonitor::Instance().GetLock().AssertAcquired();
  VideoFrameMonitor::Instance().OnCloseFrameLocked(device_id_, frame_id);
}

template <typename NativeFrameType>
void FrameQueueUnderlyingSource<NativeFrameType>::MonitorPushFrameLocked(
    const NativeFrameType& media_frame) {
  DCHECK(MustUseMonitor());
  media::VideoFrame::ID frame_id = GetFrameId(media_frame);
  VideoFrameMonitor::Instance().GetLock().AssertAcquired();
  VideoFrameMonitor::Instance().OnOpenFrameLocked(device_id_, frame_id);
}

template <typename NativeFrameType>
typename FrameQueueUnderlyingSource<NativeFrameType>::NewFrameAction
FrameQueueUnderlyingSource<NativeFrameType>::AnalyzeNewFrameLocked(
    const NativeFrameType& new_frame,
    const std::optional<NativeFrameType>& oldest_frame) {
  DCHECK(MustUseMonitor());
  std::optional<media::VideoFrame::ID> oldest_frame_id;
  if (oldest_frame.has_value())
    oldest_frame_id = GetFrameId(oldest_frame.value());

  VideoFrameMonitor& monitor = VideoFrameMonitor::Instance();
  monitor.GetLock().AssertAcquired();
  wtf_size_t num_total_frames = monitor.NumFramesLocked(device_id_);
  if (num_total_frames < frame_pool_size_) {
    // The limit is not reached yet.
    return NewFrameAction::kPush;
  }

  media::VideoFrame::ID new_frame_id = GetFrameId(new_frame);
  if (monitor.NumRefsLocked(device_id_, new_frame_id) > 0) {
    // The new frame is already in another queue or exposed to JS, so adding
    // it to the queue would not count against the limit.
    return NewFrameAction::kPush;
  }

  if (!oldest_frame_id.has_value()) {
    // The limit has been reached and there is nothing that can be replaced.
    return NewFrameAction::kDrop;
  }

  if (monitor.NumRefsLocked(device_id_, oldest_frame_id.value()) == 1) {
    // The frame pool size limit has been reached. However, we can safely
    // replace the oldest frame in our queue, since it is not referenced
    // elsewhere.
    return NewFrameAction::kReplace;
  }

  return NewFrameAction::kDrop;
}

template <>
ScriptWrappable*
FrameQueueUnderlyingSource<scoped_refptr<media::VideoFrame>>::MakeBlinkFrame(
    scoped_refptr<media::VideoFrame> media_frame) {
  DCHECK(realm_task_runner_->RunsTasksInCurrentSequence());
  if (base::FeatureList::IsEnabled(kBreakoutBoxInsertVideoCaptureTimestamp)) {
    if (!first_frame_ticks_) {
      first_frame_ticks_ = base::TimeTicks::Now() - media_frame->timestamp();
    }

    if (!media_frame->metadata().capture_begin_time &&
        !media_frame->metadata().reference_time) {
      media_frame->metadata().capture_begin_time =
          *first_frame_ticks_ + media_frame->timestamp();
      media_frame->metadata().reference_time =
          *first_frame_ticks_ + media_frame->timestamp();
    }
  }
  TRACE_EVENT(
      "media", "FrameQueueUnderlyingSource::MakeBlinkFrame", "ts",
      media_frame->timestamp(), "rt",
      media_frame->metadata().reference_time.value_or(base::TimeTicks()), "cbt",
      media_frame->metadata().capture_begin_time.value_or(base::TimeTicks()));
  return MakeGarbageCollected<VideoFrame>(
      std::move(media_frame), GetExecutionContext(), device_id_,
      /*sk_image=*/nullptr,
      /*prefer_capture_timestamp=*/
      base::FeatureList::IsEnabled(
          kBreakoutBoxPreferCaptureTimestampInVideoFrames));
}

template <>
ScriptWrappable*
FrameQueueUnderlyingSource<scoped_refptr<media::AudioBuffer>>::MakeBlinkFrame(
    scoped_refptr<media::AudioBuffer> media_frame) {
  DCHECK(realm_task_runner_->RunsTasksInCurrentSequence());
  return MakeGarbageCollected<AudioData>(std::move(media_frame));
}

template <>
bool FrameQueueUnderlyingSource<
    scoped_refptr<media::AudioBuffer>>::MustUseMonitor() const {
  return false;
}

template class MODULES_TEMPLATE_EXPORT
    FrameQueueUnderlyingSource<scoped_refptr<media::AudioBuffer>>;
template class MODULES_TEMPLATE_EXPORT
    FrameQueueUnderlyingSource<scoped_refptr<media::VideoFrame>>;

}  // namespace blink
