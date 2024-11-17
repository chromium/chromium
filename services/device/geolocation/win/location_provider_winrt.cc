// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/geolocation/win/location_provider_winrt.h"

#include <windows.devices.enumeration.h>
#include <windows.foundation.h>
#include <wrl/event.h>

#include <optional>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/win/core_winrt_util.h"
#include "components/device_event_log/device_event_log.h"
#include "services/device/public/cpp/device_features.h"
#include "services/device/public/cpp/geolocation/geoposition.h"
#include "services/device/public/mojom/geolocation_internals.mojom-shared.h"

namespace device {

namespace {
using ABI::Windows::Devices::Enumeration::DeviceAccessStatus;
using ABI::Windows::Devices::Enumeration::DeviceClass;
using ABI::Windows::Devices::Enumeration::IDeviceAccessInformation;
using ABI::Windows::Devices::Enumeration::IDeviceAccessInformationStatics;
using ABI::Windows::Devices::Geolocation::AltitudeReferenceSystem;
using ABI::Windows::Devices::Geolocation::AltitudeReferenceSystem_Ellipsoid;
using ABI::Windows::Devices::Geolocation::AltitudeReferenceSystem_Unspecified;
using ABI::Windows::Devices::Geolocation::BasicGeoposition;
using ABI::Windows::Devices::Geolocation::Geolocator;
using ABI::Windows::Devices::Geolocation::IGeocoordinate;
using ABI::Windows::Devices::Geolocation::IGeocoordinateWithPoint;
using ABI::Windows::Devices::Geolocation::IGeocoordinateWithPositionData;
using ABI::Windows::Devices::Geolocation::IGeolocator;
using ABI::Windows::Devices::Geolocation::IGeopoint;
using ABI::Windows::Devices::Geolocation::IGeoposition;
using ABI::Windows::Devices::Geolocation::IGeoshape;
using ABI::Windows::Devices::Geolocation::IPositionChangedEventArgs;
using ABI::Windows::Devices::Geolocation::IStatusChangedEventArgs;
using ABI::Windows::Devices::Geolocation::PositionAccuracy;
using ABI::Windows::Devices::Geolocation::PositionChangedEventArgs;
using ABI::Windows::Devices::Geolocation::PositionSource;
using ABI::Windows::Devices::Geolocation::PositionSource_Unknown;
using ABI::Windows::Devices::Geolocation::PositionStatus;
using ABI::Windows::Devices::Geolocation::StatusChangedEventArgs;
using ABI::Windows::Foundation::IReference;
using ABI::Windows::Foundation::ITypedEventHandler;
using ABI::Windows::Foundation::TimeSpan;
using Microsoft::WRL::ComPtr;

// The amount of change (in meters) in the reported position from the Windows
// API which will trigger an update.
constexpr double kDefaultMovementThresholdMeters = 1.0;

template <typename F>
std::optional<DOUBLE> GetOptionalDouble(F&& getter) {
  DOUBLE value = 0;
  HRESULT hr = getter(&value);
  if (SUCCEEDED(hr))
    return value;
  return std::nullopt;
}

template <typename F>
std::optional<DOUBLE> GetReferenceOptionalDouble(F&& getter) {
  IReference<DOUBLE>* reference_value;
  HRESULT hr = getter(&reference_value);
  if (!SUCCEEDED(hr) || !reference_value)
    return std::nullopt;
  return GetOptionalDouble([&](DOUBLE* value) -> HRESULT {
    return reference_value->get_Value(value);
  });
}

bool IsSystemLocationSettingEnabled() {
  ComPtr<IDeviceAccessInformationStatics> dev_access_info_statics;
  HRESULT hr = base::win::GetActivationFactory<
      IDeviceAccessInformationStatics,
      RuntimeClass_Windows_Devices_Enumeration_DeviceAccessInformation>(
      &dev_access_info_statics);
  if (FAILED(hr)) {
    GEOLOCATION_LOG(ERROR) << "IDeviceAccessInformationStatics failed: "
                           << logging::SystemErrorCodeToString(hr);
    return true;
  }

  ComPtr<IDeviceAccessInformation> dev_access_info;
  hr = dev_access_info_statics->CreateFromDeviceClass(
      DeviceClass::DeviceClass_Location, &dev_access_info);
  if (FAILED(hr)) {
    GEOLOCATION_LOG(ERROR) << "IDeviceAccessInformation failed: "
                           << logging::SystemErrorCodeToString(hr);
    return true;
  }

  auto status = DeviceAccessStatus::DeviceAccessStatus_Unspecified;
  dev_access_info->get_CurrentStatus(&status);

  return !(status == DeviceAccessStatus::DeviceAccessStatus_DeniedBySystem ||
           status == DeviceAccessStatus::DeviceAccessStatus_DeniedByUser);
}

HRESULT GetPointFromCoordinate(const ComPtr<IGeocoordinate>& coordinate,
                               IGeopoint** point) {
  CHECK(point);

  ComPtr<IGeocoordinateWithPoint> coordinate_with_point = nullptr;
  HRESULT result = coordinate.As(&coordinate_with_point);
  if (FAILED(result)) {
    GEOLOCATION_LOG(ERROR) << "Failed to cast to GeocoordinateWithPoint. "
                           << logging::SystemErrorCodeToString(result);
    return result;
  }
  if (!coordinate_with_point) {
    GEOLOCATION_LOG(ERROR) << "coordinate_with_point is null.";
    return E_POINTER;
  }

  result = coordinate_with_point->get_Point(point);
  if (FAILED(result)) {
    GEOLOCATION_LOG(ERROR) << "Failed to get point from coordinate. "
                           << logging::SystemErrorCodeToString(result);
    return result;
  }
  if (!*point) {
    GEOLOCATION_LOG(ERROR) << "Failed to get a valid point";
    return E_POINTER;
  }

  return result;
}

AltitudeReferenceSystem GetAltitudeReferenceSystemFromPoint(
    const ComPtr<IGeopoint>& point) {
  ComPtr<IGeoshape> shape;
  HRESULT hr = point.As(&shape);
  if (FAILED(hr)) {
    GEOLOCATION_LOG(ERROR) << "Failed to cast to GeoShape. "
                           << logging::SystemErrorCodeToString(hr);
    return AltitudeReferenceSystem_Unspecified;
  }

  AltitudeReferenceSystem reference_system =
      AltitudeReferenceSystem_Unspecified;
  hr = shape->get_AltitudeReferenceSystem(&reference_system);
  if (FAILED(hr)) {
    GEOLOCATION_LOG(ERROR) << "Failed to get altitude reference system. "
                           << logging::SystemErrorCodeToString(hr);
    return AltitudeReferenceSystem_Unspecified;
  }
  return reference_system;
}

PositionSource GetPositionSourceFromCoordinate(
    const ComPtr<IGeocoordinate>& coordinate) {
  ComPtr<IGeocoordinateWithPositionData> position_data;
  HRESULT hr = coordinate.As(&position_data);
  if (FAILED(hr)) {
    GEOLOCATION_LOG(ERROR)
        << "Failed to cast to GeocoordinateWithPositionData. "
        << logging::SystemErrorCodeToString(hr);
    return PositionSource_Unknown;
  }

  PositionSource position_source;
  hr = position_data->get_PositionSource(&position_source);
  if (FAILED(hr)) {
    GEOLOCATION_LOG(ERROR) << "Failed to get position source. "
                           << logging::SystemErrorCodeToString(hr);
    return PositionSource_Unknown;
  }
  return position_source;
}

void RecordUmaStartProviderError(HRESULT result) {
  base::UmaHistogramSparse("Geolocation.LocationProviderWinrt.StartProvider",
                           result);
}

void RecordUmaRegisterCallbacksError(HRESULT result) {
  base::UmaHistogramSparse(
      "Geolocation.LocationProviderWinrt.RegisterCallbacks", result);
}

void RecordUmaOnPositionChangedError(HRESULT result) {
  base::UmaHistogramSparse(
      "Geolocation.LocationProviderWinrt.OnPositionChanged", result);
}

void RecordUmaOnStatusChangedError(HRESULT result) {
  base::UmaHistogramSparse("Geolocation.LocationProviderWinrt.OnStatusChanged",
                           result);
}

void RecordUmaErrorStatus(
    ABI::Windows::Devices::Geolocation::PositionStatus status) {
  base::UmaHistogramSparse("Geolocation.LocationProviderWinrt.ErrorStatus",
                           static_cast<int>(status));
}

void RecordUmaCreateGeopositionError(HRESULT result) {
  base::UmaHistogramSparse(
      "Geolocation.LocationProviderWinrt.CreateGeoposition", result);
}

void RecordUmaAccuracy(int accuracy) {
  base::UmaHistogramCounts10M("Geolocation.LocationProviderWinrt.Accuracy",
                              accuracy);
}

void RecordUmaPositionSource(PositionSource source) {
  base::UmaHistogramSparse("Geolocation.LocationProviderWinrt.PositionSource",
                           source);
}

void RecordUmaSessionResult(HRESULT result) {
  base::UmaHistogramSparse("Geolocation.LocationProviderWinrt.SessionResult",
                           result);
}

}  // namespace

// LocationProviderWinrt
LocationProviderWinrt::LocationProviderWinrt() = default;

LocationProviderWinrt::~LocationProviderWinrt() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  StopProvider();
}

