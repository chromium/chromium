// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_PUBLIC_CPP_GEOLOCATION_SYSTEM_GEOLOCATION_SOURCE_WIN_H_
#define SERVICES_DEVICE_PUBLIC_CPP_GEOLOCATION_SYSTEM_GEOLOCATION_SOURCE_WIN_H_

#include <windows.devices.geolocation.h>
#include <windows.security.authorization.appcapabilityaccess.h>
#include <wrl/client.h>

#include <memory>
#include <optional>
#include <string_view>

#include "base/memory/weak_ptr.h"
#include "base/threading/sequence_bound.h"
#include "services/device/public/cpp/geolocation/geolocation_system_permission_manager.h"
#include "services/device/public/cpp/geolocation/location_system_permission_status.h"
#include "services/device/public/cpp/geolocation/system_geolocation_source.h"

namespace device {

class COMPONENT_EXPORT(GEOLOCATION) SystemGeolocationSourceWin
    : public SystemGeolocationSource {
 public:
  static std::unique_ptr<GeolocationSystemPermissionManager>
  CreateGeolocationSystemPermissionManager();

  SystemGeolocationSourceWin();
  SystemGeolocationSourceWin(const SystemGeolocationSourceWin&) = delete;
  SystemGeolocationSourceWin& operator=(const SystemGeolocationSourceWin&) =
      delete;
  ~SystemGeolocationSourceWin() override;

  // SystemGeolocationSource implementation:
  void RegisterPermissionUpdateCallback(
      PermissionUpdateCallback callback) override;
  void OpenSystemPermissionSetting() override;
  void RequestPermission() override;

  // Handles system permission update from `AccessCheckHelper`.
  void OnPermissionStatusUpdated(LocationSystemPermissionStatus status);

 private:
  class AccessCheckHelper;

  void OnLaunchUriSuccess(uint8_t launched);
  void OnLaunchUriFailure(HRESULT result);

  void OnRequestLocationAccessSuccess(
      ABI::Windows::Devices::Geolocation::GeolocationAccessStatus
          geolocation_access_status);
  void OnRequestLocationAccessFailure(HRESULT result);

  // The pending operation for launching the settings page, or nullptr if not
  // launching the settings page.
  Microsoft::WRL::ComPtr<ABI::Windows::Foundation::IAsyncOperation<bool>>
      launch_uri_op_;

  // The pending operation for requesting location access, or nullptr if not
  // requesting access.
  Microsoft::WRL::ComPtr<ABI::Windows::Foundation::IAsyncOperation<
      ABI::Windows::Devices::Geolocation::GeolocationAccessStatus>>
      request_location_access_op_;

  // True if the user was prompted to grant location permissions and the
  // permission status has not yet changed. Used for metrics logging.
  bool has_pending_system_prompt_ = false;

  // Callback to invoke when the system permission status changes.
  PermissionUpdateCallback permission_update_callback_;

  // The current permission status, or nullopt if the status has not been
  // polled.
  std::optional<LocationSystemPermissionStatus> permission_status_;

  // Helper for checking location permission status on a dedicated COM STA
  // thread.
  base::SequenceBound<AccessCheckHelper> access_check_helper_;

  base::WeakPtrFactory<SystemGeolocationSourceWin> weak_factory_{this};
};

}  // namespace device

#endif  // SERVICES_DEVICE_PUBLIC_CPP_GEOLOCATION_SYSTEM_GEOLOCATION_SOURCE_WIN_H_
