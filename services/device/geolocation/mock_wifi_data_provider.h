// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GEOLOCATION_MOCK_WIFI_DATA_PROVIDER_H_
#define SERVICES_DEVICE_GEOLOCATION_MOCK_WIFI_DATA_PROVIDER_H_

#include "base/memory/weak_ptr.h"
#include "services/device/geolocation/wifi_data.h"
#include "services/device/geolocation/wifi_data_provider.h"

namespace device {

// A mock implementation of WifiDataProvider for testing.
class MockWifiDataProvider : public WifiDataProvider {
 public:
  // Factory method for use with WifiDataProvider::SetFactoryForTesting.
  static WifiDataProvider* GetInstance();

  static MockWifiDataProvider* CreateInstance();

  MockWifiDataProvider();

  MockWifiDataProvider(const MockWifiDataProvider&) = delete;
  MockWifiDataProvider& operator=(const MockWifiDataProvider&) = delete;

  // WifiDataProvider implementation.
  void StartDataProvider() override;
  void StopDataProvider() override;
  bool DelayedByPolicy() override;
  bool GetData(WifiData* data_out) override;
  void ForceRescan() override;

  void SetData(const WifiData& new_data);

  void set_got_data(bool got_data) { got_data_ = got_data; }

 private:
  ~MockWifiDataProvider() override;

  static MockWifiDataProvider* instance_;

  WifiData data_;
  bool got_data_ = true;
  base::WeakPtrFactory<MockWifiDataProvider> weak_factory_{this};
};

}  // namespace device

#endif  // SERVICES_DEVICE_GEOLOCATION_MOCK_WIFI_DATA_PROVIDER_H_
