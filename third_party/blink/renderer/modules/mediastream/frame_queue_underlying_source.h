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
class FrameQueueUnderlyingSource : public UnderlyingSourceBase {
 public:
  FrameQueueUnderlyingSource(ScriptState*, wtf_size_t queue_size);
  ~FrameQueueUnderlyingSource() override = default;

  FrameQueueUnderlyingSource(const FrameQueueUnderlyingSource&) = delete;
  FrameQueueUnderlyingSource& operator=(const FrameQueueUnderlyingSource&) =
      delete;

  // UnderlyingSourceBase
  ScriptPromise pull(ScriptState*) override;
  ScriptPromise Start(ScriptState*) override;
  ScriptPromise Cancel(ScriptState*, ScriptValue reason) override;

  // ExecutionLifecycleObserver
  void ContextDestroyed() override;

  wtf_size_t MaxQueueSize() const { return max_queue_size_; }

  // Clears all internal state and closes the UnderlyingSource's Controller.
  // Must be called on |realm_task_runner_|.
  void Close();

  // Adds a frame to |queue_|, dropping the oldest frame if it is full.
  // Can be called from any task runner, and will jump to |realm_task_runner_|.
  void QueueFrame(NativeFrameType media_frame);

  // Start or stop the delivery of frames via QueueFrame().
  // Must be called on |main_trask_runner_|.
  virtual bool StartFrameDelivery() = 0;
  virtual void StopFrameDelivery() = 0;

  // Temporary workaround for crbug.com/1182497. Marks blink::VideoFrames to be
  // closed when cloned(), to prevent stalls when posting internally to a
  // transferred stream.
  void SetStreamWasTransferred() { stream_was_transferred_ = true; }

  bool IsPendingPullForTesting() const { return is_pending_pull_; }
  const Deque<NativeFrameType>& QueueForTesting() const { return queue_; }
  double DesiredSizeForTesting() const;

  void Trace(Visitor*) const override;

 protected:
  scoped_refptr<base::SequencedTaskRunner> GetSourceRunner() {
    return realm_task_runner_;
  }

 private:
  void QueueFrameOnRealmTaskRunner(NativeFrameType media_frame);
  void ProcessPullRequest();
  void SendFrameToStream(NativeFrameType media_frame);
  ScriptWrappable* MakeBlinkFrame(NativeFrameType media_frame);

  // Used when a stream endpoint was transferred to another realm, to
  // automatically close frames as they are posted to the other stream.
  bool stream_was_transferred_ = false;

  // Main task runner for the window or worker context.
  const scoped_refptr<base::SequencedTaskRunner> realm_task_runner_;

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
