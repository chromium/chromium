// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_MEDIA_STREAM_VIDEO_TRACK_UNDERLYING_SOURCE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_MEDIA_STREAM_VIDEO_TRACK_UNDERLYING_SOURCE_H_

#include "base/threading/thread_checker.h"
#include "third_party/blink/public/web/modules/mediastream/media_stream_video_sink.h"
#include "third_party/blink/renderer/core/streams/underlying_source_base.h"
#include "third_party/blink/renderer/modules/modules_export.h"

namespace blink {

class MediaStreamComponent;

class MODULES_EXPORT MediaStreamVideoTrackUnderlyingSource
    : public UnderlyingSourceBase,
      public MediaStreamVideoSink {
 public:
  explicit MediaStreamVideoTrackUnderlyingSource(ScriptState*,
                                                 MediaStreamComponent*);
  MediaStreamVideoTrackUnderlyingSource(
      const MediaStreamVideoTrackUnderlyingSource&) = delete;
  MediaStreamVideoTrackUnderlyingSource& operator=(
      const MediaStreamVideoTrackUnderlyingSource&) = delete;

  // UnderlyingSourceBase
  ScriptPromise pull(ScriptState*) override;
  ScriptPromise Start(ScriptState*) override;
  ScriptPromise Cancel(ScriptState*, ScriptValue reason) override;

  void Close();
  void Trace(Visitor*) const override;

 private:
  FRIEND_TEST_ALL_PREFIXES(MediaStreamVideoTrackUnderlyingSourceTest,
                           FramesAreDropped);

  void OnFrameFromTrack(scoped_refptr<media::VideoFrame> media_frame,
                        base::TimeTicks estimated_capture_time);
  void OnFrameFromTrackOnMainThread(
      scoped_refptr<media::VideoFrame> media_frame,
      base::TimeTicks estimated_capture_time);

  const scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  const Member<MediaStreamComponent> track_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_MEDIA_STREAM_VIDEO_TRACK_UNDERLYING_SOURCE_H_
