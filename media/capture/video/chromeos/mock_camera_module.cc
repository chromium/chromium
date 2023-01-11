// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/chromeos/mock_camera_module.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"

namespace media {
namespace unittest_internal {

MockCameraModule::MockCameraModule() : mock_module_thread_("MockModuleThread") {
  mock_module_thread_.Start();
}

MockCameraModule::~MockCameraModule() {
  mock_module_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&MockCameraModule::CloseBindingOnThread,
                                base::Unretained(this)));
  mock_module_thread_.Stop();
}

void MockCameraModule::OpenDevice(
    int32_t camera_id,
    mojo::PendingReceiver<cros::mojom::Camera3DeviceOps> device_ops_receiver,
    OpenDeviceCallback callback) {
  DoOpenDevice(camera_id, std::move(device_ops_receiver), callback);
}

void MockCameraModule::GetNumberOfCameras(GetNumberOfCamerasCallback callback) {
  DoGetNumberOfCameras(callback);
}

void MockCameraModule::GetCameraInfo(int32_t camera_id,
                                     GetCameraInfoCallback callback) {
  DoGetCameraInfo(camera_id, callback);
}

void MockCameraModule::SetCallbacks(
    mojo::PendingRemote<cros::mojom::CameraModuleCallbacks> callbacks,
    SetCallbacksCallback callback) {
  // Method deprecated and not expected to be called.
}

void MockCameraModule::Init(InitCallback callback) {
  DoInit(callback);
  std::move(callback).Run(0);
}

void MockCameraModule::SetTorchMode(int32_t camera_id,
                                    bool enabled,
                                    SetTorchModeCallback callback) {
  DoSetTorchMode(camera_id, enabled, callback);
  std::move(callback).Run(0);
}

void MockCameraModule::GetVendorTagOps(
    mojo::PendingReceiver<cros::mojom::VendorTagOps> vendor_tag_ops_receiver,
    GetVendorTagOpsCallback callback) {
  DoGetVendorTagOps(std::move(vendor_tag_ops_receiver), callback);
  std::move(callback).Run();
}

void MockCameraModule::SetCallbacksAssociated(
    mojo::PendingAssociatedRemote<cros::mojom::CameraModuleCallbacks> callbacks,
    SetCallbacksAssociatedCallback callback) {
  DoSetCallbacksAssociated(callbacks, callback);
  callbacks_.Bind(std::move(callbacks));
  std::move(callback).Run(0);
}
void MockCameraModule::NotifyCameraDeviceChange(
    int camera_id,
    cros::mojom::CameraDeviceStatus status) {
  mock_module_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&MockCameraModule::NotifyCameraDeviceChangeOnThread,
                     base::Unretained(this), camera_id, status));
}

void MockCameraModule::NotifyCameraDeviceChangeOnThread(
    int camera_id,
    cros::mojom::CameraDeviceStatus status) {
  callbacks_->CameraDeviceStatusChange(camera_id, status);
}

mojo::PendingRemote<cros::mojom::CameraModule>
MockCameraModule::GetPendingRemote() {
  base::WaitableEvent done(base::WaitableEvent::ResetPolicy::MANUAL,
                           base::WaitableEvent::InitialState::NOT_SIGNALED);
  mojo::PendingRemote<cros::mojom::CameraModule> pending_remote;
  mock_module_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&MockCameraModule::BindOnThread,
                                base::Unretained(this), base::Unretained(&done),
                                base::Unretained(&pending_remote)));
  done.Wait();
  return pending_remote;
}

void MockCameraModule::CloseBindingOnThread() {
  receiver_.reset();
  if (callbacks_.is_bound()) {
    callbacks_.reset();
  }
}

void MockCameraModule::BindOnThread(
    base::WaitableEvent* done,
    mojo::PendingRemote<cros::mojom::CameraModule>* pending_remote) {
  *pending_remote = receiver_.BindNewPipeAndPassRemote();
  done->Signal();
}

}  // namespace unittest_internal
}  // namespace media
