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
#include "base/task/thread_pool.h"
#include "base/win/core_winrt_util.h"
#include "base/win/post_async_results.h"
#include "services/device/public/cpp/device_features.h"

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

}  // namespace

class SystemGeolocationSourceWin::AccessCheckHelper {
 public:
  AccessCheckHelper(
      base::WeakPtr<SystemGeolocationSourceWin> source,
      scoped_refptr<base::SequencedTaskRunner> source_task_runner);

  // This function periodically checks and informs `source_` about the system's
  // geolocation permission status. It handles potential access failures and
  // ensures `source_` is always updated on any permission changes by scheduling
  // regular checks.
  void PollPermissionStatus();

 private:
  // Minimum and maximum polling intervals (in milliseconds). Any value fetched
  // from Finch config (features::kWinSystemLocationPermissionPollingParam) will
  // be clamped within this range to ensure reasonable polling frequency
  static constexpr int kMinPollingIntervalMs = 500;
  static constexpr int kMaxPollingIntervalMs = 3000;
  // The interval (in milliseconds) at which to poll for permission changes.
  const int polling_interval_;
  // COM interface to check the app's capability to access location.
  ComPtr<IAppCapability> location_capability_;
  base::WeakPtr<SystemGeolocationSourceWin> source_;
  scoped_refptr<base::SequencedTaskRunner> source_task_runner_;
  base::WeakPtrFactory<SystemGeolocationSourceWin::AccessCheckHelper>
      weak_ptr_factory_{this};
};

SystemGeolocationSourceWin::AccessCheckHelper::AccessCheckHelper(
    base::WeakPtr<SystemGeolocationSourceWin> source,
    scoped_refptr<base::SequencedTaskRunner> source_task_runner)
    : polling_interval_(
          std::clamp(features::kWinSystemLocationPermissionPollingParam.Get(),
                     kMinPollingIntervalMs,
                     kMaxPollingIntervalMs)),
      location_capability_(CreateAppCapability("location")),
      source_(source),
      source_task_runner_(source_task_runner) {
  PollPermissionStatus();
}

void SystemGeolocationSourceWin::AccessCheckHelper::PollPermissionStatus() {
  using ::ABI::Windows::Security::Authorization::AppCapabilityAccess::
      AppCapabilityAccessStatus;
  using ::ABI::Windows::Security::Authorization::AppCapabilityAccess::
      AppCapabilityAccessStatus_Allowed;
  using ::ABI::Windows::Security::Authorization::AppCapabilityAccess::
      AppCapabilityAccessStatus_UserPromptRequired;

  // If the location capability object is not available (potentially due
  // to initialization failure or other issues), post a task to notify the
  // SystemGeolocationSourceWin that the permission status is 'kNotDetermined'
  // and exit the function early without scheduling next poll.
  if (!location_capability_) {
    source_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&SystemGeolocationSourceWin::OnPermissionStatusUpdated,
                       source_,
                       LocationSystemPermissionStatus::kNotDetermined));
    return;
  }

  LocationSystemPermissionStatus status =
      LocationSystemPermissionStatus::kDenied;
  AppCapabilityAccessStatus access_status;
  HRESULT hr = location_capability_->CheckAccess(&access_status);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to get location access status: "
               << logging::SystemErrorCodeToString(hr);
    RecordUmaCheckAccessError(hr);
    status = LocationSystemPermissionStatus::kNotDetermined;
  } else if (access_status == AppCapabilityAccessStatus_Allowed) {
    status = LocationSystemPermissionStatus::kAllowed;
  } else if (access_status == AppCapabilityAccessStatus_UserPromptRequired) {
    status = LocationSystemPermissionStatus::kNotDetermined;
  }

  source_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&SystemGeolocationSourceWin::OnPermissionStatusUpdated,
                     source_, status));

  // Schedule next poll.
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          &SystemGeolocationSourceWin::AccessCheckHelper::PollPermissionStatus,
          weak_ptr_factory_.GetWeakPtr()),
      base::Milliseconds(polling_interval_));
}

SystemGeolocationSourceWin::SystemGeolocationSourceWin() {
  access_check_helper_ = base::SequenceBound<AccessCheckHelper>(
      base::ThreadPool::CreateCOMSTATaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE}),
      weak_factory_.GetWeakPtr(),
      base::SequencedTaskRunner::GetCurrentDefault());
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

void SystemGeolocationSourceWin::OnPermissionStatusUpdated(
    LocationSystemPermissionStatus status) {
  // Record the first polled permission status.
  if (!permission_status_.has_value()) {
    RecordUmaInitialPermissionStatus(status);
  }

  // Handle permission status change.
  if (status != permission_status_) {
    permission_status_ = status;
    if (permission_update_callback_) {
      permission_update_callback_.Run(status);
    }
    RecordUmaPermissionStatusChanged(status, has_pending_system_prompt_);
    has_pending_system_prompt_ = false;
  }
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
