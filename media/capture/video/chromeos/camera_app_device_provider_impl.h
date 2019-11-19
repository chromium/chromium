// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_CHROMEOS_CAMERA_APP_DEVICE_PROVIDER_IMPL_H_
#define MEDIA_CAPTURE_VIDEO_CHROMEOS_CAMERA_APP_DEVICE_PROVIDER_IMPL_H_

#include <string>

#include "media/capture/capture_export.h"
#include "media/capture/video/chromeos/mojom/camera_app.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace media {

class CAPTURE_EXPORT CameraAppDeviceProviderImpl
    : public cros::mojom::CameraAppDeviceProvider {
 public:
  using WithRealIdCallback =
      base::OnceCallback<void(const base::Optional<std::string>&)>;
  using DeviceIdMappingCallback =
      base::RepeatingCallback<void(const std::string&, WithRealIdCallback)>;

  CameraAppDeviceProviderImpl(
      mojo::PendingRemote<cros::mojom::CameraAppDeviceBridge> bridge,
      DeviceIdMappingCallback mapping_callback);
  ~CameraAppDeviceProviderImpl() override;

  // cros::mojom::CameraAppDeviceProvider implementations.
  void GetCameraAppDevice(const std::string& source_id,
                          GetCameraAppDeviceCallback callback) override;
  void IsSupported(IsSupportedCallback callback) override;

 private:
  void GetCameraAppDeviceWithDeviceId(
      GetCameraAppDeviceCallback callback,
      const base::Optional<std::string>& device_id);

  mojo::Remote<cros::mojom::CameraAppDeviceBridge> bridge_;

  DeviceIdMappingCallback mapping_callback_;

  base::WeakPtrFactory<CameraAppDeviceProviderImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CameraAppDeviceProviderImpl);
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_CHROMEOS_CAMERA_APP_DEVICE_PROVIDER_IMPL_H_