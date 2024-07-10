// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/public/cpp/geolocation/system_geolocation_source_win.h"

#include <windows.foundation.h>
#include <windows.system.h>

#include <optional>
#include <string_view>

#include "base/memory/raw_ref.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/win/core_winrt_util.h"
#include "base/win/post_async_results.h"

namespace device {

namespace {

using ::ABI::Windows::Devices::Geolocation::GeolocationAccessStatus;
using ::ABI::Windows::Security::Authorization::AppCapabilityAccess::
    IAppCapability;
using ::Microsoft::WRL::ComPtr;

void RecordUmaInitialPermissionStatus(LocationSystemPermissionStatus status) {
  base::UmaHistogramEnumeration(
      "Geolocation.SystemGeolocationSourceWin.InitialPermissionStatus", status);
}

void RecordUmaPermissionStatusChanged(LocationSystemPermissionStatus status,
                                      bool after_prompt) {
  // We don't know what caused the permission status to change. Assume that the
  // first status change after showing the system permission prompt was caused
  // by the user interacting with the prompt.
  if (after_prompt) {
    base::UmaHistogramEnumeration(
        "Geolocation.SystemGeolocationSourceWin."
        "PermissionStatusChangedAfterPrompt",
        status);
  } else {
    base::UmaHistogramEnumeration(
        "Geolocation.SystemGeolocationSourceWin.PermissionStatusChanged",
        status);
  }
}

void RecordUmaCheckAccessError(HRESULT error) {
  base::UmaHistogramSparse(
      "Geolocation.SystemGeolocationSourceWin.CheckAccessError", error);
}

void RecordUmaCreateAppCapabilityError(HRESULT error) {
  base::UmaHistogramSparse(
      "Geolocation.SystemGeolocationSourceWin.CreateAppCapabilityError", error);
}

void RecordUmaLaunchSettingsResult(HRESULT result) {
  base::UmaHistogramSparse(
      "Geolocation.SystemGeolocationSourceWin.LaunchSettingsResult", result);
}

void RecordUmaRequestAccessResult(HRESULT result) {
  base::UmaHistogramSparse(
      "Geolocation.SystemGeolocationSourceWin.RequestAccessResult", result);
}

// Create an AppCapability object for the capability named `name`.
ComPtr<IAppCapability> CreateAppCapability(std::string_view name) {
  using ::ABI::Windows::Security::Authorization::AppCapabilityAccess::
      IAppCapabilityStatics;
  ComPtr<IAppCapabilityStatics> app_capability_statics;
  HRESULT hr = base::win::GetActivationFactory<
      IAppCapabilityStatics,
      RuntimeClass_Windows_Security_Authorization_AppCapabilityAccess_AppCapability>(
      &app_capability_statics);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to get IAppCapability statics: "
               << logging::SystemErrorCodeToString(hr);
    RecordUmaCreateAppCapabilityError(hr);
    return nullptr;
  }
  auto capability_name = base::win::ScopedHString::Create(name);
  ComPtr<IAppCapability> app_capability;
  hr = app_capability_statics->Create(capability_name.get(), &app_capability);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to create IAppCapability: "
               << logging::SystemErrorCodeToString(hr);
    RecordUmaCreateAppCapabilityError(hr);
    return nullptr;
  }
  return app_capability;
}

// Check the current access status for `app_capability` and return the
// equivalent LocationSystemPermissionStatus.
LocationSystemPermissionStatus GetLocationSystemPermissionStatus(
    ComPtr<IAppCapability> app_capability) {
  using ::ABI::Windows::Security::Authorization::AppCapabilityAccess::
      AppCapabilityAccessStatus;
  using ::ABI::Windows::Security::Authorization::AppCapabilityAccess::
      AppCapabilityAccessStatus_Allowed;
  using ::ABI::Windows::Security::Authorization::AppCapabilityAccess::
      AppCapabilityAccessStatus_UserPromptRequired;
  if (!app_capability) {
    return LocationSystemPermissionStatus::kNotDetermined;
  }
  AppCapabilityAccessStatus access_status;
  HRESULT hr = app_capability->CheckAccess(&access_status);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to get location access status: "
               << logging::SystemErrorCodeToString(hr);
    RecordUmaCheckAccessError(hr);
    return LocationSystemPermissionStatus::kNotDetermined;
  }
  if (access_status == AppCapabilityAccessStatus_Allowed) {
    return LocationSystemPermissionStatus::kAllowed;
  }
  if (access_status == AppCapabilityAccessStatus_UserPromptRequired) {
    return LocationSystemPermissionStatus::kNotDetermined;
  }
  return LocationSystemPermissionStatus::kDenied;
}

}  // namespace

SystemGeolocationSourceWin::SystemGeolocationSourceWin()
    : location_capability_(CreateAppCapability("location")) {
  if (location_capability_) {
    PollPermissionStatus();
    RecordUmaInitialPermissionStatus(permission_status_.value());
  }
}

SystemGeolocationSourceWin::~SystemGeolocationSourceWin() = default;

// static
std::unique_ptr<GeolocationSystemPermissionManager>
SystemGeolocationSourceWin::CreateGeolocationSystemPermissionManager() {
  return std::make_unique<GeolocationSystemPermissionManager>(
      std::make_unique<SystemGeolocationSourceWin>());
}

void SystemGeolocationSourceWin::RegisterPermissionUpdateCallback(
    PermissionUpdateCallback callback) {
  permission_update_callback_ = std::move(callback);
  if (permission_status_.has_value()) {
    permission_update_callback_.Run(permission_status_.value());
  }
}

