// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/frame_queue_underlying_source.h"

#include "base/bind_post_task.h"
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
void FrameQueueUnderlyingSource<NativeFrameType>::CloseController() {
  if (Controller())
    Controller()->Close();
}

template <typename NativeFrameType>
void FrameQueueUnderlyingSource<NativeFrameType>::Close() {
  DCHECK(realm_task_runner_->RunsTasksInCurrentSequence());
  if (is_closed_)
    return;

  StopFrameDelivery();
  CloseController();
  queue_.clear();
  pending_transfer_queue_.clear();
  transfer_frames_cb_.Reset();
  transfer_done_cb_.Reset();
  is_pending_pull_ = false;

  is_closed_ = true;
}

template <typename NativeFrameType>
bool FrameQueueUnderlyingSource<NativeFrameType>::HasPendingActivity() const {
  return is_pending_pull_ && Controller();
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
  DCHECK(!queue_transferred_);

  // We don't bother to acquire a lock to check |transfer_frames_cb_|.
  // It should be set on |realm_task_runner_| (at which point it's
  // still fine to call QueueFrameOnRealmTaskRunnder()), and unset on
  // |transfer_task_runner_|
  if (transfer_frames_cb_) {
    DCHECK(transfer_task_runner_->RunsTasksInCurrentSequence());
    pending_transfer_queue_.push_back(std::move(media_frame));
    return;
  }

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
  DCHECK(!queue_transferred_);
  DCHECK(realm_task_runner_->RunsTasksInCurrentSequence());
  DCHECK_LE(queue_.size(), max_queue_size_);

  if (transfer_frames_cb_) {
    // We are currently transferring this frame queue. This frame should be
    // immediately sent, because older frames (the ones in |queue_|) are
    // already transferred, and new frames are saved in
    // |pending_transfer_queue_|.
    DCHECK(queue_.empty());
    transfer_frames_cb_.Run(std::move(media_frame));
    return;
  }

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
void FrameQueueUnderlyingSource<NativeFrameType>::TransferQueueFromRealmRunner(
    TransferFramesCB transfer_frames_cb,
    scoped_refptr<base::SequencedTaskRunner> transfer_task_runner,
    CrossThreadOnceClosure transfer_done_cb) {
  DCHECK(!queue_transferred_);
  DCHECK(realm_task_runner_->RunsTasksInCurrentSequence());

  // QueueFrame() will stop posting frames to QueueFrameOnRealmTaskRunner().
  // New frames will be saved in |pending_transfer_queue_| instead.
  transfer_frames_cb_ = std::move(transfer_frames_cb);
  transfer_done_cb_ = std::move(transfer_done_cb);
  transfer_task_runner_ = std::move(transfer_task_runner);

  // All current frames should be send immediately, as they are the oldest.
  while (!queue_.empty()) {
    transfer_frames_cb_.Run(std::move(queue_.front()));
    queue_.pop_front();
  }

  // Make sure that all in-flight calls to QueueFrameOnRealmTaskRunner()
  // settle.
  EnsureAllRealmRunnerFramesProcessed();
}

template <typename NativeFrameType>
void FrameQueueUnderlyingSource<
    NativeFrameType>::EnsureAllRealmRunnerFramesProcessed() {
  DCHECK(realm_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(!queue_transferred_);
  DCHECK(queue_.empty());

  auto frame_barrier_done_cb = ConvertToBaseOnceCallback(
      CrossThreadBindOnce(&FrameQueueUnderlyingSource<
                              NativeFrameType>::OnAllRealmRunnerFrameProcessed,
                          WrapCrossThreadPersistent(this)));

  // Send |frame_barrier_done_cb| to |transfer_task_runner_| and back,
  // acting as a "barrier" for pending QueueFrameOnRealmTaskRunner() calls.
  transfer_task_runner_->PostTask(
      FROM_HERE,
      BindPostTask(realm_task_runner_, std::move(frame_barrier_done_cb)));
}

template <typename NativeFrameType>
void FrameQueueUnderlyingSource<
    NativeFrameType>::OnAllRealmRunnerFrameProcessed() {
  DCHECK(realm_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(!queue_transferred_);
  DCHECK(queue_.empty());

  // We can now start queueing frames directly on the transferred source.
  CloseController();

  // Unset this flag, as to not keep |this| alive through HasPendingActivity().
  is_pending_pull_ = false;

  PostCrossThreadTask(
      *transfer_task_runner_.get(), FROM_HERE,
      CrossThreadBindOnce(
          &FrameQueueUnderlyingSource<
              NativeFrameType>::FinalizeQueueTransferOnTransferRunner,
          WrapCrossThreadPersistent(this)));
}

template <typename NativeFrameType>
void FrameQueueUnderlyingSource<
    NativeFrameType>::FinalizeQueueTransferOnTransferRunner() {
  DCHECK(transfer_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(!queue_transferred_);
  DCHECK(queue_.empty());

  // All frames that were bound to the |realm_task_runner_| have been
  // transferred. We can transfer our temporary frames without fear of changing
  // the ordering of frames.
  while (!pending_transfer_queue_.empty()) {
    transfer_frames_cb_.Run(std::move(pending_transfer_queue_.front()));
    pending_transfer_queue_.pop_front();
  }

  queue_transferred_ = true;
  transfer_task_runner_.reset();
  transfer_frames_cb_.Reset();
  std::move(transfer_done_cb_).Run();
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
  return MakeGarbageCollected<VideoFrame>(std::move(media_frame),
                                          GetExecutionContext());
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