void LocationProviderWinrt::FillDiagnostics(
    mojom::GeolocationDiagnostics& diagnostics) {
  if (!is_started_) {
    diagnostics.provider_state =
        mojom::GeolocationDiagnostics::ProviderState::kStopped;
  } else if (!permission_granted_) {
    diagnostics.provider_state = mojom::GeolocationDiagnostics::ProviderState::
        kBlockedBySystemPermission;
  } else if (enable_high_accuracy_) {
    diagnostics.provider_state =
        mojom::GeolocationDiagnostics::ProviderState::kHighAccuracy;
  } else {
    diagnostics.provider_state =
        mojom::GeolocationDiagnostics::ProviderState::kLowAccuracy;
  }
}

void LocationProviderWinrt::SetUpdateCallback(
    const LocationProviderUpdateCallback& callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  location_update_callback_ = callback;
}

void LocationProviderWinrt::StartProvider(bool high_accuracy) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  is_started_ = true;
  enable_high_accuracy_ = high_accuracy;

  HRESULT hr = S_OK;

  if (!geo_locator_) {
    hr = GetGeolocator(&geo_locator_);
    if (FAILED(hr)) {
      RecordUmaStartProviderError(hr);
      SetSessionErrorIfNotSet(hr);
      HandleErrorCondition(mojom::GeopositionErrorCode::kPositionUnavailable,
                           "Unable to create instance of Geolocation API. " +
                               logging::SystemErrorCodeToString(hr));
      return;
    }

    hr = geo_locator_->put_MovementThreshold(kDefaultMovementThresholdMeters);

    if (FAILED(hr)) {
      RecordUmaStartProviderError(hr);
      GEOLOCATION_LOG(ERROR)
          << "Failed to set movement threshold on Geolocator. "
          << logging::SystemErrorCodeToString(hr);
    }
  }

  hr = geo_locator_->put_DesiredAccuracy(
      enable_high_accuracy_ ? PositionAccuracy::PositionAccuracy_High
                            : PositionAccuracy::PositionAccuracy_Default);

  if (FAILED(hr)) {
    RecordUmaStartProviderError(hr);
    GEOLOCATION_LOG(ERROR) << "Failed to set DesiredAccuracy on Geolocator: "
                           << logging::SystemErrorCodeToString(hr);
  }

  RegisterCallbacks();
}

