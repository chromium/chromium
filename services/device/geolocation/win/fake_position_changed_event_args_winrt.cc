// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/geolocation/win/fake_position_changed_event_args_winrt.h"

#include "services/device/geolocation/win/fake_geocoordinate_winrt.h"
#include "services/device/geolocation/win/fake_geoposition_winrt.h"

namespace device {
namespace {
using ABI::Windows::Devices::Geolocation::IGeoposition;
using Microsoft::WRL::Make;
}  // namespace

FakePositionChangedEventArgs::FakePositionChangedEventArgs(
    std::unique_ptr<FakeGeocoordinateData> position_data)
    : position_data_(std::move(position_data)) {}

FakePositionChangedEventArgs::~FakePositionChangedEventArgs() = default;

IFACEMETHODIMP FakePositionChangedEventArgs::get_Position(
    IGeoposition** value) {
  *value = Make<FakeGeoposition>(std::move(position_data_)).Detach();
  return S_OK;
}

}  // namespace device