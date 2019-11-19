// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/geolocation/win/fake_geocoordinate_winrt.h"

#include "base/win/reference.h"

namespace device {
namespace {
using ABI::Windows::Foundation::DateTime;
using ABI::Windows::Foundation::IReference;
using base::win::Reference;
using Microsoft::WRL::Make;
}  // namespace

FakeGeocoordinateData::FakeGeocoordinateData() = default;

FakeGeocoordinateData::FakeGeocoordinateData(const FakeGeocoordinateData& obj) =
    default;

FakeGeocoordinate::FakeGeocoordinate(
    std::unique_ptr<FakeGeocoordinateData> position_data)
    : position_data_(std::move(position_data)) {}

FakeGeocoordinate::~FakeGeocoordinate() = default;

IFACEMETHODIMP FakeGeocoordinate::get_Latitude(DOUBLE* value) {
  *value = position_data_->latitude;
  return S_OK;
}

IFACEMETHODIMP FakeGeocoordinate::get_Longitude(DOUBLE* value) {
  *value = position_data_->longitude;
  return S_OK;
}

IFACEMETHODIMP FakeGeocoordinate::get_Altitude(IReference<double>** value) {
  if (!position_data_->altitude.has_value())
    return E_NOTIMPL;

  Make<Reference<double>>(*position_data_->altitude).CopyTo(value);
  return S_OK;
}

IFACEMETHODIMP FakeGeocoordinate::get_Accuracy(DOUBLE* value) {
  *value = position_data_->accuracy;
  return S_OK;
}

IFACEMETHODIMP FakeGeocoordinate::get_AltitudeAccuracy(
    IReference<double>** value) {
  if (!position_data_->altitude_accuracy.has_value())
    return E_NOTIMPL;

  Make<Reference<double>>(*position_data_->altitude_accuracy).CopyTo(value);
  return S_OK;
}

IFACEMETHODIMP FakeGeocoordinate::get_Heading(IReference<double>** value) {
  if (!position_data_->heading.has_value())
    return E_NOTIMPL;

  Make<Reference<double>>(*position_data_->heading).CopyTo(value);
  return S_OK;
}

IFACEMETHODIMP FakeGeocoordinate::get_Speed(IReference<double>** value) {
  if (!position_data_->speed.has_value())
    return E_NOTIMPL;

  Make<Reference<double>>(*position_data_->speed).CopyTo(value);
  return S_OK;
}

IFACEMETHODIMP FakeGeocoordinate::get_Timestamp(DateTime* value) {
  return E_NOTIMPL;
}

}  // namespace device