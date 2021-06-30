// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/breakout_box/pushable_media_stream_video_source.h"

#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-blink.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_video_track_signal_observer.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

PushableMediaStreamVideoSource::Broker::Broker(
    PushableMediaStreamVideoSource* source)
    : source_(source),
      main_task_runner_(Thread::MainThread()->GetTaskRunner()),
      io_task_runner_(source->io_task_runner()) {
  DCHECK(main_task_runner_);
  DCHECK(io_task_runner_);
}

void PushableMediaStreamVideoSource::Broker::OnClientStarted() {
  WTF::MutexLocker locker(mutex_);
  DCHECK_GE(num_clients_, 0);
  ++num_clients_;
}

void PushableMediaStreamVideoSource::Broker::OnClientStopped() {
  bool should_stop = false;
  {
    WTF::MutexLocker locker(mutex_);
    should_stop = --num_clients_ == 0;
    DCHECK_GE(num_clients_, 0);
  }
  if (should_stop)
    StopSource();
}

bool PushableMediaStreamVideoSource::Broker::IsRunning() {
  WTF::MutexLocker locker(mutex_);
  return !frame_callback_.is_null();
}

void PushableMediaStreamVideoSource::Broker::PushFrame(
    scoped_refptr<media::VideoFrame> video_frame,
    base::TimeTicks estimated_capture_time) {
  WTF::MutexLocker locker(mutex_);
  if (!source_ || frame_callback_.is_null())
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
      *io_task_runner_, FROM_HERE,
      CrossThreadBindOnce(frame_callback_, std::move(video_frame),
                          std::vector<scoped_refptr<media::VideoFrame>>(),
                          estimated_capture_time));
}

void PushableMediaStreamVideoSource::Broker::StopSource() {
  if (main_task_runner_->BelongsToCurrentThread()) {
    StopSourceOnMain();
  } else {
    PostCrossThreadTask(
        *main_task_runner_, FROM_HERE,
        CrossThreadBindOnce(
            &PushableMediaStreamVideoSource::Broker::StopSourceOnMain,
            WrapRefCounted(this)));
  }
}

void PushableMediaStreamVideoSource::Broker::OnSourceStarted(
    VideoCaptureDeliverFrameCB frame_callback) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DCHECK(!frame_callback.is_null());
  if (!source_)
    return;

  WTF::MutexLocker locker(mutex_);
  frame_callback_ = std::move(frame_callback);
}

void PushableMediaStreamVideoSource::Broker::OnSourceDestroyedOrStopped() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  WTF::MutexLocker locker(mutex_);
  source_ = nullptr;
  frame_callback_.Reset();
}

void PushableMediaStreamVideoSource::Broker::StopSourceOnMain() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  if (!source_)
    return;

  source_->StopSource();
}

PushableMediaStreamVideoSource::PushableMediaStreamVideoSource()
    : broker_(AdoptRef(new Broker(this))) {}

PushableMediaStreamVideoSource::PushableMediaStreamVideoSource(
    const base::WeakPtr<MediaStreamVideoSource>& upstream_source)
    : upstream_source_(upstream_source), broker_(AdoptRef(new Broker(this))) {}

PushableMediaStreamVideoSource::~PushableMediaStreamVideoSource() {
  broker_->OnSourceDestroyedOrStopped();
}

void PushableMediaStreamVideoSource::PushFrame(
    scoped_refptr<media::VideoFrame> video_frame,
    base::TimeTicks estimated_capture_time) {
  broker_->PushFrame(std::move(video_frame), estimated_capture_time);
}

void PushableMediaStreamVideoSource::RequestRefreshFrame() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (upstream_source_)
    upstream_source_->RequestRefreshFrame();
  if (signal_observer_)
    signal_observer_->RequestFrame();
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
           const media::VideoCaptureFeedback& feedback) {
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
  broker_->OnSourceStarted(std::move(frame_callback));
  OnStartDone(mojom::blink::MediaStreamRequestResult::OK);
}

void PushableMediaStreamVideoSource::StopSourceImpl() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  broker_->OnSourceDestroyedOrStopped();
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

void PushableMediaStreamVideoSource::SetSignalObserver(
    MediaStreamVideoTrackSignalObserver* observer) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  signal_observer_ = observer;
}

}  // namespace blink
