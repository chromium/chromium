// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_CHROMEOS_CAMERA_APP_DEVICE_BRIDGE_IMPL_H_
#define MEDIA_CAPTURE_VIDEO_CHROMEOS_CAMERA_APP_DEVICE_BRIDGE_IMPL_H_

#include <string>

#include "media/capture/capture_export.h"
#include "media/capture/video/chromeos/camera_app_device_impl.h"
#include "media/capture/video/chromeos/mojom/camera_app.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace media {

// A bridge class which helps to construct the connection of CameraAppDevice
// between remote side (Chrome) and receiver side (Video Capture Service).
class CAPTURE_EXPORT CameraAppDeviceBridgeImpl
    : public cros::mojom::CameraAppDeviceBridge {
 public:
  using CameraInfoGetter =
      base::RepeatingCallback<cros::mojom::CameraInfoPtr(const std::string&)>;

  CameraAppDeviceBridgeImpl();

  ~CameraAppDeviceBridgeImpl() override;

  void SetIsSupported(bool is_supported);

  void BindReceiver(
      mojo::PendingReceiver<cros::mojom::CameraAppDeviceBridge> receiver);

  void OnDeviceClosed(const std::string& device_id);

  void SetCameraInfoGetter(CameraInfoGetter camera_info_getter);

  void UnsetCameraInfoGetter();

  CameraAppDeviceImpl* GetCameraAppDevice(const std::string& device_id);

  // cros::mojom::CameraAppDeviceBridge implementations.
  void GetCameraAppDevice(const std::string& device_id,
                          GetCameraAppDeviceCallback callback) override;

  void IsSupported(IsSupportedCallback callback) override;

 private:
  CameraAppDeviceImpl* CreateCameraAppDevice(const std::string& device_id);

  bool is_supported_;

  CameraInfoGetter camera_info_getter_;

  mojo::ReceiverSet<cros::mojom::CameraAppDeviceBridge> receivers_;

  base::flat_map<std::string, std::unique_ptr<media::CameraAppDeviceImpl>>
      camera_app_devices_;

  DISALLOW_COPY_AND_ASSIGN(CameraAppDeviceBridgeImpl);
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_CHROMEOS_CAMERA_APP_DEVICE_BRIDGE_IMPL_H_