void LocationProviderWinrt::StopProvider() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  is_started_ = false;

  // Only report to the metric if we have either encountered an error or
  // retrieved a valid Geoposition.
  if (session_error_ || position_received_) {
    RecordUmaSessionResult(session_error_.value_or(S_OK));
    session_error_.reset();
  }

  // Reset the reference location state (provider+position)
  // so that future starts use fresh locations from
  // the newly constructed providers.
  last_result_.reset();
  if (!geo_locator_) {
    return;
  }

  UnregisterCallbacks();

  geo_locator_.Reset();
}

const mojom::GeopositionResult* LocationProviderWinrt::GetPosition() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return last_result_.get();
}

void LocationProviderWinrt::OnPermissionGranted() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  const bool was_permission_granted = permission_granted_;
  permission_granted_ = true;
  if (!was_permission_granted) {
    RegisterCallbacks();
  }
}

void LocationProviderWinrt::HandleErrorCondition(
    mojom::GeopositionErrorCode position_error_code,
    const std::string& position_error_message) {
  GEOLOCATION_LOG(ERROR) << position_error_message;
  last_result_ =
      mojom::GeopositionResult::NewError(mojom::GeopositionError::New(
          position_error_code, position_error_message, /*error_technical=*/""));
  if (location_update_callback_) {
    location_update_callback_.Run(this, last_result_.Clone());
  }
}

