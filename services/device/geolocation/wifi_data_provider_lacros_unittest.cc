// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/geolocation/wifi_data_provider_lacros.h"

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "services/device/geolocation/wifi_polling_policy.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr int kDefaultIntervalMillis = 200;
constexpr int kNoChangeIntervalMillis = 300;
constexpr int kTwoNoChangeIntervalMillis = 400;
constexpr int kNoWifiIntervalMillis = 100;

}  // namespace

namespace device {

class WifiDataProviderLacrosTest : public testing::Test {
 public:
  void SetUp() override {
    WifiPollingPolicy::Initialize(
        std::make_unique<GenericWifiPollingPolicy<
            kDefaultIntervalMillis, kNoChangeIntervalMillis,
            kTwoNoChangeIntervalMillis, kNoWifiIntervalMillis>>());
    polling_policy_ = WifiPollingPolicy::Get();
    provider_ = new WifiDataProviderLacros();
    Test::SetUp();
  }

  void TearDown() override {
    polling_policy_ = nullptr;
    WifiPollingPolicy::Shutdown();
    Test::TearDown();
  }

  void CallDidWifiScanTask(bool service_initialized,
                           bool data_available,
                           int num_aps) {
    std::vector<crosapi::mojom::AccessPointDataPtr> ap_data_vector;
    for (int i = 0; i < num_aps; ++i) {
      auto ap_data = crosapi::mojom::AccessPointData::New();
      ap_data->mac_address = base::NumberToString16(i);
      ap_data->radio_signal_strength = i;
      ap_data->channel = i;
      ap_data->signal_to_noise = i;
      ap_data->ssid = base::NumberToString16(i);
      ap_data_vector.push_back(std::move(ap_data));
    }
    provider_->DidWifiScanTaskForTesting(service_initialized, data_available,
                                         base::TimeDelta(),
                                         std::move(ap_data_vector));
    base::RunLoop().RunUntilIdle();
  }

  WifiDataProviderLacros* provider() { return provider_.get(); }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  raw_ptr<WifiPollingPolicy> polling_policy_ = nullptr;
  scoped_refptr<WifiDataProviderLacros> provider_;
};

TEST_F(WifiDataProviderLacrosTest, NoDataWhenNoScanTaskHasCompleted) {
  WifiData wifi_data;

  // By default the provider should not have any wifi data.
  EXPECT_FALSE(provider()->GetData(&wifi_data));
  EXPECT_TRUE(wifi_data.access_point_data.empty());
}

TEST_F(WifiDataProviderLacrosTest, NoDataWhenServiceIsNotInitialized) {
  WifiData wifi_data;
  EXPECT_FALSE(provider()->GetData(&wifi_data));

  // The provider should indicate it does not have any wifi data if the service
  // is reported to be uninitialized.
  CallDidWifiScanTask(false, false, 0);
  EXPECT_FALSE(provider()->GetData(&wifi_data));
  EXPECT_TRUE(wifi_data.access_point_data.empty());
}

TEST_F(WifiDataProviderLacrosTest, RespondsCorrectlyWhenDataIsNotAvailable) {
  WifiData wifi_data;
  EXPECT_FALSE(provider()->GetData(&wifi_data));

  // Simulate a successful callback to DidWifiScanTask().
  CallDidWifiScanTask(true, true, 1);
  EXPECT_TRUE(provider()->GetData(&wifi_data));
  EXPECT_EQ(1u, wifi_data.access_point_data.size());

  // The provider should continue to report the most recent wifi data
  // successfully returned by the callback.
  CallDidWifiScanTask(true, false, 0);
  EXPECT_TRUE(provider()->GetData(&wifi_data));
  EXPECT_EQ(1u, wifi_data.access_point_data.size());
}

TEST_F(WifiDataProviderLacrosTest, DoesNotNotifyWhenIncommingDataIsIdentical) {
  WifiData wifi_data;
  EXPECT_FALSE(provider()->GetData(&wifi_data));

  int times_called = 0;
  auto wifi_data_callback =
      base::BindLambdaForTesting([&]() { times_called++; });
  provider()->AddCallback(&wifi_data_callback);

  // Simulate a successful callback to DidWifiScanTask().
  CallDidWifiScanTask(true, true, 2);
  EXPECT_TRUE(provider()->GetData(&wifi_data));
  EXPECT_EQ(2u, wifi_data.access_point_data.size());
  EXPECT_EQ(1, times_called);

  // Simulate a successful callback to DidWifiScanTask() with the same data as
  // before. This should not trigger any callbacks.
  CallDidWifiScanTask(true, false, 2);
  EXPECT_TRUE(provider()->GetData(&wifi_data));
  EXPECT_EQ(2u, wifi_data.access_point_data.size());
  EXPECT_EQ(1, times_called);

  provider()->RemoveCallback(&wifi_data_callback);
}

TEST_F(WifiDataProviderLacrosTest,
       NotifiesWhenIncommingDataIsSufficientlyDifferent) {
  WifiData wifi_data;
  EXPECT_FALSE(provider()->GetData(&wifi_data));

  int times_called = 0;
  WifiData original_wifi_data = wifi_data;
  auto wifi_data_callback =
      base::BindLambdaForTesting([&]() { times_called++; });
  provider()->AddCallback(&wifi_data_callback);

  // Simulate a successful callback to DidWifiScanTask().
  CallDidWifiScanTask(true, true, 2);
  EXPECT_TRUE(provider()->GetData(&wifi_data));
  EXPECT_EQ(2u, wifi_data.access_point_data.size());
  EXPECT_EQ(1, times_called);

  // Simulate a successful callback to DidWifiScanTask() with the significantly
  // different wifi data (>50% difference). This should trigger callbacks with
  // the new data.
  original_wifi_data = wifi_data;
  CallDidWifiScanTask(true, true, 5);
  EXPECT_TRUE(provider()->GetData(&wifi_data));
  EXPECT_EQ(5u, wifi_data.access_point_data.size());
  EXPECT_EQ(2, times_called);
  EXPECT_TRUE(original_wifi_data.DiffersSignificantly(wifi_data));

  // Simulate a successful callback to DidWifiScanTask() with very similar wifi
  // data (<50% difference). This should not trigger callbacks.
  original_wifi_data = wifi_data;
  CallDidWifiScanTask(true, true, 6);
  EXPECT_TRUE(provider()->GetData(&wifi_data));
  EXPECT_EQ(6u, wifi_data.access_point_data.size());
  EXPECT_EQ(2, times_called);
  EXPECT_FALSE(original_wifi_data.DiffersSignificantly(wifi_data));

  provider()->RemoveCallback(&wifi_data_callback);
}

}  // namespace device
