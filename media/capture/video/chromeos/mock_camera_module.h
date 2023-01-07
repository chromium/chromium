// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_CHROMEOS_MOCK_CAMERA_MODULE_H_
#define MEDIA_CAPTURE_VIDEO_CHROMEOS_MOCK_CAMERA_MODULE_H_

#include <stddef.h>
#include <stdint.h>

#include "base/threading/thread.h"
#include "media/capture/video/chromeos/mojom/camera3.mojom.h"
#include "media/capture/video/chromeos/mojom/camera_common.mojom.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {
namespace unittest_internal {

class MockCameraModule : public cros::mojom::CameraModule {
 public:
  MockCameraModule();

  MockCameraModule(const MockCameraModule&) = delete;
  MockCameraModule& operator=(const MockCameraModule&) = delete;

  ~MockCameraModule() override;

  void OpenDevice(
      int32_t camera_id,
      mojo::PendingReceiver<cros::mojom::Camera3DeviceOps> device_ops_receiver,
      OpenDeviceCallback callback) override;
  MOCK_METHOD3(DoOpenDevice,
               void(int32_t camera_id,
                    mojo::PendingReceiver<cros::mojom::Camera3DeviceOps>
                        device_ops_receiver,
                    OpenDeviceCallback& callback));

  void GetNumberOfCameras(GetNumberOfCamerasCallback callback) override;
  MOCK_METHOD1(DoGetNumberOfCameras,
               void(GetNumberOfCamerasCallback& callback));

  void GetCameraInfo(int32_t camera_id,
                     GetCameraInfoCallback callback) override;
  MOCK_METHOD2(DoGetCameraInfo,
               void(int32_t camera_id, GetCameraInfoCallback& callback));

  void SetCallbacks(
      mojo::PendingRemote<cros::mojom::CameraModuleCallbacks> callbacks,
      SetCallbacksCallback callback) override;
  MOCK_METHOD2(
      DoSetCallbacks,
      void(mojo::PendingRemote<cros::mojom::CameraModuleCallbacks>& callbacks,
           SetCallbacksCallback& callback));

  void Init(InitCallback callback) override;
  MOCK_METHOD1(DoInit, void(InitCallback& callback));

  void SetTorchMode(int32_t camera_id,
                    bool enabled,
                    SetTorchModeCallback callback) override;
  MOCK_METHOD3(DoSetTorchMode,
               void(int32_t camera_id,
                    bool enabled,
                    SetTorchModeCallback& callback));

  void GetVendorTagOps(
      mojo::PendingReceiver<cros::mojom::VendorTagOps> vendor_tag_ops_receiver,
      GetVendorTagOpsCallback callback) override;
  MOCK_METHOD2(DoGetVendorTagOps,
               void(mojo::PendingReceiver<cros::mojom::VendorTagOps>
                        vendor_tag_ops_receiver,
                    GetVendorTagOpsCallback& callback));

  void SetCallbacksAssociated(mojo::PendingAssociatedRemote<
                                  cros::mojom::CameraModuleCallbacks> callbacks,
                              SetCallbacksAssociatedCallback callback) override;
  MOCK_METHOD2(DoSetCallbacksAssociated,
               void(mojo::PendingAssociatedRemote<
                        cros::mojom::CameraModuleCallbacks>& callbacks,
                    SetCallbacksAssociatedCallback& callback));

  void NotifyCameraDeviceChange(int camera_id,
                                cros::mojom::CameraDeviceStatus status);

  mojo::PendingRemote<cros::mojom::CameraModule> GetPendingRemote();

 private:
  void NotifyCameraDeviceChangeOnThread(int camera_id,
                                        cros::mojom::CameraDeviceStatus status);

  void CloseBindingOnThread();

  void BindOnThread(
      base::WaitableEvent* done,
      mojo::PendingRemote<cros::mojom::CameraModule>* pending_remote);

  base::Thread mock_module_thread_;
  mojo::Receiver<cros::mojom::CameraModule> receiver_{this};
  mojo::AssociatedRemote<cros::mojom::CameraModuleCallbacks> callbacks_;
};

}  // namespace unittest_internal
}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_CHROMEOS_MOCK_CAMERA_MODULE_H_
