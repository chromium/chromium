// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/mock_media_stream_video_source.h"

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-blink.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_media.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

MockMediaStreamVideoSource::MockMediaStreamVideoSource()
    : MockMediaStreamVideoSource(false) {}

MockMediaStreamVideoSource::MockMediaStreamVideoSource(
    bool respond_to_request_refresh_frame)
    : MediaStreamVideoSource(scheduler::GetSingleThreadTaskRunnerForTesting()),
      respond_to_request_refresh_frame_(respond_to_request_refresh_frame),
      attempted_to_start_(false) {}

MockMediaStreamVideoSource::MockMediaStreamVideoSource(
    const media::VideoCaptureFormat& format,
    bool respond_to_request_refresh_frame)
    : MediaStreamVideoSource(scheduler::GetSingleThreadTaskRunnerForTesting()),
      format_(format),
      respond_to_request_refresh_frame_(respond_to_request_refresh_frame),
      attempted_to_start_(false) {}

MockMediaStreamVideoSource::~MockMediaStreamVideoSource() = default;

void MockMediaStreamVideoSource::StartMockedSource() {
  DCHECK(attempted_to_start_);
  attempted_to_start_ = false;
  OnStartDone(mojom::blink::MediaStreamRequestResult::OK);
}

void MockMediaStreamVideoSource::FailToStartMockedSource() {
  DCHECK(attempted_to_start_);
  attempted_to_start_ = false;
  OnStartDone(
      mojom::blink::MediaStreamRequestResult::TRACK_START_FAILURE_VIDEO);
}

void MockMediaStreamVideoSource::RequestKeyFrame() {
  OnRequestKeyFrame();
}

void MockMediaStreamVideoSource::RequestRefreshFrame() {
  DCHECK(!frame_callback_.is_null());
  if (respond_to_request_refresh_frame_) {
    const scoped_refptr<media::VideoFrame> frame =
        media::VideoFrame::CreateColorFrame(format_.frame_size, 0, 0, 0,
                                            base::TimeDelta());
    PostCrossThreadTask(
        *video_task_runner(), FROM_HERE,
        CrossThreadBindOnce(frame_callback_, frame, base::TimeTicks()));
  }
  OnRequestRefreshFrame();
}

void MockMediaStreamVideoSource::OnHasConsumers(bool has_consumers) {
  is_suspended_ = !has_consumers;
}

base::WeakPtr<MediaStreamVideoSource> MockMediaStreamVideoSource::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void MockMediaStreamVideoSource::DoChangeSource(
    const MediaStreamDevice& new_device) {
  ChangeSourceImpl(new_device);
}

void MockMediaStreamVideoSource::StartSourceImpl(
    VideoCaptureDeliverFrameCB frame_callback,
    EncodedVideoFrameCB encoded_frame_callback,
    VideoCaptureSubCaptureTargetVersionCB sub_capture_target_version_callback,
    VideoCaptureNotifyFrameDroppedCB frame_dropped_callback) {
  DCHECK(frame_callback_.is_null());
  DCHECK(encoded_frame_callback_.is_null());
  DCHECK(sub_capture_target_version_callback_.is_null());
  attempted_to_start_ = true;
  frame_callback_ = std::move(frame_callback);
  encoded_frame_callback_ = std::move(encoded_frame_callback);
  sub_capture_target_version_callback_ =
      std::move(sub_capture_target_version_callback);
  frame_dropped_callback_ = std::move(frame_dropped_callback);
}

void MockMediaStreamVideoSource::StopSourceImpl() {}

std::optional<media::VideoCaptureFormat>
MockMediaStreamVideoSource::GetCurrentFormat() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return std::optional<media::VideoCaptureFormat>(format_);
}

void MockMediaStreamVideoSource::DeliverVideoFrame(
    scoped_refptr<media::VideoFrame> frame) {
  DCHECK(!is_stopped_for_restart_);
  DCHECK(!frame_callback_.is_null());
  PostCrossThreadTask(
      *video_task_runner(), FROM_HERE,
      CrossThreadBindOnce(frame_callback_, std::move(frame),
                          base::TimeTicks()));
}

void MockMediaStreamVideoSource::DeliverEncodedVideoFrame(
    scoped_refptr<EncodedVideoFrame> frame) {
  DCHECK(!is_stopped_for_restart_);
  DCHECK(!encoded_frame_callback_.is_null());
  PostCrossThreadTask(*video_task_runner(), FROM_HERE,
                      CrossThreadBindOnce(encoded_frame_callback_,
                                          std::move(frame), base::TimeTicks()));
}

void MockMediaStreamVideoSource::DropFrame(
    media::VideoCaptureFrameDropReason reason) {
  DCHECK(!is_stopped_for_restart_);
  DCHECK(!frame_dropped_callback_.is_null());
  PostCrossThreadTask(*video_task_runner(), FROM_HERE,
                      CrossThreadBindOnce(frame_dropped_callback_, reason));
}

void MockMediaStreamVideoSource::DeliverNewSubCaptureTargetVersion(
    uint32_t sub_capture_target_version) {
  DCHECK(!sub_capture_target_version_callback_.is_null());
  PostCrossThreadTask(*video_task_runner(), FROM_HERE,
                      CrossThreadBindOnce(sub_capture_target_version_callback_,
                                          sub_capture_target_version));
}

void MockMediaStreamVideoSource::StopSourceForRestartImpl() {
  if (can_stop_for_restart_)
    is_stopped_for_restart_ = true;
  OnStopForRestartDone(is_stopped_for_restart_);
}

void MockMediaStreamVideoSource::RestartSourceImpl(
    const media::VideoCaptureFormat& new_format) {
  DCHECK(is_stopped_for_restart_);
  if (!can_restart_) {
    OnRestartDone(false);
    return;
  }
  ++restart_count_;
  is_stopped_for_restart_ = false;
  format_ = new_format;
  OnRestartDone(true);
}

}  // namespace blink
