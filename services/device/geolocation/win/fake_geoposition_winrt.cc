// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/geolocation/win/fake_geoposition_winrt.h"

#include "services/device/geolocation/win/fake_geocoordinate_winrt.h"

namespace device {
namespace {
using ABI::Windows::Devices::Geolocation::ICivicAddress;
using ABI::Windows::Devices::Geolocation::IGeocoordinate;
using Microsoft::WRL::Make;
}  // namespace

FakeGeoposition::FakeGeoposition(
    std::unique_ptr<FakeGeocoordinateData> position_data)
    : position_data_(std::move(position_data)) {}

FakeGeoposition::~FakeGeoposition() = default;

IFACEMETHODIMP
FakeGeoposition::get_Coordinate(IGeocoordinate** value) {
  *value = Make<FakeGeocoordinate>(std::move(position_data_)).Detach();
  return S_OK;
}

IFACEMETHODIMP FakeGeoposition::get_CivicAddress(ICivicAddress** value) {
  return E_NOTIMPL;
}

}  // namespace device