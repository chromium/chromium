// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/geolocation/win/location_provider_winrt.h"

#include <windows.foundation.h>
#include <wrl/event.h>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/optional.h"
#include "base/strings/string_number_conversions.h"
#include "base/win/core_winrt_util.h"
#include "services/device/public/cpp/device_features.h"
#include "services/device/public/cpp/geolocation/geoposition.h"

namespace device {
namespace {
using ABI::Windows::Devices::Geolocation::Geolocator;
using ABI::Windows::Devices::Geolocation::IGeocoordinate;
using ABI::Windows::Devices::Geolocation::IGeolocator;
using ABI::Windows::Devices::Geolocation::IGeoposition;
using ABI::Windows::Devices::Geolocation::IPositionChangedEventArgs;
using ABI::Windows::Devices::Geolocation::IStatusChangedEventArgs;
using ABI::Windows::Devices::Geolocation::PositionAccuracy;
using ABI::Windows::Devices::Geolocation::PositionChangedEventArgs;
using ABI::Windows::Devices::Geolocation::PositionStatus;
using ABI::Windows::Devices::Geolocation::StatusChangedEventArgs;
using ABI::Windows::Foundation::IAsyncOperation;
using ABI::Windows::Foundation::IReference;
using ABI::Windows::Foundation::ITypedEventHandler;
using ABI::Windows::Foundation::TimeSpan;
using Microsoft::WRL::ComPtr;

// The amount of change (in meters) in the reported position from the Windows
// API which will trigger an update.
constexpr double kDefaultMovementThresholdMeters = 1.0;

// These values are logged to UMA. Entries should not be renumbered and
// numeric values should never be reused. Please keep in sync with
// "WindowsRTLocationRequestEvent" in
// src/tools/metrics/histograms/enums.xml.
enum WindowsRTLocationRequestEvent {
  WINDOWS_RT_LOCATION_CALLBACK_EVENT_START = 0,
  WINDOWS_RT_LOCATION_CALLBACK_EVENT_CANCEL = 1,
  WINDOWS_RT_LOCATION_CALLBACK_EVENT_SUCCESS = 2,
  WINDOWS_RT_LOCATION_CALLBACK_EVENT_FAILURE = 3,
  WINDOWS_RT_LOCATION_CALLBACK_EVENT_INVALID_POSITION = 4,
  WINDOWS_RT_LOCATION_CALLBACK_EVENT_PERMISSION_DENIED = 5,
  WINDOWS_RT_LOCATION_CALLBACK_EVENT_POSITION_UNAVAILABLE = 6,
  WINDOWS_RT_LOCATION_CALLBACK_EVENT_TIMEOUT = 7,
  WINDOWS_RT_LOCATION_CALLBACK_EVENT_UNKNOWN_ERROR_CONDITION = 8,
  kMaxValue = WINDOWS_RT_LOCATION_CALLBACK_EVENT_UNKNOWN_ERROR_CONDITION
};

void RecordUmaEvent(WindowsRTLocationRequestEvent event) {
  base::UmaHistogramEnumeration("Windows.RT.LocationRequest.Event", event);
}

bool IsWinRTSupported() {
  return base::win::ResolveCoreWinRTDelayload() &&
         base::win::ScopedHString::ResolveCoreWinRTStringDelayload();
}

template <typename F>
base::Optional<DOUBLE> GetOptionalDouble(F&& getter) {
  DOUBLE value = 0;
  HRESULT hr = getter(&value);
  if (SUCCEEDED(hr))
    return value;
  return base::nullopt;
}

template <typename F>
base::Optional<DOUBLE> GetReferenceOptionalDouble(F&& getter) {
  IReference<DOUBLE>* reference_value;
  HRESULT hr = getter(&reference_value);
  if (!SUCCEEDED(hr) || !reference_value)
    return base::nullopt;
  return GetOptionalDouble([&](DOUBLE* value) -> HRESULT {
    return reference_value->get_Value(value);
  });
}
}  // namespace

// LocationProviderWinrt
LocationProviderWinrt::LocationProviderWinrt() = default;

LocationProviderWinrt::~LocationProviderWinrt() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  StopProvider();
}

void LocationProviderWinrt::SetUpdateCallback(
    const LocationProviderUpdateCallback& callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  location_update_callback_ = callback;
}

