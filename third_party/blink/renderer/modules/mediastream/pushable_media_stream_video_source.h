// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_PUSHABLE_MEDIA_STREAM_VIDEO_SOURCE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_PUSHABLE_MEDIA_STREAM_VIDEO_SOURCE_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/public/web/modules/mediastream/media_stream_video_source.h"
#include "third_party/blink/renderer/modules/modules_export.h"

namespace blink {

// Simplifies the creation of video tracks.  Just do this:
// auto source = std::make_unique<PushableMediaStreamVideoSource>();
// auto* track = CreateVideoTrackFromSource(script_state, source);
// for each frame:
//   source->PushFrame(video_frame, capture_time);
// source->Stop();
class MODULES_EXPORT PushableMediaStreamVideoSource
    : public MediaStreamVideoSource {
 public:
  PushableMediaStreamVideoSource() = default;
  explicit PushableMediaStreamVideoSource(
      const base::WeakPtr<MediaStreamVideoSource>& upstream_source);
  // See the definition of VideoCaptureDeliverFrameCB in
  // media/capture/video_capturer_source.h for the documentation
  // of |estimated_capture_time| and the difference with
  // media::VideoFrame::timestamp().
  void PushFrame(scoped_refptr<media::VideoFrame> video_frame,
                 base::TimeTicks estimated_capture_time);
  bool running() const { return running_; }

  // MediaStreamVideoSource
  void RequestRefreshFrame() override;
  void OnFrameDropped(media::VideoCaptureFrameDropReason reason) override;
  VideoCaptureFeedbackCB GetFeedbackCallback() const override;
  void StartSourceImpl(VideoCaptureDeliverFrameCB frame_callback,
                       EncodedVideoFrameCB encoded_frame_callback) override;
  void StopSourceImpl() override;
  base::WeakPtr<MediaStreamVideoSource> GetWeakPtr() const override;

  VideoCaptureFeedbackCB GetInternalFeedbackCallback() const;

 private:
  bool running_ = false;
  VideoCaptureDeliverFrameCB deliver_frame_cb_;

  base::WeakPtr<MediaStreamVideoSource> upstream_source_;

  THREAD_CHECKER(thread_checker_);
  base::WeakPtrFactory<MediaStreamVideoSource> weak_factory_{this};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_PUSHABLE_MEDIA_STREAM_VIDEO_SOURCE_H_
