// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/video_capture_device_proxy_lacros.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/check_op.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/token.h"
#include "chromeos/crosapi/mojom/screen_manager.mojom.h"
#include "chromeos/crosapi/mojom/video_capture.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "content/public/browser/desktop_media_id.h"
#include "media/capture/mojom/video_capture_types.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/device/public/mojom/wake_lock.mojom.h"
#include "services/device/public/mojom/wake_lock_provider.mojom.h"
#include "services/video_capture/lacros/video_frame_handler_proxy_lacros.h"
#include "services/video_capture/public/cpp/receiver_media_to_mojo_adapter.h"

namespace content {

namespace {

const int kVideoCaptureMinVersion = crosapi::mojom::ScreenManager::
    MethodMinVersions::kGetScreenVideoCapturerMinVersion;
const int kRequestRefreshFrameMinVersion = crosapi::mojom::VideoCaptureDevice::
    MethodMinVersions::kRequestRefreshFrameMinVersion;

void BindWakeLockProvider(
    mojo::PendingReceiver<device::mojom::WakeLockProvider> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  GetDeviceService().BindWakeLockProvider(std::move(receiver));
}

}  // namespace

// static
bool VideoCaptureDeviceProxyLacros::IsAvailable() {
  auto* service = chromeos::LacrosService::Get();

  if (!service)
    return false;

  return service->GetInterfaceVersion(crosapi::mojom::ScreenManager::Uuid_) >=
         kVideoCaptureMinVersion;
}

VideoCaptureDeviceProxyLacros::VideoCaptureDeviceProxyLacros(
    const DesktopMediaID& device_id)
    : capture_id_(device_id) {
  CHECK(IsAvailable());
  CHECK(capture_id_.type == DesktopMediaID::TYPE_SCREEN ||
        capture_id_.type == DesktopMediaID::TYPE_WINDOW);

  // The LacrosService exists at all times except during early start-up and
  // late shut-down. This class should never be used in those two times.
  auto* lacros_service = chromeos::LacrosService::Get();
  DCHECK(lacros_service);
  DCHECK(lacros_service->IsAvailable<crosapi::mojom::ScreenManager>());
  lacros_service->BindScreenManagerReceiver(
      screen_manager_.BindNewPipeAndPassReceiver());

  screen_manager_.set_disconnect_handler(base::BindOnce(
      &VideoCaptureDeviceProxyLacros::OnFatalError, base::Unretained(this),
      "Mojo connection to screen manager was closed"));
}

VideoCaptureDeviceProxyLacros::~VideoCaptureDeviceProxyLacros() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!receiver_adapter_)
      << "StopAndDeAllocate() was never called after start.";
}

void VideoCaptureDeviceProxyLacros::AllocateAndStartWithReceiver(
    const media::VideoCaptureParams& params,
    std::unique_ptr<media::VideoFrameReceiver> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!device_);

  // If the device has already ended on a fatal error, or screen_manager_ was
  // disconnected, abort immediately.
  if (fatal_error_message_) {
    receiver->OnLog(*fatal_error_message_);
    receiver->OnError(
        media::VideoCaptureError::
            kLacrosVideoCaptureDeviceProxyAlreadyEndedOnFatalError);
    return;
  }

  switch (capture_id_.type) {
    case DesktopMediaID::TYPE_SCREEN:
      screen_manager_->GetScreenVideoCapturer(
          device_.BindNewPipeAndPassReceiver(), capture_id_.id);
      break;
    case DesktopMediaID::TYPE_WINDOW:
      screen_manager_->GetWindowVideoCapturer(
          device_.BindNewPipeAndPassReceiver(), capture_id_.id);
      break;
    case DesktopMediaID::TYPE_NONE:
    case DesktopMediaID::TYPE_WEB_CONTENTS:
      LOG(FATAL) << "Unknown Type: " << capture_id_.type;
  }

  device_.set_disconnect_handler(base::BindOnce(
      &VideoCaptureDeviceProxyLacros::OnFatalError, base::Unretained(this),
      "Mojom connection to device was closed"));

  // Note that currently all versioned calls that we need to make are
  // best effort, and can just be dropped if we haven't gotten an updated
  // version yet. If that changes, we'll need to track that we have an
  // outstanding query and respond accordingly.
  device_.QueryVersion(base::DoNothing());

  // Adapt the media::VideoFrameReceiver we've received to a
  // crosapi::mojom::VideoFrameHandler remote that we can pass over crosapi to
  // let ash-chrome pass us captured frames.
  mojo::PendingRemote<crosapi::mojom::VideoFrameHandler>
      pending_crosapi_remote_proxy;
  receiver_adapter_ =
      std::make_unique<video_capture::ReceiverMediaToCrosapiAdapter>(
          pending_crosapi_remote_proxy.InitWithNewPipeAndPassReceiver(),
          std::move(receiver));

  device_->Start(params, std::move(pending_crosapi_remote_proxy));

  DCHECK(!wake_lock_);
  RequestWakeLock();
}