void LocationProviderWinrt::StartProvider(bool high_accuracy) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  enable_high_accuracy_ = high_accuracy;

  HRESULT hr = S_OK;

  if (!geo_locator_) {
    hr = GetGeolocator(&geo_locator_);
    if (FAILED(hr)) {
      HandleErrorCondition(
          mojom::Geoposition::ErrorCode::POSITION_UNAVAILABLE,
          "Unable to create instance of Geolocation API. HRESULT: " +
              base::NumberToString(hr));
      return;
    }

    hr = geo_locator_->put_MovementThreshold(kDefaultMovementThresholdMeters);

    if (FAILED(hr)) {
      VLOG(1) << "Failed to set movement threshold on geo_locator. HRESULT: "
              << hr;
    }
  }

  hr = geo_locator_->put_DesiredAccuracy(
      enable_high_accuracy_ ? PositionAccuracy::PositionAccuracy_High
                            : PositionAccuracy::PositionAccuracy_Default);

  if (FAILED(hr)) {
    VLOG(1) << "Failed to set DesiredAccuracy on geo_locator: " << hr;
  }

  RegisterCallbacks();
}

void LocationProviderWinrt::StopProvider() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Reset the reference location state (provider+position)
  // so that future starts use fresh locations from
  // the newly constructed providers.
  last_position_ = mojom::Geoposition();
  if (!geo_locator_) {
    return;
  }

  UnregisterCallbacks();

  geo_locator_.Reset();
}

