// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/geolocation/mock_wifi_data_provider.h"

namespace device {

MockWifiDataProvider* MockWifiDataProvider::instance_ = nullptr;

// static
WifiDataProvider* MockWifiDataProvider::GetInstance() {
  CHECK(instance_);
  return instance_;
}

// static
MockWifiDataProvider* MockWifiDataProvider::CreateInstance() {
  CHECK(!instance_);
  instance_ = new MockWifiDataProvider;
  return instance_;
}

MockWifiDataProvider::MockWifiDataProvider() = default;

MockWifiDataProvider::~MockWifiDataProvider() {
  CHECK(this == instance_);
  instance_ = nullptr;
}

void MockWifiDataProvider::StartDataProvider() {}

void MockWifiDataProvider::StopDataProvider() {}

bool MockWifiDataProvider::DelayedByPolicy() {
  return false;
}

bool MockWifiDataProvider::GetData(WifiData* data_out) {
  CHECK(data_out);
  *data_out = data_;
  return got_data_;
}

void MockWifiDataProvider::ForceRescan() {}

void MockWifiDataProvider::SetData(const WifiData& new_data) {
  got_data_ = true;
  const bool differs = data_.DiffersSignificantly(new_data);
  data_ = new_data;
  if (differs) {
    this->RunCallbacks();
  }
}

}  // namespace device
