// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_CHROMEOS_CAMERA_APP_DEVICE_PROVIDER_IMPL_H_
#define MEDIA_CAPTURE_VIDEO_CHROMEOS_CAMERA_APP_DEVICE_PROVIDER_IMPL_H_

#include <string>

#include "media/capture/capture_export.h"
#include "media/capture/video/chromeos/mojom/camera_app.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace media {

class CAPTURE_EXPORT CameraAppDeviceProviderImpl
    : public cros::mojom::CameraAppDeviceProvider {
 public:
  using WithRealIdCallback =
      base::OnceCallback<void(const std::optional<std::string>&)>;
  using DeviceIdMappingCallback =
      base::RepeatingCallback<void(const std::string&, WithRealIdCallback)>;
  using ConnectToBridgeCallback = base::RepeatingCallback<void(
      mojo::PendingReceiver<cros::mojom::CameraAppDeviceBridge>)>;

  CameraAppDeviceProviderImpl(
      ConnectToBridgeCallback connect_to_bridge_callback,
      DeviceIdMappingCallback mapping_callback);

  CameraAppDeviceProviderImpl(const CameraAppDeviceProviderImpl&) = delete;
  CameraAppDeviceProviderImpl& operator=(const CameraAppDeviceProviderImpl&) =
      delete;

  ~CameraAppDeviceProviderImpl() override;
  void Bind(
      mojo::PendingReceiver<cros::mojom::CameraAppDeviceProvider> receiver);

  // cros::mojom::CameraAppDeviceProvider implementations.
  void GetCameraAppDevice(const std::string& source_id,
                          GetCameraAppDeviceCallback callback) override;
  void IsSupported(IsSupportedCallback callback) override;
  void IsDeviceInUse(const std::string& source_id,
                     IsDeviceInUseCallback callback) override;

 private:
  void GetCameraAppDeviceWithDeviceId(
      GetCameraAppDeviceCallback callback,
      const std::optional<std::string>& device_id);

  void IsDeviceInUseWithDeviceId(IsDeviceInUseCallback callback,
                                 const std::optional<std::string>& device_id);

  void ConnectToCameraAppDeviceBridge();

  ConnectToBridgeCallback connect_to_bridge_callback_;

  DeviceIdMappingCallback mapping_callback_;

  mojo::Remote<cros::mojom::CameraAppDeviceBridge> bridge_;

  mojo::Receiver<cros::mojom::CameraAppDeviceProvider> receiver_{this};

  base::WeakPtrFactory<CameraAppDeviceProviderImpl> weak_ptr_factory_;
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_CHROMEOS_CAMERA_APP_DEVICE_PROVIDER_IMPL_H_
