// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/geolocation/wifi_data_provider_common.h"

#include <memory>

#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "services/device/geolocation/wifi_data_provider_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::AnyNumber;
using testing::AtLeast;
using testing::DoAll;
using testing::Invoke;
using testing::InvokeWithoutArgs;
using testing::Return;
using testing::SetArgPointee;
using testing::WithArgs;

namespace device {

class MockWlanApi : public WifiDataProviderCommon::WlanApiInterface {
 public:
  MockWlanApi() {
    ON_CALL(*this, GetAccessPointData(_))
        .WillByDefault(DoAll(SetArgPointee<0>(data_out_), Return(true)));
  }

  MOCK_METHOD1(GetAccessPointData, bool(WifiData::AccessPointDataSet* data));

 private:
  WifiData::AccessPointDataSet data_out_;
};

class MockPollingPolicy : public WifiPollingPolicy {
 public:
  MockPollingPolicy() {
    ON_CALL(*this, InitialInterval()).WillByDefault(Return(0));
    ON_CALL(*this, PollingInterval()).WillByDefault(Return(1));
    ON_CALL(*this, NoWifiInterval()).WillByDefault(Return(1));
    // We are not interested in calls to UpdatePollingInterval() method.
    EXPECT_CALL(*this, UpdatePollingInterval(_)).Times(AnyNumber());
  }

  // WifiPollingPolicy implementation.
  MOCK_METHOD1(UpdatePollingInterval, void(bool));
  MOCK_METHOD0(InitialInterval, int());
  MOCK_METHOD0(PollingInterval, int());
  MOCK_METHOD0(NoWifiInterval, int());
};

class WifiDataProviderCommonWithMock : public WifiDataProviderCommon {
 public:
  WifiDataProviderCommonWithMock() : wlan_api_(new MockWlanApi) {}

  // WifiDataProviderCommon
  std::unique_ptr<WlanApiInterface> CreateWlanApi() override {
    return std::move(wlan_api_);
  }
  std::unique_ptr<WifiPollingPolicy> CreatePollingPolicy() override {
    auto policy = std::make_unique<MockPollingPolicy>();
    // Save a pointer to the MockPollingPolicy.
    polling_policy_ = policy.get();
    return std::move(policy);
  }

  std::unique_ptr<MockWlanApi> wlan_api_;
  MockPollingPolicy* polling_policy_ = nullptr;

 private:
  ~WifiDataProviderCommonWithMock() override = default;

  DISALLOW_COPY_AND_ASSIGN(WifiDataProviderCommonWithMock);
};

// Main test fixture
class GeolocationWifiDataProviderCommonTest : public testing::Test {
 public:
  GeolocationWifiDataProviderCommonTest()
      : task_environment_(
            base::test::SingleThreadTaskEnvironment::MainThreadType::UI),
        wifi_data_callback_(base::DoNothing()) {}

  void TearDownProvider() {
    provider_->RemoveCallback(&wifi_data_callback_);
    provider_->StopDataProvider();
    provider_ = nullptr;
    wlan_api_ = nullptr;
  }

  // Some usage patterns cause the provider to be created and destroyed
  // frequently. Allow tests to simulate this behavior by recreating the
  // provider without resetting WifiPollingPolicy.
  void RecreateProvider() {
    if (provider_)
      TearDownProvider();
    provider_ = new WifiDataProviderCommonWithMock;
    provider_->AddCallback(&wifi_data_callback_);
    wlan_api_ = provider_->wlan_api_.get();

    // Initialize WifiPollingPolicy early so we can watch for calls to mocked
    // functions. Normally the policy is initialized in StartDataProvider.
    //
    // The policy should be initialized only once to ensure its state is
    // retained across restarts of the provider.
    if (!polling_policy_) {
      WifiPollingPolicy::Initialize(provider_->CreatePollingPolicy());
      polling_policy_ = provider_->polling_policy_;
    }
  }

  void SetUp() override { RecreateProvider(); }

  void TearDown() override {
    TearDownProvider();
    WifiPollingPolicy::Shutdown();
    polling_policy_ = nullptr;
  }

 protected:
  const base::test::SingleThreadTaskEnvironment task_environment_;
  WifiDataProviderManager::WifiDataUpdateCallback wifi_data_callback_;
  scoped_refptr<WifiDataProviderCommonWithMock> provider_;

