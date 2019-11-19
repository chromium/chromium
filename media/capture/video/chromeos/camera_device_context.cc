// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/chromeos/camera_device_context.h"

namespace media {

CameraDeviceContext::CameraDeviceContext(
    std::unique_ptr<VideoCaptureDevice::Client> client)
    : state_(State::kStopped),
      sensor_orientation_(0),
      screen_rotation_(0),
      client_(std::move(client)) {
  DCHECK(client_);
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

CameraDeviceContext::~CameraDeviceContext() = default;

void CameraDeviceContext::SetState(State state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  state_ = state;
  if (state_ == State::kCapturing) {
    client_->OnStarted();
  }
}

CameraDeviceContext::State CameraDeviceContext::GetState() {
  return state_;
}

void CameraDeviceContext::SetErrorState(media::VideoCaptureError error,
                                        const base::Location& from_here,
                                        const std::string& reason) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  state_ = State::kError;
  LOG(ERROR) << reason;
  client_->OnError(error, from_here, reason);
}

void CameraDeviceContext::LogToClient(std::string message) {
  client_->OnLog(message);
}

void CameraDeviceContext::SubmitCapturedVideoCaptureBuffer(
    VideoCaptureDevice::Client::Buffer buffer,
    const VideoCaptureFormat& frame_format,
    base::TimeTicks reference_time,
    base::TimeDelta timestamp) {
  VideoFrameMetadata metadata;
  // All frames are pre-rotated to the display orientation.
  metadata.SetRotation(VideoFrameMetadata::Key::ROTATION,
                       VideoRotation::VIDEO_ROTATION_0);
  // TODO: Figure out the right color space for the camera frame.  We may need
  // to populate the camera metadata with the color space reported by the V4L2
  // device.
  client_->OnIncomingCapturedBufferExt(
      std::move(buffer), frame_format, gfx::ColorSpace(), reference_time,
      timestamp, gfx::Rect(frame_format.frame_size), std::move(metadata));
}

void CameraDeviceContext::SubmitCapturedGpuMemoryBuffer(
    gfx::GpuMemoryBuffer* buffer,
    const VideoCaptureFormat& frame_format,
    base::TimeTicks reference_time,
    base::TimeDelta timestamp) {
  client_->OnIncomingCapturedGfxBuffer(buffer, frame_format,
                                       GetCameraFrameOrientation(),
                                       reference_time, timestamp);
}

void CameraDeviceContext::SetSensorOrientation(int sensor_orientation) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(sensor_orientation >= 0 && sensor_orientation < 360 &&
         sensor_orientation % 90 == 0);
  sensor_orientation_ = sensor_orientation;
}

void CameraDeviceContext::SetScreenRotation(int screen_rotation) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(screen_rotation >= 0 && screen_rotation < 360 &&
         screen_rotation % 90 == 0);
  screen_rotation_ = screen_rotation;
}

int CameraDeviceContext::GetCameraFrameOrientation() {
  return (sensor_orientation_ + screen_rotation_) % 360;
}

bool CameraDeviceContext::ReserveVideoCaptureBufferFromPool(
    gfx::Size size,
    VideoPixelFormat format,
    VideoCaptureDevice::Client::Buffer* buffer) {
  // Use a dummy frame feedback id as we don't need it.
  constexpr int kDummyFrameFeedbackId = 0;
  auto result =
      client_->ReserveOutputBuffer(size, format, kDummyFrameFeedbackId, buffer);
  return result == VideoCaptureDevice::Client::ReserveResult::kSucceeded;
}

}  // namespace media
