// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webrtc/mock_capture_device_controller.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"

namespace content {

MockCaptureDeviceController::MockCaptureDeviceController(
    scoped_refptr<base::SequencedTaskRunner> video_capture_service_task_runner,
    ConnectToVideoSourceProviderCallback
        connect_to_video_source_provider_callback)
    : video_capture_service_task_runner_(
          std::move(video_capture_service_task_runner)),
      connect_to_video_source_provider_callback_(
          std::move(connect_to_video_source_provider_callback)) {
  DCHECK(video_capture_service_task_runner_);
  DCHECK(!connect_to_video_source_provider_callback_.is_null());
}

MockCaptureDeviceController::~MockCaptureDeviceController() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Destroy cameras before closing the provider remote. Destroying each
  // MockCameraDevice closes its Producer / SharedMemoryVirtualDevice endpoints,
  // which unregisters the virtual camera.
  cameras_.clear();
  source_provider_.reset();
}

void MockCaptureDeviceController::AddMockCamera(MockCameraConfig config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!config.device_id.empty());

  EnsureConnectedToVideoSourceProvider();

  const std::string device_id = config.device_id;

  // Treat adding the same device id as replacement.
  cameras_.erase(device_id);

  auto camera = std::make_unique<MockCameraDevice>(std::move(config));

  source_provider_->AddSharedMemoryVirtualDevice(camera->BuildDeviceInfo(),
                                                 camera->BindProducerRemote(),
                                                 camera->BindDeviceReceiver());

  // Shared-memory virtual devices are producer-driven. Start the low-rate
  // frame pump after registration so getUserMedia() receives real frames.
  camera->StartProducingFrames();

  cameras_[device_id] = std::move(camera);
}

void MockCaptureDeviceController::RemoveMockCamera(
    const std::string& device_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  cameras_.erase(device_id);
}

void MockCaptureDeviceController::Reset() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  cameras_.clear();
}

void MockCaptureDeviceController::EnsureConnectedToVideoSourceProvider() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (source_provider_.is_bound()) {
    return;
  }

  mojo::PendingReceiver<video_capture::mojom::VideoSourceProvider> receiver =
      source_provider_.BindNewPipeAndPassReceiver();

  video_capture_service_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(connect_to_video_source_provider_callback_,
                                std::move(receiver)));
}

}  // namespace content