  MockWlanApi* wlan_api_ = nullptr;
  MockPollingPolicy* polling_policy_ = nullptr;
};

TEST_F(GeolocationWifiDataProviderCommonTest, CreateDestroy) {
  // Test fixture members were SetUp correctly.
  EXPECT_TRUE(provider_);
  EXPECT_TRUE(wlan_api_);
  EXPECT_TRUE(polling_policy_);
}

TEST_F(GeolocationWifiDataProviderCommonTest, NoWifi) {
  base::RunLoop run_loop;
  EXPECT_CALL(*polling_policy_, InitialInterval()).Times(1);
  EXPECT_CALL(*polling_policy_, NoWifiInterval()).Times(AtLeast(1));
  EXPECT_CALL(*wlan_api_, GetAccessPointData(_))
      .WillOnce(InvokeWithoutArgs([&run_loop]() {
        run_loop.Quit();
        return false;
      }));

  provider_->StartDataProvider();
  run_loop.Run();
}

TEST_F(GeolocationWifiDataProviderCommonTest, IntermittentWifi) {
  base::RunLoop run_loop;
  EXPECT_CALL(*polling_policy_, InitialInterval()).Times(1);
  EXPECT_CALL(*polling_policy_, PollingInterval()).Times(AtLeast(1));
  EXPECT_CALL(*polling_policy_, NoWifiInterval()).Times(1);
  EXPECT_CALL(*wlan_api_, GetAccessPointData(_))
      .WillOnce(Return(true))
      .WillOnce(InvokeWithoutArgs([&run_loop]() {
        run_loop.Quit();
        return false;
      }));

  provider_->StartDataProvider();
  run_loop.Run();
}

// This test runs StartDataProvider() and expects that GetAccessPointData() is
// called. The retrieved WifiData is expected to be empty.
TEST_F(GeolocationWifiDataProviderCommonTest, DoAnEmptyScan) {
  base::RunLoop run_loop;

  EXPECT_CALL(*polling_policy_, InitialInterval()).Times(1);
  EXPECT_CALL(*polling_policy_, PollingInterval()).Times(AtLeast(1));
  EXPECT_CALL(*wlan_api_, GetAccessPointData(_))
      .WillOnce(InvokeWithoutArgs([&run_loop]() {
        run_loop.Quit();
        return true;
      }));

  provider_->StartDataProvider();
  run_loop.Run();

  WifiData data;
  EXPECT_TRUE(provider_->GetData(&data));
  EXPECT_TRUE(data.access_point_data.empty());
}

// This test runs StartDataProvider() and expects that GetAccessPointData() is
// called. Some mock WifiData is returned then and expected to be retrieved.
TEST_F(GeolocationWifiDataProviderCommonTest, DoScanWithResults) {
  base::RunLoop run_loop;

  EXPECT_CALL(*polling_policy_, InitialInterval()).Times(1);
  EXPECT_CALL(*polling_policy_, PollingInterval()).Times(AtLeast(1));
  AccessPointData single_access_point;
  single_access_point.channel = 2;
  single_access_point.mac_address = 3;
  single_access_point.radio_signal_strength = 4;
  single_access_point.signal_to_noise = 5;
  single_access_point.ssid = base::ASCIIToUTF16("foossid");

  WifiData::AccessPointDataSet data_out({single_access_point});

  EXPECT_CALL(*wlan_api_, GetAccessPointData(_))
      .WillOnce(WithArgs<0>(
          Invoke([&data_out, &run_loop](WifiData::AccessPointDataSet* data) {
            *data = data_out;
            run_loop.Quit();
            return true;
          })));

  provider_->StartDataProvider();
  run_loop.Run();

  WifiData data;
  EXPECT_TRUE(provider_->GetData(&data));
  ASSERT_EQ(1u, data.access_point_data.size());
  EXPECT_EQ(single_access_point.ssid, data.access_point_data.begin()->ssid);
}

TEST_F(GeolocationWifiDataProviderCommonTest, DelayedByPolicy) {
  static const int kPollingIntervalMillis = 1000;
  base::RunLoop run_loop;

  EXPECT_CALL(*polling_policy_, InitialInterval())
      // Initial scan: no delay
      .WillOnce(Return(0))
      // Third scan (after recreating the provider): scheduled after a delay
      .WillOnce(Return(kPollingIntervalMillis));
  EXPECT_CALL(*polling_policy_, PollingInterval())
      // Second scan: scheduled after a delay
      .WillOnce(Return(kPollingIntervalMillis));

  // Simulate a successful scan that found no wifi APs.
  EXPECT_CALL(*wlan_api_, GetAccessPointData(_))
      .WillOnce(InvokeWithoutArgs([&run_loop]() {
        run_loop.Quit();
        return true;
      }));

  // The initial scan is scheduled with InitialInterval and should not be
  // delayed.
  provider_->StartDataProvider();
  EXPECT_FALSE(provider_->DelayedByPolicy());

  // Allow the pending call to DoWifiScanTask to proceed. This will fetch our
  // mock wifi AP data and mark the first scan complete. It will also schedule
  // a new scan to occur after PollingInterval.
  run_loop.Run();
  EXPECT_TRUE(provider_->DelayedByPolicy());

  // Destroy the provider and recreate it, which will schedule a new scan.
  // InitialInterval is used to schedule the new scan, but unlike the first
  // scan which was scheduled immediately, it will now incur a delay.
  RecreateProvider();
  provider_->StartDataProvider();
  EXPECT_TRUE(provider_->DelayedByPolicy());
}

}  // namespace device
