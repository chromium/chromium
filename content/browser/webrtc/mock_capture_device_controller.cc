// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webrtc/mock_capture_device_controller.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/task/bind_post_task.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"

namespace content {
namespace {

using RegistrationResult =
    video_capture::mojom::VideoSourceProvider::AddVirtualDeviceResult;
using AddResult = MockCaptureDeviceController::AddMockCameraResult;

AddResult ToAddMockCameraResult(RegistrationResult result) {
  switch (result) {
    case RegistrationResult::kSuccess:
      return AddResult::kSuccess;
    case RegistrationResult::kErrorInvalidDeviceInfo:
      return AddResult::kInvalidDeviceInfo;
    case RegistrationResult::kErrorHardwareDeviceIdCollision:
      return AddResult::kHardwareDeviceIdCollision;
    case RegistrationResult::kErrorNotSupported:
      return AddResult::kNotSupported;
    case RegistrationResult::kErrorDroppedRequest:
      return AddResult::kAborted;
  }
  NOTREACHED();
}

}  // namespace

MockCaptureDeviceController::MockCaptureDeviceController(
    scoped_refptr<base::SequencedTaskRunner> video_capture_service_task_runner,
    ConnectToVideoSourceProviderCallback
        connect_to_video_source_provider_callback)
    : video_capture_service_task_runner_(
          std::move(video_capture_service_task_runner)),
      connect_to_video_source_provider_callback_(
          std::move(connect_to_video_source_provider_callback)) {
  CHECK(video_capture_service_task_runner_, base::NotFatalUntil::M160);
  CHECK(!connect_to_video_source_provider_callback_.is_null(),
        base::NotFatalUntil::M160);
}

MockCaptureDeviceController::~MockCaptureDeviceController() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Resetting the remote may synchronously invoke a default reply callback.
  weak_factory_.InvalidateWeakPtrs();
  Reset();
  source_provider_.reset();
}

void MockCaptureDeviceController::AddMockCamera(
    MockCameraConfig config,
    AddMockCameraCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!config.device_id.empty(), base::NotFatalUntil::M160);

  // Post client callbacks so completion and cancellation cannot re-enter this
  // controller, including while it is being destroyed.
  if (callback) {
    callback = base::BindPostTaskToCurrentDefault(std::move(callback));
  }

  if (cameras_.contains(config.device_id)) {
    if (callback) {
      std::move(callback).Run(AddMockCameraResult::kAlreadyExists);
    }
    return;
  }

  EnsureConnectedToVideoSourceProvider();

  const std::string device_id = config.device_id;
  auto [it, inserted] = cameras_.try_emplace(device_id);
  DCHECK(inserted);
  CameraEntry& entry = it->second;
  entry.registration_id = ++next_registration_id_;
  entry.camera = std::make_unique<MockCameraDevice>(std::move(config));
  entry.callback = std::move(callback);

  source_provider_->AddSharedMemoryVirtualDevice(
      entry.camera->BuildDeviceInfo(), entry.camera->BindProducerRemote(),
      entry.camera->BindDeviceReceiver(),
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          base::BindOnce(&MockCaptureDeviceController::OnCameraRegistered,
                         weak_factory_.GetWeakPtr(), device_id,
                         entry.registration_id),
          RegistrationResult::kErrorDroppedRequest));
}

void MockCaptureDeviceController::OnCameraRegistered(
    const std::string& device_id,
    uint64_t registration_id,
    RegistrationResult result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = cameras_.find(device_id);
  if (it == cameras_.end() || it->second.registration_id != registration_id) {
    // Ignore a reply for a cancelled registration. The same ID may now
    // belong to a new registration after RemoveMockCamera() or Reset().
    return;
  }

  auto callback = std::move(it->second.callback);
  if (result == RegistrationResult::kSuccess) {
    it->second.camera->StartProducingFrames();
  } else {
    // Remove the failed attempt before notifying callers so the ID can be used
    // by a subsequent request.
    cameras_.erase(it);
  }
  if (callback) {
    std::move(callback).Run(ToAddMockCameraResult(result));
  }
}

void MockCaptureDeviceController::RemoveMockCamera(
    const std::string& device_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = cameras_.find(device_id);
  if (it == cameras_.end()) {
    return;
  }
  auto callback = std::move(it->second.callback);
  cameras_.erase(it);
  if (callback) {
    std::move(callback).Run(AddMockCameraResult::kAborted);
  }
}

void MockCaptureDeviceController::Reset() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto cameras = std::exchange(cameras_, {});
  for (auto& [device_id, entry] : cameras) {
    entry.camera.reset();
    if (entry.callback) {
      std::move(entry.callback).Run(AddMockCameraResult::kAborted);
    }
  }
}

void MockCaptureDeviceController::OnVideoSourceProviderDisconnected() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  source_provider_.reset();
  Reset();
  // Existing cameras are not restored automatically. A later explicit add can
  // establish a new provider connection.
}

void MockCaptureDeviceController::EnsureConnectedToVideoSourceProvider() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (source_provider_.is_bound()) {
    return;
  }

  auto receiver = source_provider_.BindNewPipeAndPassReceiver();
  source_provider_.set_disconnect_handler(base::BindOnce(
      &MockCaptureDeviceController::OnVideoSourceProviderDisconnected,
      weak_factory_.GetWeakPtr()));

  video_capture_service_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(connect_to_video_source_provider_callback_,
                                std::move(receiver)));
}

}  // namespace content