bool LocationProviderWinrt::HasValidLastPosition() const {
  return last_result_ && last_result_->is_position() &&
         ValidateGeoposition(*last_result_->get_position());
}

void LocationProviderWinrt::RegisterCallbacks() {
  if (!permission_granted_ || !geo_locator_) {
    return;
  }

  HRESULT hr = S_OK;

  if (!position_changed_token_) {
    position_received_ = false;
    EventRegistrationToken tmp_position_token;
    hr = geo_locator_->add_PositionChanged(
        Microsoft::WRL::Callback<
            ITypedEventHandler<Geolocator*, PositionChangedEventArgs*>>(
            [task_runner(base::SingleThreadTaskRunner::GetCurrentDefault()),
             callback(
                 base::BindRepeating(&LocationProviderWinrt::OnPositionChanged,
                                     weak_ptr_factory_.GetWeakPtr()))](
                IGeolocator* sender, IPositionChangedEventArgs* args) {
              task_runner->PostTask(
                  FROM_HERE,
                  base::BindOnce(callback, ComPtr<IGeolocator>(sender),
                                 ComPtr<IPositionChangedEventArgs>(args)));
              return S_OK;
            })
            .Get(),
        &tmp_position_token);

    if (FAILED(hr)) {
      RecordUmaRegisterCallbacksError(hr);
      SetSessionErrorIfNotSet(hr);
      if (!HasValidLastPosition()) {
        HandleErrorCondition(
            mojom::GeopositionErrorCode::kPositionUnavailable,
            "Unable to add a callback to retrieve position for Geolocation "
            "API. " +
                logging::SystemErrorCodeToString(hr));
      }
      return;
    }

    position_callback_initialized_time_ = base::TimeTicks::Now();
    position_changed_token_ = tmp_position_token;
  }

  if (!status_changed_token_) {
    EventRegistrationToken tmp_status_token;
    hr = geo_locator_->add_StatusChanged(
        Microsoft::WRL::Callback<
            ITypedEventHandler<Geolocator*, StatusChangedEventArgs*>>(
            [task_runner(base::SingleThreadTaskRunner::GetCurrentDefault()),
             callback(
                 base::BindRepeating(&LocationProviderWinrt::OnStatusChanged,
                                     weak_ptr_factory_.GetWeakPtr()))](
                IGeolocator* sender, IStatusChangedEventArgs* args) {
              task_runner->PostTask(
                  FROM_HERE,
                  base::BindOnce(callback, ComPtr<IGeolocator>(sender),
                                 ComPtr<IStatusChangedEventArgs>(args)));
              return S_OK;
            })
            .Get(),
        &tmp_status_token);

    if (FAILED(hr)) {
      RecordUmaRegisterCallbacksError(hr);
      // If this occurs we may still be able to provide a position update, but
      // if the geoloc API is Disabled(denied permission) we won't inform the
      // user, it will just fail silently or timeout.
      GEOLOCATION_LOG(ERROR)
          << "Failed to set a Status Changed callback for geolocation API. "
          << logging::SystemErrorCodeToString(hr);
      return;
    }

    status_changed_token_ = tmp_status_token;
  }
}