void VideoCaptureDeviceProxyLacros::RequestRefreshFrame() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (device_ && device_.version() >= kRequestRefreshFrameMinVersion)
    device_->RequestRefreshFrame();
}

void VideoCaptureDeviceProxyLacros::MaybeSuspend() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (device_)
    device_->MaybeSuspend();
}

void VideoCaptureDeviceProxyLacros::Resume() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (device_)
    device_->Resume();
}

void VideoCaptureDeviceProxyLacros::Crop(
    const base::Token& crop_id,
    uint32_t crop_version,
    base::OnceCallback<void(media::mojom::CropRequestResult)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback);

  std::move(callback).Run(
      media::mojom::CropRequestResult::kUnsupportedCaptureDevice);
}

void VideoCaptureDeviceProxyLacros::StopAndDeAllocate() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (wake_lock_) {
    wake_lock_->CancelWakeLock();
    wake_lock_.reset();
  }

  device_.reset();
  receiver_adapter_.reset();
}

void VideoCaptureDeviceProxyLacros::GetPhotoState(
    GetPhotoStateCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (device_)
    device_->GetPhotoState(std::move(callback));
}

void VideoCaptureDeviceProxyLacros::SetPhotoOptions(
    media::mojom::PhotoSettingsPtr settings,
    SetPhotoOptionsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (device_)
    device_->SetPhotoOptions(std::move(settings), std::move(callback));
}

void VideoCaptureDeviceProxyLacros::TakePhoto(TakePhotoCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (device_)
    device_->TakePhoto(std::move(callback));
}

void VideoCaptureDeviceProxyLacros::OnUtilizationReport(
    media::VideoCaptureFeedback feedback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (device_)
    device_->ProcessFeedback(std::move(feedback));
}

void VideoCaptureDeviceProxyLacros::OnFatalError(std::string message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  fatal_error_message_ = std::move(message);
  if (receiver_adapter_) {
    receiver_adapter_->OnLog(*fatal_error_message_);
    receiver_adapter_->OnError(
        media::VideoCaptureError::
            kLacrosVideoCaptureDeviceProxyEncounteredFatalError);
  }

  StopAndDeAllocate();
}

void VideoCaptureDeviceProxyLacros::RequestWakeLock() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  mojo::Remote<device::mojom::WakeLockProvider> wake_lock_provider;
  auto receiver = wake_lock_provider.BindNewPipeAndPassReceiver();
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&BindWakeLockProvider, std::move(receiver)));
  wake_lock_provider->GetWakeLockWithoutContext(
      device::mojom::WakeLockType::kPreventDisplaySleep,
      device::mojom::WakeLockReason::kOther, "screen capture",
      wake_lock_.BindNewPipeAndPassReceiver());

  wake_lock_->RequestWakeLock();
}

void VideoCaptureDeviceProxyLacros::AllocateAndStart(
    const media::VideoCaptureParams& params,
    std::unique_ptr<media::VideoCaptureDevice::Client> client) {
  // VideoCaptureDeviceProxyLacros does not use a
  // VideoCaptureDevice::Client. Instead, it provides frames to a
  // VideoFrameReceiver directly.
  NOTREACHED();
}

}  // namespace content
