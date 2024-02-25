// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/breakout_box/pushable_media_stream_video_source.h"

#include "base/synchronization/lock.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-blink.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_source.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_media.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

PushableMediaStreamVideoSource::Broker::Broker(
    PushableMediaStreamVideoSource* source)
    : source_(source),
      main_task_runner_(source->GetTaskRunner()),
      video_task_runner_(source->video_task_runner()) {
  DCHECK(main_task_runner_);
  DCHECK(video_task_runner_);
}

void PushableMediaStreamVideoSource::Broker::OnClientStarted() {
  base::AutoLock locker(lock_);
  DCHECK_GE(num_clients_, 0);
  ++num_clients_;
}

void PushableMediaStreamVideoSource::Broker::OnClientStopped() {
  bool should_stop = false;
  {
    base::AutoLock locker(lock_);
    should_stop = --num_clients_ == 0;
    DCHECK_GE(num_clients_, 0);
  }
  if (should_stop)
    StopSource();
}

bool PushableMediaStreamVideoSource::Broker::IsRunning() {
  base::AutoLock locker(lock_);
  return !frame_callback_.is_null();
}

bool PushableMediaStreamVideoSource::Broker::CanDiscardAlpha() {
  base::AutoLock locker(lock_);
  return can_discard_alpha_;
}

bool PushableMediaStreamVideoSource::Broker::RequireMappedFrame() {
  base::AutoLock locker(lock_);
  return feedback_.require_mapped_frame;
}

void PushableMediaStreamVideoSource::Broker::PushFrame(
    scoped_refptr<media::VideoFrame> video_frame,
    base::TimeTicks estimated_capture_time) {
  base::AutoLock locker(lock_);
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
      *video_task_runner_, FROM_HERE,
      CrossThreadBindOnce(frame_callback_, std::move(video_frame),
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
  base::AutoLock locker(lock_);
  return muted_;
}

void PushableMediaStreamVideoSource::Broker::SetMuted(bool muted) {
  base::AutoLock locker(lock_);
  muted_ = muted;
}

void PushableMediaStreamVideoSource::Broker::OnSourceStarted(
    VideoCaptureDeliverFrameCB frame_callback) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DCHECK(!frame_callback.is_null());
  if (!source_)
    return;

  base::AutoLock locker(lock_);
  frame_callback_ = std::move(frame_callback);
}

void PushableMediaStreamVideoSource::Broker::OnSourceDestroyedOrStopped() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  base::AutoLock locker(lock_);
  source_ = nullptr;
  frame_callback_.Reset();
}

void PushableMediaStreamVideoSource::Broker::StopSourceOnMain() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  if (!source_)
    return;

  source_->StopSource();
}

void PushableMediaStreamVideoSource::Broker::SetCanDiscardAlpha(
    bool can_discard_alpha) {
  base::AutoLock locker(lock_);
  can_discard_alpha_ = can_discard_alpha;
}

void PushableMediaStreamVideoSource::Broker::ProcessFeedback(
    const media::VideoCaptureFeedback& feedback) {
  base::AutoLock locker(lock_);
  feedback_ = feedback;
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
    EncodedVideoFrameCB encoded_frame_callback,
    VideoCaptureSubCaptureTargetVersionCB sub_capture_target_version_callback,
    // The pushable media stream does not report frame drops.
    VideoCaptureNotifyFrameDroppedCB) {
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
PushableMediaStreamVideoSource::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void PushableMediaStreamVideoSource::OnSourceCanDiscardAlpha(
    bool can_discard_alpha) {
  broker_->SetCanDiscardAlpha(can_discard_alpha);
}

media::VideoCaptureFeedbackCB
PushableMediaStreamVideoSource::GetFeedbackCallback() const {
  return base::BindPostTask(
      GetTaskRunner(),
      WTF::BindRepeating(
          &PushableMediaStreamVideoSource::ProcessFeedbackInternal,
          weak_factory_.GetMutableWeakPtr()));
}

void PushableMediaStreamVideoSource::ProcessFeedbackInternal(
    const media::VideoCaptureFeedback& feedback) {
  broker_->ProcessFeedback(feedback);
}

}  // namespace blink
