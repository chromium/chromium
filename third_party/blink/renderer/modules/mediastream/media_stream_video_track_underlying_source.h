// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_MEDIA_STREAM_VIDEO_TRACK_UNDERLYING_SOURCE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_MEDIA_STREAM_VIDEO_TRACK_UNDERLYING_SOURCE_H_

#include "base/sequence_checker.h"
#include "third_party/blink/public/web/modules/mediastream/media_stream_video_sink.h"
#include "third_party/blink/renderer/core/streams/readable_stream_transferring_optimizer.h"
#include "third_party/blink/renderer/modules/mediastream/frame_queue_underlying_source.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/heap/impl/garbage_collected.h"

namespace blink {

class MediaStreamComponent;

class MODULES_EXPORT MediaStreamVideoTrackUnderlyingSource
    : public VideoFrameQueueUnderlyingSource,
      public MediaStreamVideoSink {
 public:
  explicit MediaStreamVideoTrackUnderlyingSource(ScriptState*,
                                                 MediaStreamComponent*,
                                                 wtf_size_t queue_size);
  MediaStreamVideoTrackUnderlyingSource(
      const MediaStreamVideoTrackUnderlyingSource&) = delete;
  MediaStreamVideoTrackUnderlyingSource& operator=(
      const MediaStreamVideoTrackUnderlyingSource&) = delete;

  MediaStreamComponent* Track() const { return track_.Get(); }

  void Trace(Visitor*) const override;

  std::unique_ptr<ReadableStreamTransferringOptimizer>
  GetStreamTransferOptimizer();

 private:
  // FrameQueueUnderlyingSource implementation.
  bool StartFrameDelivery() override;
  void StopFrameDelivery() override;

  void OnFrameFromTrack(
      scoped_refptr<media::VideoFrame> media_frame,
      std::vector<scoped_refptr<media::VideoFrame>> scaled_media_frames,
      base::TimeTicks estimated_capture_time);

  const Member<MediaStreamComponent> track_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_MEDIA_STREAM_VIDEO_TRACK_UNDERLYING_SOURCE_H_
