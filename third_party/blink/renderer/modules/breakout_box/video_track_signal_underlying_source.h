// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_BREAKOUT_BOX_VIDEO_TRACK_SIGNAL_UNDERLYING_SOURCE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_BREAKOUT_BOX_VIDEO_TRACK_SIGNAL_UNDERLYING_SOURCE_H_

#include "base/threading/thread_checker.h"
#include "third_party/blink/renderer/core/streams/underlying_source_base.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_video_track_signal_observer.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/wtf/deque.h"

namespace blink {

class MediaStreamTrackSignal;
class MediaStreamTrackGenerator;

// This class serves as the source for the control signals sent to
// a MediaStreamTrackGenerator and exposed on its readableControl field.
// This class maintains an internal circular queue to store unconsumed
// signals. If the queue becomes full and new signals are produced,
// older signals are dropped to accommodate the new signals.
class MODULES_EXPORT VideoTrackSignalUnderlyingSource
    : public UnderlyingSourceBase,
      public MediaStreamVideoTrackSignalObserver {
 public:
  explicit VideoTrackSignalUnderlyingSource(ScriptState*,
                                            MediaStreamTrackGenerator*,
                                            wtf_size_t queue_size);
  VideoTrackSignalUnderlyingSource(const VideoTrackSignalUnderlyingSource&) =
      delete;
  VideoTrackSignalUnderlyingSource& operator=(
      const VideoTrackSignalUnderlyingSource&) = delete;

  // UnderlyingSourceBase
  ScriptPromise pull(ScriptState*) override;
  ScriptPromise Start(ScriptState*) override;
  ScriptPromise Cancel(ScriptState*, ScriptValue reason) override;

  // ExecutionLifecycleObserver
  void ContextDestroyed() override;

  MediaStreamTrackGenerator* Generator() const { return generator_.Get(); }
  wtf_size_t MaxQueueSize() const { return max_queue_size_; }

  bool IsPendingPullForTesting() const { return is_pending_pull_; }
  const HeapDeque<Member<MediaStreamTrackSignal>>& QueueForTesting() const {
    return queue_;
  }
  double DesiredSizeForTesting() const;

  void Close();
  void Trace(Visitor*) const override;

 private:
  // MediaStreamVideoTrackSignalObserver
  void SetMinimumFrameRate(double) override;
  void RequestFrame() override;

  void ProcessNewSignal(MediaStreamTrackSignal*);
  void SendSignalToStream(MediaStreamTrackSignal*);
  void PullFromQueue();
  void Stop();

  const scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  Member<MediaStreamTrackGenerator> generator_;

  // An internal deque prior to the stream controller's queue. It acts as a ring
  // buffer and allows dropping old signals instead of new ones in case signals
  // accumulate due to slow consumption.
  HeapDeque<Member<MediaStreamTrackSignal>> queue_;
  const wtf_size_t max_queue_size_;
  bool is_pending_pull_ = false;
  bool is_active_ = true;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_BREAKOUT_BOX_VIDEO_TRACK_SIGNAL_UNDERLYING_SOURCE_H_
