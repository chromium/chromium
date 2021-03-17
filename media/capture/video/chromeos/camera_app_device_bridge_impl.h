// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_CHROMEOS_CAMERA_APP_DEVICE_BRIDGE_IMPL_H_
#define MEDIA_CAPTURE_VIDEO_CHROMEOS_CAMERA_APP_DEVICE_BRIDGE_IMPL_H_

#include <string>

#include "base/memory/singleton.h"
#include "media/capture/capture_export.h"
#include "media/capture/video/chromeos/camera_app_device_impl.h"
#include "media/capture/video/chromeos/mojom/camera_app.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace media {

// A singleton bridge class between Chrome Camera App and Video Capture Service
// which helps to construct CameraAppDevice for communication between these two
// components.
class CAPTURE_EXPORT CameraAppDeviceBridgeImpl
    : public cros::mojom::CameraAppDeviceBridge {
 public:
  using CameraInfoGetter =
      base::RepeatingCallback<cros::mojom::CameraInfoPtr(const std::string&)>;
  using VirtualDeviceController =
      base::RepeatingCallback<void(const std::string&, bool)>;

  CameraAppDeviceBridgeImpl();

  ~CameraAppDeviceBridgeImpl() override;

  static CameraAppDeviceBridgeImpl* GetInstance();

  void BindReceiver(
      mojo::PendingReceiver<cros::mojom::CameraAppDeviceBridge> receiver);

  void OnVideoCaptureDeviceCreated(
      const std::string& device_id,
      scoped_refptr<base::SingleThreadTaskRunner> ipc_task_runner);

  void OnVideoCaptureDeviceClosing(const std::string& device_id);

  void OnDeviceMojoDisconnected(const std::string& device_id);

  void SetCameraInfoGetter(CameraInfoGetter camera_info_getter);

  void UnsetCameraInfoGetter();

  void SetVirtualDeviceController(
      VirtualDeviceController virtual_device_controller);

  void UnsetVirtualDeviceController();

  base::WeakPtr<CameraAppDeviceImpl> GetWeakCameraAppDevice(
      const std::string& device_id);

  void RemoveCameraAppDevice(const std::string& device_id);

  // cros::mojom::CameraAppDeviceBridge implementations.
  void GetCameraAppDevice(const std::string& device_id,
                          GetCameraAppDeviceCallback callback) override;

  void IsSupported(IsSupportedCallback callback) override;

  void SetMultipleStreamsEnabled(
      const std::string& device_id,
      bool enabled,
      SetMultipleStreamsEnabledCallback callback) override;

 private:
  friend struct base::DefaultSingletonTraits<CameraAppDeviceBridgeImpl>;

  CameraAppDeviceImpl* GetOrCreateCameraAppDevice(const std::string& device_id);

  bool is_supported_;

  base::Lock camera_info_getter_lock_;
  CameraInfoGetter camera_info_getter_ GUARDED_BY(camera_info_getter_lock_);

  base::Lock virtual_device_controller_lock_;
  VirtualDeviceController virtual_device_controller_
      GUARDED_BY(virtual_device_controller_lock_);

  mojo::ReceiverSet<cros::mojom::CameraAppDeviceBridge> receivers_;

  base::Lock device_map_lock_;
  base::flat_map<std::string, std::unique_ptr<media::CameraAppDeviceImpl>>
      camera_app_devices_ GUARDED_BY(device_map_lock_);

  base::Lock task_runner_map_lock_;
  base::flat_map<std::string, scoped_refptr<base::SingleThreadTaskRunner>>
      ipc_task_runners_ GUARDED_BY(task_runner_map_lock_);

  DISALLOW_COPY_AND_ASSIGN(CameraAppDeviceBridgeImpl);
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_CHROMEOS_CAMERA_APP_DEVICE_BRIDGE_IMPL_H_
