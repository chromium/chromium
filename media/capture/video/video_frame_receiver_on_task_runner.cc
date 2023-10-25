// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/video_frame_receiver_on_task_runner.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"

namespace media {

VideoFrameReceiverOnTaskRunner::VideoFrameReceiverOnTaskRunner(
    const base::WeakPtr<VideoFrameReceiver>& receiver,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : receiver_(receiver), task_runner_(std::move(task_runner)) {}

VideoFrameReceiverOnTaskRunner::~VideoFrameReceiverOnTaskRunner() = default;

void VideoFrameReceiverOnTaskRunner::OnCaptureConfigurationChanged() {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VideoFrameReceiver::OnCaptureConfigurationChanged,
                     receiver_));
}

void VideoFrameReceiverOnTaskRunner::OnNewBuffer(
    int buffer_id,
    media::mojom::VideoBufferHandlePtr buffer_handle) {
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VideoFrameReceiver::OnNewBuffer, receiver_,
                                buffer_id, std::move(buffer_handle)));
}

void VideoFrameReceiverOnTaskRunner::OnFrameReadyInBuffer(
    ReadyFrameInBuffer frame) {
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VideoFrameReceiver::OnFrameReadyInBuffer,
                                receiver_, std::move(frame)));
}

void VideoFrameReceiverOnTaskRunner::OnBufferRetired(int buffer_id) {
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VideoFrameReceiver::OnBufferRetired, receiver_,
                                buffer_id));
}

void VideoFrameReceiverOnTaskRunner::OnError(VideoCaptureError error) {
  task_runner_->PostTask(FROM_HERE, base::BindOnce(&VideoFrameReceiver::OnError,
                                                   receiver_, error));
}

void VideoFrameReceiverOnTaskRunner::OnFrameDropped(
    VideoCaptureFrameDropReason reason) {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VideoFrameReceiver::OnFrameDropped, receiver_, reason));
}

void VideoFrameReceiverOnTaskRunner::OnNewSubCaptureTargetVersion(
    uint32_t sub_capture_target_version) {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VideoFrameReceiver::OnNewSubCaptureTargetVersion,
                     receiver_, sub_capture_target_version));
}

void VideoFrameReceiverOnTaskRunner::OnFrameWithEmptyRegionCapture() {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VideoFrameReceiver::OnFrameWithEmptyRegionCapture,
                     receiver_));
}

void VideoFrameReceiverOnTaskRunner::OnLog(const std::string& message) {
  task_runner_->PostTask(FROM_HERE, base::BindOnce(&VideoFrameReceiver::OnLog,
                                                   receiver_, message));
}

void VideoFrameReceiverOnTaskRunner::OnStarted() {
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VideoFrameReceiver::OnStarted, receiver_));
}

void VideoFrameReceiverOnTaskRunner::OnStartedUsingGpuDecode() {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VideoFrameReceiver::OnStartedUsingGpuDecode, receiver_));
}

void VideoFrameReceiverOnTaskRunner::OnStopped() {
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VideoFrameReceiver::OnStopped, receiver_));
}

}  // namespace media