void SystemGeolocationSourceWin::PollPermissionStatus() {
  // Poll the current permission status and notify if the status has changed.
  auto status = GetLocationSystemPermissionStatus(location_capability_);
  if (status != permission_status_) {
    permission_status_ = status;
    if (permission_update_callback_) {
      permission_update_callback_.Run(status);
    }
    RecordUmaPermissionStatusChanged(status, has_pending_system_prompt_);
    has_pending_system_prompt_ = false;
  }

  // Schedule next poll.
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&SystemGeolocationSourceWin::PollPermissionStatus,
                     weak_factory_.GetWeakPtr()),
      base::Milliseconds(500));
}

void SystemGeolocationSourceWin::OnLaunchUriSuccess(uint8_t launched) {
  RecordUmaLaunchSettingsResult(S_OK);
  launch_uri_op_.Reset();
}

void SystemGeolocationSourceWin::OnLaunchUriFailure(HRESULT result) {
  LOG(ERROR) << "LaunchUriAsync failed: "
             << logging::SystemErrorCodeToString(result);
  RecordUmaLaunchSettingsResult(result);
  launch_uri_op_.Reset();
}

void SystemGeolocationSourceWin::OpenSystemPermissionSetting() {
  using ::ABI::Windows::Foundation::IUriRuntimeClass;
  using ::ABI::Windows::Foundation::IUriRuntimeClassFactory;
  using ::ABI::Windows::System::ILauncherStatics;
  if (launch_uri_op_) {
    return;
  }
  ComPtr<IUriRuntimeClassFactory> uri_runtime_class_factory;
  HRESULT hr =
      base::win::GetActivationFactory<IUriRuntimeClassFactory,
                                      RuntimeClass_Windows_Foundation_Uri>(
          &uri_runtime_class_factory);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to get IUriRuntimeClassFactory: "
               << logging::SystemErrorCodeToString(hr);
    RecordUmaLaunchSettingsResult(hr);
    return;
  }
  ComPtr<IUriRuntimeClass> uri_runtime_class;
  auto uri_string =
      base::win::ScopedHString::Create("ms-settings:privacy-location");
  hr = uri_runtime_class_factory->CreateUri(uri_string.get(),
                                            &uri_runtime_class);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to create IUriRuntimeClass: "
               << logging::SystemErrorCodeToString(hr);
    RecordUmaLaunchSettingsResult(hr);
    return;
  }
  ComPtr<ILauncherStatics> launcher_statics;
  hr = base::win::GetActivationFactory<ILauncherStatics,
                                       RuntimeClass_Windows_System_Launcher>(
      &launcher_statics);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to get ILauncher statics: "
               << logging::SystemErrorCodeToString(hr);
    RecordUmaLaunchSettingsResult(hr);
    return;
  }
  hr = launcher_statics->LaunchUriAsync(uri_runtime_class.Get(),
                                        &launch_uri_op_);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to launch URI: "
               << logging::SystemErrorCodeToString(hr);
    RecordUmaLaunchSettingsResult(hr);
    return;
  }
  hr = base::win::PostAsyncHandlers(
      launch_uri_op_.Get(),
      base::BindOnce(&SystemGeolocationSourceWin::OnLaunchUriSuccess,
                     weak_factory_.GetWeakPtr()),
      base::BindOnce(&SystemGeolocationSourceWin::OnLaunchUriFailure,
                     weak_factory_.GetWeakPtr()));
  if (FAILED(hr)) {
    LOG(ERROR) << "PostAsyncHandlers failed: "
               << logging::SystemErrorCodeToString(hr);
    RecordUmaLaunchSettingsResult(hr);
  }
}

void SystemGeolocationSourceWin::RequestPermission() {
  using ::ABI::Windows::Devices::Geolocation::IGeolocatorStatics;
  if (request_location_access_op_) {
    return;
  }
  ComPtr<IGeolocatorStatics> geolocator_statics;
  HRESULT hr = base::win::GetActivationFactory<
      IGeolocatorStatics, RuntimeClass_Windows_Devices_Geolocation_Geolocator>(
      &geolocator_statics);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to get IGeolocator statics: "
               << logging::SystemErrorCodeToString(hr);
    RecordUmaRequestAccessResult(hr);
    return;
  }
  // Geolocator::RequestAccessAsync will trigger the one-time-per-app prompt.
  hr = geolocator_statics->RequestAccessAsync(&request_location_access_op_);
  if (FAILED(hr)) {
    LOG(ERROR) << "Location access request failed: "
               << logging::SystemErrorCodeToString(hr);
    RecordUmaRequestAccessResult(hr);
    return;
  }
  hr = base::win::PostAsyncHandlers(
      request_location_access_op_.Get(),
      base::BindOnce(
          &SystemGeolocationSourceWin::OnRequestLocationAccessSuccess,
          weak_factory_.GetWeakPtr()),
      base::BindOnce(
          &SystemGeolocationSourceWin::OnRequestLocationAccessFailure,
          weak_factory_.GetWeakPtr()));
  if (FAILED(hr)) {
    LOG(ERROR) << "PostAsyncHandlers failed: "
               << logging::SystemErrorCodeToString(hr);
    RecordUmaRequestAccessResult(hr);
    return;
  }
  has_pending_system_prompt_ = true;
}

void SystemGeolocationSourceWin::OnRequestLocationAccessSuccess(
    GeolocationAccessStatus status) {
  RecordUmaRequestAccessResult(S_OK);
  request_location_access_op_.Reset();
}

void SystemGeolocationSourceWin::OnRequestLocationAccessFailure(
    HRESULT result) {
  LOG(ERROR) << "RequestLocationAccess failed: "
             << logging::SystemErrorCodeToString(result);
  RecordUmaRequestAccessResult(result);
  request_location_access_op_.Reset();
}

}  // namespace device
