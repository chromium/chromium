// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_BREAKOUT_BOX_FRAME_QUEUE_UNDERLYING_SOURCE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_BREAKOUT_BOX_FRAME_QUEUE_UNDERLYING_SOURCE_H_

#include "base/threading/thread_checker.h"
#include "media/base/audio_buffer.h"
#include "media/base/video_frame.h"
#include "third_party/blink/renderer/core/streams/underlying_source_base.h"
#include "third_party/blink/renderer/modules/breakout_box/frame_queue.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/wtf/deque.h"

namespace blink {

template <typename NativeFrameType>
class FrameQueueUnderlyingSource
    : public UnderlyingSourceBase,
      public ActiveScriptWrappable<
          FrameQueueUnderlyingSource<NativeFrameType>> {
 public:
  using TransferFramesCB = CrossThreadFunction<void(NativeFrameType)>;

  // Initializes a new FrameQueueUnderlyingSource with a new internal circular
  // queue that can hold up to |queue_size| elements.
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

  wtf_size_t MaxQueueSize() const;

  // Clears all internal state and closes the UnderlyingSource's Controller.
  // Must be called on |realm_task_runner_|.
  void Close();

  bool IsClosed() {
    DCHECK(realm_task_runner_->RunsTasksInCurrentSequence());
    return is_closed_;
  }

  // Start or stop the delivery of frames via QueueFrame().
  // Must be called on |realm_task_runner_|.
  virtual bool StartFrameDelivery() = 0;
  virtual void StopFrameDelivery() = 0;

  // Delivers a new frame to this source.
  void QueueFrame(NativeFrameType);

  bool IsPendingPullForTesting() const;
  double DesiredSizeForTesting() const;

  void Trace(Visitor*) const override;

 protected:
  // Initializes a new FrameQueueUnderlyingSource containing a
  // |frame_queue_handle_| that references the same internal circular queue as
  // |other_source|.
  FrameQueueUnderlyingSource(
      ScriptState*,
      FrameQueueUnderlyingSource<NativeFrameType>* other_source);
  scoped_refptr<base::SequencedTaskRunner> GetRealmRunner() {
    return realm_task_runner_;
  }

  // Sets |transferred_source| as the new target for frames arriving via
  // QueueFrame(). |transferred_source| will pull frames from the same circular
  // queue. Must be called on |realm_task_runner_|.
  void TransferSource(
      FrameQueueUnderlyingSource<NativeFrameType>* transferred_source);

 private:
  // Must be called on |realm_task_runner_|.
  void CloseController();

  // If the internal queue is not empty, pops a frame from it and enqueues it
  // into the the stream's controller. Must be called on |realm_task_runner|.
  void MaybeSendFrameFromQueueToStream();

  // Creates a JS frame (VideoFrame or AudioData) backed by |media_frame|.
  // Must be called on |realm_task_runner_|.
  ScriptWrappable* MakeBlinkFrame(NativeFrameType media_frame);

  bool is_closed_ = false;
  bool is_pending_close_ = false;

  // Main task runner for the window or worker context this source runs on.
  const scoped_refptr<base::SequencedTaskRunner> realm_task_runner_;

  // |frame_queue_handle_| is a handle containing a reference to an internal
  // circular queue prior to the stream controller's queue. It allows dropping
  // older frames in case frames accumulate due to slow consumption.
  // Frames are normally pushed on a background task runner (for example, the
  // IO thread) and popped on |realm_task_runner_|.
  FrameQueueHandle<NativeFrameType> frame_queue_handle_;

  mutable Mutex mutex_;
  // If the stream backed by this source is transferred to another in-process
  // realm (e.g., a Worker), |transferred_source_| is the source of the
  // transferred stream.
  CrossThreadPersistent<FrameQueueUnderlyingSource<NativeFrameType>>
      transferred_source_ GUARDED_BY(mutex_);
  bool is_pending_pull_ GUARDED_BY(mutex_) = false;
};

template <>
ScriptWrappable* FrameQueueUnderlyingSource<scoped_refptr<media::VideoFrame>>::
    MakeBlinkFrame(scoped_refptr<media::VideoFrame>);

template <>
ScriptWrappable* FrameQueueUnderlyingSource<scoped_refptr<media::AudioBuffer>>::
    MakeBlinkFrame(scoped_refptr<media::AudioBuffer>);

extern template class MODULES_EXTERN_TEMPLATE_EXPORT
    FrameQueueUnderlyingSource<scoped_refptr<media::VideoFrame>>;
extern template class MODULES_EXTERN_TEMPLATE_EXPORT
    FrameQueueUnderlyingSource<scoped_refptr<media::AudioBuffer>>;

using VideoFrameQueueUnderlyingSource =
    FrameQueueUnderlyingSource<scoped_refptr<media::VideoFrame>>;
using AudioDataQueueUnderlyingSource =
    FrameQueueUnderlyingSource<scoped_refptr<media::AudioBuffer>>;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_BREAKOUT_BOX_FRAME_QUEUE_UNDERLYING_SOURCE_H_