void LocationProviderWinrt::UnregisterCallbacks() {
  if (!geo_locator_) {
    return;
  }

  if (position_changed_token_) {
    geo_locator_->remove_PositionChanged(*position_changed_token_);
    position_changed_token_.reset();
  }

  if (status_changed_token_) {
    geo_locator_->remove_StatusChanged(*status_changed_token_);
    status_changed_token_.reset();
  }

  weak_ptr_factory_.InvalidateWeakPtrs();
}

void LocationProviderWinrt::OnPositionChanged(
    IGeolocator* geo_locator,
    IPositionChangedEventArgs* position_update) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  ComPtr<IGeoposition> position;
  HRESULT hr = position_update->get_Position(&position);
  if (FAILED(hr)) {
    RecordUmaOnPositionChangedError(hr);
    SetSessionErrorIfNotSet(hr);
    if (!HasValidLastPosition()) {
      HandleErrorCondition(mojom::GeopositionErrorCode::kPositionUnavailable,
                           "Unable to get position from Geolocation API. " +
                               logging::SystemErrorCodeToString(hr));
    }
    return;
  }

  mojom::GeopositionPtr location_data = CreateGeoposition(position.Get());
  if (!location_data || !ValidateGeoposition(*location_data)) {
    return;
  }

  last_result_ =
      mojom::GeopositionResult::NewPosition(std::move(location_data));

  if (!position_received_) {
    const base::TimeDelta time_to_first_position =
        base::TimeTicks::Now() - position_callback_initialized_time_;

    UmaHistogramCustomTimes(
        "Geolocation.LocationProviderWinrt.TimeToFirstPosition",
        time_to_first_position, base::Milliseconds(1), base::Seconds(10), 100);
    position_received_ = true;
  }

  if (location_update_callback_) {
    location_update_callback_.Run(this, last_result_.Clone());
  }
}

void LocationProviderWinrt::OnStatusChanged(
    IGeolocator* geo_locator,
    IStatusChangedEventArgs* status_update) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  PositionStatus status;
  HRESULT hr = status_update->get_Status(&status);
  if (FAILED(hr)) {
    RecordUmaOnStatusChangedError(hr);
    GEOLOCATION_LOG(ERROR)
        << "Failed to get a status from StatusChangedEventArgs. "
        << logging::SystemErrorCodeToString(hr);
    return;
  }

  position_status_ = status;

  switch (status) {
    case PositionStatus::PositionStatus_Disabled:
      RecordUmaErrorStatus(status);
      SetSessionErrorIfNotSet(ERROR_ACCESS_DENIED);
      HandleErrorCondition(mojom::GeopositionErrorCode::kPermissionDenied,
                           "User has not allowed access to Windows Location.");
      break;
    case PositionStatus::PositionStatus_NotAvailable:
      RecordUmaErrorStatus(status);
      SetSessionErrorIfNotSet(ERROR_NOT_SUPPORTED);
      HandleErrorCondition(
          mojom::GeopositionErrorCode::kPositionUnavailable,
          "Location API is not available on this version of Windows.");
      break;
    default:
      break;
  }
}

