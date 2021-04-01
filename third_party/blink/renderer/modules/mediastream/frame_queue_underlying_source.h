// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_FRAME_QUEUE_UNDERLYING_SOURCE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_FRAME_QUEUE_UNDERLYING_SOURCE_H_

#include "base/threading/thread_checker.h"
#include "third_party/blink/renderer/core/streams/underlying_source_base.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/wtf/deque.h"

namespace blink {

class AudioFrameSerializationData;

template <typename NativeFrameType>
class FrameQueueUnderlyingSource
    : public UnderlyingSourceBase,
      public ActiveScriptWrappable<
          FrameQueueUnderlyingSource<NativeFrameType>> {
 public:
  using TransferFramesCB = CrossThreadFunction<void(NativeFrameType)>;

  FrameQueueUnderlyingSource(ScriptState*, wtf_size_t queue_size);
  ~FrameQueueUnderlyingSource() override = default;

  FrameQueueUnderlyingSource(const FrameQueueUnderlyingSource&) = delete;
  FrameQueueUnderlyingSource& operator=(const FrameQueueUnderlyingSource&) =
      delete;

  // UnderlyingSourceBase
  ScriptPromise pull(ScriptState*) override;
  ScriptPromise Start(ScriptState*) override;
  ScriptPromise Cancel(ScriptState*, ScriptValue reason) override;

  // ScriptWrappable interface
  bool HasPendingActivity() const final;

  // ExecutionLifecycleObserver
  void ContextDestroyed() override;

  wtf_size_t MaxQueueSize() const { return max_queue_size_; }

  // Clears all internal state and closes the UnderlyingSource's Controller.
  // Must be called on |realm_task_runner_|.
  void Close();

  bool IsClosed() { return is_closed_; }

  // Adds a frame to |queue_|, dropping the oldest frame if it is full.
  // Can be called from any task runner, and will jump to |realm_task_runner_|.
  void QueueFrame(NativeFrameType media_frame);

  // Start or stop the delivery of frames via QueueFrame().
  // Must be called on |realm_task_runner_|.
  virtual bool StartFrameDelivery() = 0;
  virtual void StopFrameDelivery() = 0;

  bool IsPendingPullForTesting() const { return is_pending_pull_; }
  const Deque<NativeFrameType>& QueueForTesting() const { return queue_; }
  double DesiredSizeForTesting() const;

  void Trace(Visitor*) const override;

 protected:
  scoped_refptr<base::SequencedTaskRunner> GetRealmRunner() {
    return realm_task_runner_;
  }

  // Starts transferring frames via |transfer_frames_cb|, guaranteeing frame
  // ordering between frames received on |realm_task_runner_| and
  // |transfer_task_runner|. QueueFrames() can still called until
  // |transfer_done_cb| runs, at which point QueueFrames() should never be
  // called again.
  //
  // Note:
  // - This must be called on |realm_task_runner_|.
  // - |transfer_done_cb| will be run on |transfer_task_runner|.
  // - |transfer_task_runner| must be the task runner on which QueueFrame() is
  //   normally called.
  void TransferQueueFromRealmRunner(
      TransferFramesCB transfer_frames_cb,
      scoped_refptr<base::SequencedTaskRunner> transfer_task_runner,
      CrossThreadOnceClosure transfer_done_cb);

 private:
  void CloseController();
  void QueueFrameOnRealmTaskRunner(NativeFrameType media_frame);
  void ProcessPullRequest();
  void SendFrameToStream(NativeFrameType media_frame);
  ScriptWrappable* MakeBlinkFrame(NativeFrameType media_frame);

  // Used to make sure no new frames arrive via QueueFrameOnRealmTaskRunner().
  void EnsureAllRealmRunnerFramesProcessed();
  void OnAllRealmRunnerFrameProcessed();

  // Must be called on the same task runner that calls QueueFrame() (most likely
  // the IO thread). Transfers all frames in |pending_transfer_queue_| via
  // |transfer_frames_cb|, and clears |transfer_frames_cb|.
  // QueueFrame() should never be called after this.
  void FinalizeQueueTransferOnTransferRunner();

  bool is_closed_ = false;

  // Main task runner for the window or worker context.
  const scoped_refptr<base::SequencedTaskRunner> realm_task_runner_;

  // Used when the queue is being transferred, to redirect frames that are in
  // flight between QueueFrames() and QueueFrameOnRealmTaskRunner().
  TransferFramesCB transfer_frames_cb_;
  scoped_refptr<base::SequencedTaskRunner> transfer_task_runner_;
  CrossThreadOnceClosure transfer_done_cb_;

  // Accumulates frames received while we are transferring the queue.
  Deque<NativeFrameType> pending_transfer_queue_;

  bool queue_transferred_ = false;

  // An internal deque prior to the stream controller's queue. It acts as a ring
  // buffer and allows dropping old frames instead of new ones in case frames
  // accumulate due to slow consumption.
  Deque<NativeFrameType> queue_;
  const wtf_size_t max_queue_size_;
  bool is_pending_pull_ = false;
};

template <>
ScriptWrappable* FrameQueueUnderlyingSource<scoped_refptr<media::VideoFrame>>::
    MakeBlinkFrame(scoped_refptr<media::VideoFrame>);

template <>
ScriptWrappable*
    FrameQueueUnderlyingSource<std::unique_ptr<AudioFrameSerializationData>>::
        MakeBlinkFrame(std::unique_ptr<AudioFrameSerializationData>);

extern template class MODULES_EXTERN_TEMPLATE_EXPORT
    FrameQueueUnderlyingSource<scoped_refptr<media::VideoFrame>>;
extern template class MODULES_EXTERN_TEMPLATE_EXPORT
    FrameQueueUnderlyingSource<std::unique_ptr<AudioFrameSerializationData>>;

using VideoFrameQueueUnderlyingSource =
    FrameQueueUnderlyingSource<scoped_refptr<media::VideoFrame>>;
using AudioFrameQueueUnderlyingSource =
    FrameQueueUnderlyingSource<std::unique_ptr<AudioFrameSerializationData>>;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_FRAME_QUEUE_UNDERLYING_SOURCE_H_
