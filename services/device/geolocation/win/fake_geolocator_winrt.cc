// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/geolocation/win/fake_geolocator_winrt.h"

#include "base/callback.h"
#include "base/task/post_task.h"
#include "services/device/geolocation/win/fake_geocoordinate_winrt.h"
#include "services/device/geolocation/win/fake_position_changed_event_args_winrt.h"
#include "services/device/geolocation/win/fake_status_changed_event_args_winrt.h"

namespace device {
namespace {
using ABI::Windows::Devices::Geolocation::Geolocator;
using ABI::Windows::Devices::Geolocation::Geoposition;
using ABI::Windows::Devices::Geolocation::PositionAccuracy;
using ABI::Windows::Devices::Geolocation::PositionChangedEventArgs;
using ABI::Windows::Devices::Geolocation::PositionStatus;
using ABI::Windows::Devices::Geolocation::StatusChangedEventArgs;
using ABI::Windows::Foundation::IAsyncOperation;
using ABI::Windows::Foundation::ITypedEventHandler;
using ABI::Windows::Foundation::TimeSpan;
using Microsoft::WRL::ComPtr;
using Microsoft::WRL::Make;
}  // namespace

FakeGeolocatorWinrt::FakeGeolocatorWinrt(
    std::unique_ptr<FakeGeocoordinateData> position_data,
    PositionStatus position_status)
    : position_data_(std::move(position_data)),
      position_status_(position_status) {}

FakeGeolocatorWinrt::~FakeGeolocatorWinrt() = default;

IFACEMETHODIMP FakeGeolocatorWinrt::get_DesiredAccuracy(
    PositionAccuracy* value) {
  *value = accuracy_;
  return S_OK;
}

IFACEMETHODIMP FakeGeolocatorWinrt::put_DesiredAccuracy(
    PositionAccuracy value) {
  accuracy_ = value;
  return S_OK;
}

IFACEMETHODIMP FakeGeolocatorWinrt::get_MovementThreshold(DOUBLE* value) {
  *value = movement_threshold_;
  return S_OK;
}

IFACEMETHODIMP FakeGeolocatorWinrt::put_MovementThreshold(DOUBLE value) {
  movement_threshold_ = value;
  return S_OK;
}

IFACEMETHODIMP FakeGeolocatorWinrt::get_ReportInterval(UINT32* value) {
  return E_NOTIMPL;
}

IFACEMETHODIMP FakeGeolocatorWinrt::put_ReportInterval(UINT32 value) {
  return E_NOTIMPL;
}

IFACEMETHODIMP FakeGeolocatorWinrt::get_LocationStatus(PositionStatus* value) {
  return E_NOTIMPL;
}

IFACEMETHODIMP FakeGeolocatorWinrt::GetGeopositionAsync(
    IAsyncOperation<Geoposition*>** value) {
  return E_NOTIMPL;
}

IFACEMETHODIMP FakeGeolocatorWinrt::GetGeopositionAsyncWithAgeAndTimeout(
    TimeSpan maximumAge,
    TimeSpan timeout,
    IAsyncOperation<Geoposition*>** value) {
  return E_NOTIMPL;
}

void FakeGeolocatorWinrt::RunPositionChangedHandler(
    ITypedEventHandler<Geolocator*, PositionChangedEventArgs*>* handler) {
  if (!position_changed_token_.has_value() ||
      position_status_ != PositionStatus::PositionStatus_Ready) {
    return;
  }

  auto event_args =
      Make<FakePositionChangedEventArgs>(std::move(position_data_));
  handler->Invoke(this, event_args.Detach());
}

IFACEMETHODIMP FakeGeolocatorWinrt::add_PositionChanged(
    ITypedEventHandler<Geolocator*, PositionChangedEventArgs*>* handler,
    EventRegistrationToken* token) {
  position_changed_token_ = EventRegistrationToken();
  *token = position_changed_token_.value();
  base::PostTask(
      FROM_HERE, {base::CurrentThread()},
      base::BindOnce(
          &FakeGeolocatorWinrt::RunPositionChangedHandler,
          weak_ptr_factory_.GetWeakPtr(),
          ComPtr<ITypedEventHandler<Geolocator*, PositionChangedEventArgs*>>(
              handler)));
  return S_OK;
}

IFACEMETHODIMP FakeGeolocatorWinrt::remove_PositionChanged(
    EventRegistrationToken token) {
  DCHECK(position_changed_token_.has_value());
  DCHECK_EQ(token.value, position_changed_token_->value);
  position_changed_token_.reset();
  return S_OK;
}

void FakeGeolocatorWinrt::RunStatusChangedHandler(
    ITypedEventHandler<Geolocator*, StatusChangedEventArgs*>* handler) {
  if (!status_changed_token_.has_value())
    return;

  auto event_args = Make<FakeStatusChangedEventArgs>(position_status_);
  handler->Invoke(this, event_args.Detach());
}

IFACEMETHODIMP FakeGeolocatorWinrt::add_StatusChanged(
    ITypedEventHandler<Geolocator*, StatusChangedEventArgs*>* handler,
    EventRegistrationToken* token) {
  status_changed_token_ = EventRegistrationToken();
  *token = status_changed_token_.value();
  base::PostTask(
      FROM_HERE, {base::CurrentThread()},
      base::BindOnce(
          &FakeGeolocatorWinrt::RunStatusChangedHandler,
          weak_ptr_factory_.GetWeakPtr(),
          ComPtr<ITypedEventHandler<Geolocator*, StatusChangedEventArgs*>>(
              handler)));
  return S_OK;
}

IFACEMETHODIMP FakeGeolocatorWinrt::remove_StatusChanged(
    EventRegistrationToken token) {
  DCHECK(status_changed_token_.has_value());
  DCHECK_EQ(token.value, status_changed_token_->value);
  status_changed_token_.reset();
  return S_OK;
}

}  // namespace device