mojom::GeopositionPtr LocationProviderWinrt::CreateGeoposition(
    IGeoposition* geoposition) {
  ComPtr<IGeocoordinate> coordinate;
  HRESULT hr = geoposition->get_Coordinate(&coordinate);
  if (FAILED(hr)) {
    RecordUmaCreateGeopositionError(hr);
    SetSessionErrorIfNotSet(hr);
    GEOLOCATION_LOG(ERROR)
        << "Failed to get a coordinate from geoposition from windows "
           "geolocation API. "
        << logging::SystemErrorCodeToString(hr);
    return nullptr;
  }

  ComPtr<IGeopoint> point;
  hr = GetPointFromCoordinate(coordinate, &point);
  if (FAILED(hr)) {
    RecordUmaCreateGeopositionError(hr);
    SetSessionErrorIfNotSet(hr);
    return nullptr;
  }

  BasicGeoposition position;
  hr = point->get_Position(&position);
  if (FAILED(hr)) {
    GEOLOCATION_LOG(ERROR) << "Failed to get position from point. "
                           << logging::SystemErrorCodeToString(hr);
    RecordUmaCreateGeopositionError(hr);
    SetSessionErrorIfNotSet(hr);
    return nullptr;
  }

  auto location_data = mojom::Geoposition::New();
  location_data->latitude = position.Latitude;
  location_data->longitude = position.Longitude;
  location_data->altitude = position.Altitude;
  location_data->accuracy = GetOptionalDouble([&](DOUBLE* value) -> HRESULT {
                              return coordinate->get_Accuracy(value);
                            }).value_or(device::mojom::kBadAccuracy);
  location_data->altitude_accuracy =
      GetReferenceOptionalDouble([&](IReference<DOUBLE>** value) -> HRESULT {
        return coordinate->get_AltitudeAccuracy(value);
      }).value_or(device::mojom::kBadAccuracy);
  location_data->heading =
      GetReferenceOptionalDouble([&](IReference<DOUBLE>** value) -> HRESULT {
        return coordinate->get_Heading(value);
      }).value_or(device::mojom::kBadHeading);
  location_data->speed =
      GetReferenceOptionalDouble([&](IReference<DOUBLE>** value) -> HRESULT {
        return coordinate->get_Speed(value);
      }).value_or(device::mojom::kBadSpeed);
  location_data->timestamp = base::Time::Now();

  // Overwrite the altitude if the accuracy is known to be bad or it is not
  // using ellipsoid altitude reference system.
  if (location_data->altitude_accuracy == device::mojom::kBadAccuracy ||
      GetAltitudeReferenceSystemFromPoint(point) !=
          AltitudeReferenceSystem_Ellipsoid) {
    location_data->altitude = device::mojom::kBadAltitude;
  }

  RecordUmaAccuracy(static_cast<int>(location_data->accuracy));
  RecordUmaPositionSource(GetPositionSourceFromCoordinate(coordinate));

  return location_data;
}

HRESULT LocationProviderWinrt::GetGeolocator(IGeolocator** geo_locator) {
  ComPtr<IGeolocator> temp_geo_locator;
  auto hstring = base::win::ScopedHString::Create(
      RuntimeClass_Windows_Devices_Geolocation_Geolocator);
  HRESULT hr = base::win::RoActivateInstance(hstring.get(), &temp_geo_locator);

  if (SUCCEEDED(hr)) {
    *geo_locator = temp_geo_locator.Detach();
  }

  return hr;
}

void LocationProviderWinrt::SetSessionErrorIfNotSet(HRESULT error) {
  if (session_error_) {
    return;
  }
  session_error_ = error;
}

std::unique_ptr<LocationProvider> NewSystemLocationProvider() {
  // TODO: Remove IsSystemLocationSettingEnabled once
  // system permission manager support for `LocationProviderWinrt` is ready.
  if (!IsSystemLocationSettingEnabled()) {
    return nullptr;
  }

  return std::make_unique<LocationProviderWinrt>();
}

}  // namespace device
