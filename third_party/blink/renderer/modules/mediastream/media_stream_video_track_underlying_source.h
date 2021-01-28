// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_MEDIA_STREAM_VIDEO_TRACK_UNDERLYING_SOURCE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_MEDIA_STREAM_VIDEO_TRACK_UNDERLYING_SOURCE_H_

#include "base/threading/thread_checker.h"
#include "third_party/blink/public/web/modules/mediastream/media_stream_video_sink.h"
#include "third_party/blink/renderer/core/streams/underlying_source_base.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/wtf/deque.h"

namespace blink {

class MediaStreamComponent;

class MODULES_EXPORT MediaStreamVideoTrackUnderlyingSource
    : public UnderlyingSourceBase,
      public MediaStreamVideoSink {
 public:
  explicit MediaStreamVideoTrackUnderlyingSource(ScriptState*,
                                                 MediaStreamComponent*,
                                                 wtf_size_t queue_size);
  MediaStreamVideoTrackUnderlyingSource(
      const MediaStreamVideoTrackUnderlyingSource&) = delete;
  MediaStreamVideoTrackUnderlyingSource& operator=(
      const MediaStreamVideoTrackUnderlyingSource&) = delete;

  // UnderlyingSourceBase
  ScriptPromise pull(ScriptState*) override;
  ScriptPromise Start(ScriptState*) override;
  ScriptPromise Cancel(ScriptState*, ScriptValue reason) override;

  // ExecutionLifecycleObserver
  void ContextDestroyed() override;

  MediaStreamComponent* Track() const { return track_.Get(); }
  wtf_size_t MaxQueueSize() const { return max_queue_size_; }

  bool IsPendingPullForTesting() const { return is_pending_pull_; }
  const Deque<scoped_refptr<media::VideoFrame>>& QueueForTesting() const {
    return queue_;
  }
  double DesiredSizeForTesting() const;

  void Close();
  void Trace(Visitor*) const override;

 private:
  void OnFrameFromTrack(
      scoped_refptr<media::VideoFrame> media_frame,
      std::vector<scoped_refptr<media::VideoFrame>> scaled_media_frames,
      base::TimeTicks estimated_capture_time);
  void OnFrameFromTrackOnMainThread(
      scoped_refptr<media::VideoFrame> media_frame,
      base::TimeTicks estimated_capture_time);
  void SendFrameToStream(scoped_refptr<media::VideoFrame> media_frame);
  void ProcessPullRequest();

  const scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  const Member<MediaStreamComponent> track_;

  // An internal deque prior to the stream controller's queue. It acts as a ring
  // buffer and allows dropping old frames instead of new ones in case frames
  // accumulate due to slow consumption.
  Deque<scoped_refptr<media::VideoFrame>> queue_;
  const wtf_size_t max_queue_size_;
  bool is_pending_pull_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_MEDIA_STREAM_VIDEO_TRACK_UNDERLYING_SOURCE_H_