const mojom::Geoposition& LocationProviderWinrt::GetPosition() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return last_position_;
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
    mojom::Geoposition::ErrorCode position_error_code,
    const std::string& position_error_message) {
  WindowsRTLocationRequestEvent event;
  switch (position_error_code) {
    case mojom::Geoposition::ErrorCode::PERMISSION_DENIED:
      event = WindowsRTLocationRequestEvent::
          WINDOWS_RT_LOCATION_CALLBACK_EVENT_PERMISSION_DENIED;
      break;
    case mojom::Geoposition::ErrorCode::POSITION_UNAVAILABLE:
      event = WindowsRTLocationRequestEvent::
          WINDOWS_RT_LOCATION_CALLBACK_EVENT_POSITION_UNAVAILABLE;
      break;
    case mojom::Geoposition::ErrorCode::TIMEOUT:
      event = WindowsRTLocationRequestEvent::
          WINDOWS_RT_LOCATION_CALLBACK_EVENT_TIMEOUT;
      break;
    default:
      event = WindowsRTLocationRequestEvent::
          WINDOWS_RT_LOCATION_CALLBACK_EVENT_UNKNOWN_ERROR_CONDITION;
  }
  RecordUmaEvent(event);

  last_position_ = mojom::Geoposition();
  last_position_.error_code = position_error_code;
  last_position_.error_message = position_error_message;

  if (location_update_callback_) {
    location_update_callback_.Run(this, last_position_);
  }
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
            [task_runner(base::ThreadTaskRunnerHandle::Get()),
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
      RecordUmaEvent(WindowsRTLocationRequestEvent::
                         WINDOWS_RT_LOCATION_CALLBACK_EVENT_FAILURE);
      if (!last_position_.valid) {
        HandleErrorCondition(
            mojom::Geoposition::ErrorCode::POSITION_UNAVAILABLE,
            "Unable to add a callback to retrieve position for Geolocation "
            "API. HRESULT: " +
                base::NumberToString(hr));
      }
      return;
    }

    position_callback_initialized_time_ = base::TimeTicks::Now();
    RecordUmaEvent(WindowsRTLocationRequestEvent::
                       WINDOWS_RT_LOCATION_CALLBACK_EVENT_START);
    position_changed_token_ = tmp_position_token;
  }

  if (!status_changed_token_) {
    EventRegistrationToken tmp_status_token;
    hr = geo_locator_->add_StatusChanged(
        Microsoft::WRL::Callback<
            ITypedEventHandler<Geolocator*, StatusChangedEventArgs*>>(
            [task_runner(base::ThreadTaskRunnerHandle::Get()),
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
      // If this occurs we may still be able to provide a position update, but
      // if the geoloc API is Disabled(denied permission) we won't inform the
      // user, it will just fail silently or timeout.
      VLOG(1) << "Failed to set a Status Changed callback for geolocation API. "
                 "HRESULT: "
              << hr;
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
    RecordUmaEvent(WindowsRTLocationRequestEvent::
                       WINDOWS_RT_LOCATION_CALLBACK_EVENT_CANCEL);
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
    RecordUmaEvent(WindowsRTLocationRequestEvent::
                       WINDOWS_RT_LOCATION_CALLBACK_EVENT_FAILURE);
    if (!last_position_.valid) {
      HandleErrorCondition(
          mojom::Geoposition::ErrorCode::POSITION_UNAVAILABLE,
          "Unable to get position from Geolocation API. HRESULT: " +
              base::NumberToString(hr));
    }
    return;
  }

  mojom::Geoposition location_data;
  PopulateLocationData(position.Get(), &location_data);
  if (!ValidateGeoposition(location_data)) {
    RecordUmaEvent(WindowsRTLocationRequestEvent::
                       WINDOWS_RT_LOCATION_CALLBACK_EVENT_INVALID_POSITION);
    return;
  }

  location_data.valid = true;
  last_position_ = location_data;

  if (!position_received_) {
    const base::TimeDelta time_to_first_position =
        base::TimeTicks::Now() - position_callback_initialized_time_;

    UmaHistogramCustomTimes("Windows.RT.LocationRequest.TimeToFirstPosition",
                            time_to_first_position,
                            base::TimeDelta::FromMilliseconds(1),
                            base::TimeDelta::FromSeconds(10), 100);
    position_received_ = true;
  }
  RecordUmaEvent(WindowsRTLocationRequestEvent::
                     WINDOWS_RT_LOCATION_CALLBACK_EVENT_SUCCESS);

  if (location_update_callback_) {
    location_update_callback_.Run(this, last_position_);
  }
}

void LocationProviderWinrt::OnStatusChanged(
    IGeolocator* geo_locator,
    IStatusChangedEventArgs* status_update) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  PositionStatus status;
  HRESULT hr = status_update->get_Status(&status);
  if (FAILED(hr)) {
    VLOG(1) << "Failed to get a status from StatusChangedEventArgs. HRESULT: "
            << hr;
    return;
  }

  position_status_ = status;

  switch (status) {
    case PositionStatus::PositionStatus_Disabled:
      HandleErrorCondition(mojom::Geoposition::ErrorCode::PERMISSION_DENIED,
                           "User has not allowed access to Windows Location.");
      break;
    case PositionStatus::PositionStatus_NotAvailable:
      HandleErrorCondition(
          mojom::Geoposition::ErrorCode::POSITION_UNAVAILABLE,
          "Location API is not available on this version of Windows.");
      break;
    default:
      break;
  }
}

void LocationProviderWinrt::PopulateLocationData(
    IGeoposition* geoposition,
    mojom::Geoposition* location_data) {
  ComPtr<IGeocoordinate> coordinate;
  HRESULT hr = geoposition->get_Coordinate(&coordinate);
  if (FAILED(hr)) {
    VLOG(1) << "Failed to get a coordinate from getposition from windows "
               "geolocation API. HRESULT: "
            << hr;
    return;
  }

  location_data->latitude = GetOptionalDouble([&](DOUBLE* value) -> HRESULT {
                              return coordinate->get_Latitude(value);
                            }).value_or(device::mojom::kBadLatitudeLongitude);
  location_data->longitude = GetOptionalDouble([&](DOUBLE* value) -> HRESULT {
                               return coordinate->get_Longitude(value);
                             }).value_or(device::mojom::kBadLatitudeLongitude);
  location_data->accuracy = GetOptionalDouble([&](DOUBLE* value) -> HRESULT {
                              return coordinate->get_Accuracy(value);
                            }).value_or(device::mojom::kBadAccuracy);
  location_data->altitude =
      GetReferenceOptionalDouble([&](IReference<DOUBLE>** value) -> HRESULT {
        return coordinate->get_Altitude(value);
      }).value_or(device::mojom::kBadAltitude);
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

// static
std::unique_ptr<LocationProvider> NewSystemLocationProvider() {
  if (!base::FeatureList::IsEnabled(
          features::kWinrtGeolocationImplementation) ||
      !IsWinRTSupported()) {
    return nullptr;
  }

  return std::make_unique<LocationProviderWinrt>();
}

}  // namespace device
