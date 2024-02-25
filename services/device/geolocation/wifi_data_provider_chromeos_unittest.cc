// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/geolocation/wifi_data_provider_chromeos.h"

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/dbus/shill/shill_clients.h"
#include "chromeos/ash/components/dbus/shill/shill_manager_client.h"
#include "chromeos/ash/components/network/geolocation_handler.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace device {

class GeolocationChromeOsWifiDataProviderTest : public testing::Test {
 protected:
  GeolocationChromeOsWifiDataProviderTest()
      : task_environment_(
            base::test::SingleThreadTaskEnvironment::MainThreadType::UI) {}

  void SetUp() override {
    provider_ = new WifiDataProviderChromeOs();
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    provider_.reset();
  }

  bool GetAccessPointData() {
    auto wifi_data = provider_->GetWifiDataForTesting();
    if (!wifi_data) {
      return false;
    }
    ap_data_ = std::move(wifi_data->access_point_data);
    return true;
  }

  void AddAccessPoints(int ssids, int aps_per_ssid) {
    for (int i = 0; i < ssids; ++i) {
      for (int j = 0; j < aps_per_ssid; ++j) {
        base::Value::Dict properties;
        std::string mac_address = base::StringPrintf(
            "%02X:%02X:%02X:%02X:%02X:%02X", i, j, 3, 4, 5, 6);
        std::string channel = base::NumberToString(i * 10 + j);
        std::string strength = base::NumberToString(i * 100 + j);
        properties.Set(shill::kGeoMacAddressProperty, mac_address);
        properties.Set(shill::kGeoChannelProperty, channel);
        properties.Set(shill::kGeoSignalStrengthProperty, strength);
        network_handler_test_helper_.manager_test()->AddGeoNetwork(
            shill::kGeoWifiAccessPointsProperty, std::move(properties));
      }
    }
    base::RunLoop().RunUntilIdle();
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  ash::NetworkHandlerTestHelper network_handler_test_helper_;
  scoped_refptr<WifiDataProviderChromeOs> provider_;
  raw_ptr<ash::ShillManagerClient::TestInterface> manager_test_;
  WifiData::AccessPointDataSet ap_data_;
};

TEST_F(GeolocationChromeOsWifiDataProviderTest, NoAccessPoints) {
  base::RunLoop().RunUntilIdle();
  // Initial call to GetAccessPointData requests data and will return false.
  EXPECT_FALSE(GetAccessPointData());
  base::RunLoop().RunUntilIdle();
  // Additional call to GetAccessPointData also returns false with no devices.
  EXPECT_FALSE(GetAccessPointData());
  EXPECT_EQ(0u, ap_data_.size());
}

TEST_F(GeolocationChromeOsWifiDataProviderTest, GetOneAccessPoint) {
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(GetAccessPointData());

  AddAccessPoints(1, 1);
  EXPECT_TRUE(GetAccessPointData());
  ASSERT_EQ(1u, ap_data_.size());
  EXPECT_EQ("00:00:03:04:05:06", ap_data_.begin()->mac_address);
}

TEST_F(GeolocationChromeOsWifiDataProviderTest, GetManyAccessPoints) {
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(GetAccessPointData());

  AddAccessPoints(3, 4);
  EXPECT_TRUE(GetAccessPointData());
  ASSERT_EQ(12u, ap_data_.size());
}

}  // namespace device
