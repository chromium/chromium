// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/breakout_box/pushable_media_stream_video_source.h"

#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-blink.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_source.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

PushableMediaStreamVideoSource::Broker::Broker(
    PushableMediaStreamVideoSource* source)
    : source_(source),
      main_task_runner_(source->GetTaskRunner()),
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
  // If the source is muted, we don't forward frames.
  if (muted_) {
    return;
  }

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

bool PushableMediaStreamVideoSource::Broker::IsMuted() {
  WTF::MutexLocker locker(mutex_);
  return muted_;
}

void PushableMediaStreamVideoSource::Broker::SetMuted(bool muted) {
  WTF::MutexLocker locker(mutex_);
  muted_ = muted;
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

PushableMediaStreamVideoSource::PushableMediaStreamVideoSource(
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner)
    : MediaStreamVideoSource(std::move(main_task_runner)),
      broker_(AdoptRef(new Broker(this))) {}

PushableMediaStreamVideoSource::~PushableMediaStreamVideoSource() {
  broker_->OnSourceDestroyedOrStopped();
}

void PushableMediaStreamVideoSource::PushFrame(
    scoped_refptr<media::VideoFrame> video_frame,
    base::TimeTicks estimated_capture_time) {
  broker_->PushFrame(std::move(video_frame), estimated_capture_time);
}

void PushableMediaStreamVideoSource::StartSourceImpl(
    VideoCaptureDeliverFrameCB frame_callback,
    EncodedVideoFrameCB encoded_frame_callback) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  DCHECK(frame_callback);
  broker_->OnSourceStarted(std::move(frame_callback));
  OnStartDone(mojom::blink::MediaStreamRequestResult::OK);
}

void PushableMediaStreamVideoSource::StopSourceImpl() {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  broker_->OnSourceDestroyedOrStopped();
}

base::WeakPtr<MediaStreamVideoSource>
PushableMediaStreamVideoSource::GetWeakPtr() const {
  return weak_factory_.GetWeakPtr();
}

}  // namespace blink
