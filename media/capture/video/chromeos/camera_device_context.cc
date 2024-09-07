// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/chromeos/camera_device_context.h"

#include <string>

#include "base/strings/string_number_conversions.h"
#include "media/capture/video/chromeos/video_capture_features_chromeos.h"

namespace media {

CameraDeviceContext::CameraDeviceContext()
    : state_(State::kStopped),
      sensor_orientation_(0),
      screen_rotation_(0),
      frame_rotation_at_source_(true) {
  DETACH_FROM_SEQUENCE(sequence_checker_);

  base::SysInfo::GetHardwareInfo(base::BindOnce(
      &CameraDeviceContext::OnGotHardwareInfo, weak_ptr_factory_.GetWeakPtr()));
}

CameraDeviceContext::~CameraDeviceContext() = default;

bool CameraDeviceContext::AddClient(
    ClientType client_type,
    std::unique_ptr<VideoCaptureDevice::Client> client) {
  DCHECK(client);
  base::AutoLock lock(client_lock_);
  if (!clients_.insert({client_type, std::move(client)}).second) {
    SetErrorState(
        media::VideoCaptureError::kCrosHalV3DeviceContextDuplicatedClient,
        FROM_HERE,
        std::string("Duplicated client in camera device context: ") +
            base::NumberToString(static_cast<uint32_t>(client_type)));
    return false;
  }
  return true;
}

void CameraDeviceContext::RemoveClient(ClientType client_type) {
  base::AutoLock lock(client_lock_);
  auto client = clients_.find(client_type);
  if (client == clients_.end()) {
    return;
  }
  clients_.erase(client);
}

void CameraDeviceContext::SetState(State state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  state_ = state;
  if (state_ == State::kCapturing) {
    base::AutoLock lock(client_lock_);
    for (const auto& client : clients_) {
      client.second->OnStarted();
    }
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
  LOG(ERROR) << from_here.ToString() << ": " << reason;
  base::AutoLock lock(client_lock_);
  for (const auto& client : clients_) {
    client.second->OnError(error, from_here, reason);
  }
}

void CameraDeviceContext::LogToClient(std::string message) {
  base::AutoLock lock(client_lock_);
  for (const auto& client : clients_) {
    client.second->OnLog(message);
  }
}

void CameraDeviceContext::SubmitCapturedVideoCaptureBuffer(
    ClientType client_type,
    VideoCaptureDevice::Client::Buffer buffer,
    const VideoCaptureFormat& frame_format,
    base::TimeTicks reference_time,
    base::TimeDelta timestamp,
    const VideoFrameMetadata& metadata) {
  base::AutoLock lock(client_lock_);
  auto client = clients_.find(client_type);
  if (client == clients_.end()) {
    return;
  }

  client->second->OnIncomingCapturedBufferExt(
      std::move(buffer), frame_format,
      // TODO(b/322448224): It is a workaround and should be removed once we
      // have a general solution for b/40626119.
      color_space_override_.value_or(gfx::ColorSpace()), reference_time,
      timestamp, std::nullopt, gfx::Rect(frame_format.frame_size),
      std::move(metadata));
}

void CameraDeviceContext::SubmitCapturedGpuMemoryBuffer(
    ClientType client_type,
    gfx::GpuMemoryBuffer* buffer,
    const VideoCaptureFormat& frame_format,
    base::TimeTicks reference_time,
    base::TimeDelta timestamp) {
  base::AutoLock lock(client_lock_);
  auto client = clients_.find(client_type);
  if (client == clients_.end()) {
    return;
  }

  client->second->OnIncomingCapturedGfxBuffer(
      buffer, frame_format, GetCameraFrameRotation(), reference_time, timestamp,
      std::nullopt);
}

void CameraDeviceContext::SetSensorOrientation(int sensor_orientation) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(sensor_orientation >= 0 && sensor_orientation < 360 &&
         sensor_orientation % 90 == 0);
  base::AutoLock lock(rotation_state_lock_);
  sensor_orientation_ = sensor_orientation;
}

void CameraDeviceContext::SetScreenRotation(int screen_rotation) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(screen_rotation >= 0 && screen_rotation < 360 &&
         screen_rotation % 90 == 0);
  base::AutoLock lock(rotation_state_lock_);
  screen_rotation_ = screen_rotation;
}

void CameraDeviceContext::SetCameraFrameRotationEnabledAtSource(
    bool is_enabled) {
  base::AutoLock lock(rotation_state_lock_);
  frame_rotation_at_source_ = is_enabled;
}

int CameraDeviceContext::GetCameraFrameRotation() {
  base::AutoLock lock(rotation_state_lock_);
  return (sensor_orientation_ + screen_rotation_) % 360;
}

bool CameraDeviceContext::IsCameraFrameRotationEnabledAtSource() {
  base::AutoLock lock(rotation_state_lock_);
  return !base::FeatureList::IsEnabled(
             features::kDisableCameraFrameRotationAtSource) &&
         frame_rotation_at_source_;
}

bool CameraDeviceContext::ReserveVideoCaptureBufferFromPool(
    ClientType client_type,
    gfx::Size size,
    VideoPixelFormat format,
    VideoCaptureDevice::Client::Buffer* buffer,
    int* require_new_buffer_id,
    int* retire_old_buffer_id) {
  base::AutoLock lock(client_lock_);
  auto client = clients_.find(client_type);
  if (client == clients_.end()) {
    return false;
  }

  // Use a dummy frame feedback id as we don't need it.
  constexpr int kDummyFrameFeedbackId = 0;
  auto result = client->second->ReserveOutputBuffer(
      size, format, kDummyFrameFeedbackId, buffer, require_new_buffer_id,
      retire_old_buffer_id);
  return result == VideoCaptureDevice::Client::ReserveResult::kSucceeded;
}

bool CameraDeviceContext::HasClient() {
  base::AutoLock lock(client_lock_);
  return !clients_.empty();
}

void CameraDeviceContext::OnCaptureConfigurationChanged() {
  base::AutoLock lock(client_lock_);
  for (const auto& client : clients_) {
    client.second->OnCaptureConfigurationChanged();
  }
}

void CameraDeviceContext::OnGotHardwareInfo(
    base::SysInfo::HardwareInfo hardware_info) {
  base::AutoLock lock(client_lock_);
  if (hardware_info.model.starts_with("screebo")) {
    color_space_override_ = gfx::ColorSpace::CreateSRGB();
  }
}

}  // namespace media
