// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/pushable_media_stream_video_source.h"

#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-blink.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

PushableMediaStreamVideoSource::PushableMediaStreamVideoSource(
    const base::WeakPtr<MediaStreamVideoSource>& upstream_source)
    : upstream_source_(upstream_source) {}

void PushableMediaStreamVideoSource::PushFrame(
    scoped_refptr<media::VideoFrame> video_frame,
    base::TimeTicks estimated_capture_time) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!running_)
    return;

  // Note that although use of the IO thread is rare in blink, it's required
  // by any implementation of MediaStreamVideoSource, which is made clear by
  // the documentation of MediaStreamVideoSource::StartSourceImpl which reads
  // "An implementation must call |frame_callback| on the IO thread."
  // Also see the DCHECK at VideoTrackAdapter::DeliverFrameOnIO
  // and the other of implementations of MediaStreamVideoSource at
  // MediaStreamRemoteVideoSource::StartSourceImpl,
  // CastReceiverSession::StartVideo,
  // CanvasCaptureHandler::SendFrame,
  // and HtmlVideoElementCapturerSource::sendNewFrame.
  PostCrossThreadTask(
      *io_task_runner(), FROM_HERE,
      CrossThreadBindOnce(deliver_frame_cb_, std::move(video_frame),
                          std::vector<scoped_refptr<media::VideoFrame>>(),
                          estimated_capture_time));
}

void PushableMediaStreamVideoSource::RequestRefreshFrame() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (upstream_source_)
    upstream_source_->RequestRefreshFrame();
}

void PushableMediaStreamVideoSource::OnFrameDropped(
    media::VideoCaptureFrameDropReason reason) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (upstream_source_)
    upstream_source_->OnFrameDropped(reason);
}

VideoCaptureFeedbackCB PushableMediaStreamVideoSource::GetFeedbackCallback()
    const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (upstream_source_) {
    return WTF::BindRepeating(
        [](const base::WeakPtr<MediaStreamVideoSource>& source,
           const media::VideoFrameFeedback& feedback) {
          if (!source)
            return;

          PushableMediaStreamVideoSource* pushable_source =
              static_cast<PushableMediaStreamVideoSource*>(source.get());
          pushable_source->GetInternalFeedbackCallback().Run(feedback);
        },
        GetWeakPtr());
  }
  return VideoCaptureFeedbackCB();
}

void PushableMediaStreamVideoSource::StartSourceImpl(
    VideoCaptureDeliverFrameCB frame_callback,
    EncodedVideoFrameCB encoded_frame_callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(frame_callback);
  running_ = true;
  deliver_frame_cb_ = frame_callback;
  OnStartDone(mojom::blink::MediaStreamRequestResult::OK);
}

void PushableMediaStreamVideoSource::StopSourceImpl() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  running_ = false;
}

base::WeakPtr<MediaStreamVideoSource>
PushableMediaStreamVideoSource::GetWeakPtr() const {
  return weak_factory_.GetWeakPtr();
}

VideoCaptureFeedbackCB
PushableMediaStreamVideoSource::GetInternalFeedbackCallback() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!upstream_source_)
    return VideoCaptureFeedbackCB();

  return upstream_source_->GetFeedbackCallback();
}

}  // namespace blink
