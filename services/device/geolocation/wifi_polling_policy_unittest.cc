// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/geolocation/wifi_polling_policy.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {

const int kDefaultIntervalMillis = 200;
const int kNoChangeIntervalMillis = 300;
const int kTwoNoChangeIntervalMillis = 400;
const int kNoWifiIntervalMillis = 100;

}  // namespace

// Main test fixture
class GeolocationWifiPollingPolicyTest : public testing::Test {
 public:
  void SetUp() override {
    WifiPollingPolicy::Initialize(
        std::make_unique<GenericWifiPollingPolicy<
            kDefaultIntervalMillis, kNoChangeIntervalMillis,
            kTwoNoChangeIntervalMillis, kNoWifiIntervalMillis>>());
    polling_policy_ = WifiPollingPolicy::Get();
  }

  void TearDown() override {
    polling_policy_ = nullptr;
    WifiPollingPolicy::Shutdown();
  }

 protected:
  WifiPollingPolicy* polling_policy_ = nullptr;
};

TEST_F(GeolocationWifiPollingPolicyTest, CreateDestroy) {
  // Test fixture members were SetUp correctly.
  EXPECT_TRUE(polling_policy_);
}

// Tests that the InitialInterval is zero when the policy is first created.
TEST_F(GeolocationWifiPollingPolicyTest, InitialIntervalZero) {
  // The first call should return zero, indicating we may scan immediately.
  // Internally, the policy starts a new interval at the current time.
  EXPECT_EQ(0, polling_policy_->InitialInterval());

  // The second call should return the non-zero remainder of the interval
  // created by the first call.
  int interval = polling_policy_->InitialInterval();
  EXPECT_GT(interval, 0);
  EXPECT_LE(interval, kDefaultIntervalMillis);
}

// Tests that the PollingInterval is equal to the default polling interval when
// the policy is first created.
TEST_F(GeolocationWifiPollingPolicyTest, PollingIntervalNonZero) {
  // PollingInterval assumes it is only called immediately following a wifi
  // scan. The first call should start a new interval at the current time and
  // return the full duration of the new interval.
  EXPECT_EQ(kDefaultIntervalMillis, polling_policy_->PollingInterval());

  // The second call should return the non-zero remainder of the interval
  // created by the first call.
  int interval = polling_policy_->PollingInterval();
  EXPECT_GT(interval, 0);
  EXPECT_LE(interval, kDefaultIntervalMillis);
}

// Tests that the NoWifiInterval is equal to the default no-wifi interval when
// the policy is first created.
TEST_F(GeolocationWifiPollingPolicyTest, NoWifiIntervalNonZero) {
  // NoWifiInterval assumes it is only called immediately following a failed
  // attempt at a wifi scan. The first call should start a new interval at the
  // current time and return the full duration of the new interval.
  EXPECT_EQ(kNoWifiIntervalMillis, polling_policy_->NoWifiInterval());

  // The second call should return the non-zero remainder of the interval
  // created by the first call.
  int interval = polling_policy_->NoWifiInterval();
  EXPECT_GT(interval, 0);
  EXPECT_LE(interval, kNoWifiIntervalMillis);
}

// Calls UpdatePollingInterval once with unchanged scan results. Verifies that
// the no-change interval is used.
TEST_F(GeolocationWifiPollingPolicyTest, UpdatePollingIntervalOnce) {
  polling_policy_->UpdatePollingInterval(false);
  EXPECT_EQ(kNoChangeIntervalMillis, polling_policy_->PollingInterval());
}

// Calls UpdatePollingInterval twice with unchanged scan results. Verifies that
// the two-no-change interval is used.
TEST_F(GeolocationWifiPollingPolicyTest, UpdatePollingIntervalTwice) {
  polling_policy_->UpdatePollingInterval(false);
  polling_policy_->UpdatePollingInterval(false);
  EXPECT_EQ(kTwoNoChangeIntervalMillis, polling_policy_->PollingInterval());
}

// Calls UpdatePollingInterval three times with unchanged scan results. This
// should have the same effect as calling it twice.
TEST_F(GeolocationWifiPollingPolicyTest, UpdatePollingIntervalThrice) {
  polling_policy_->UpdatePollingInterval(false);
  polling_policy_->UpdatePollingInterval(false);
  polling_policy_->UpdatePollingInterval(false);
  EXPECT_EQ(kTwoNoChangeIntervalMillis, polling_policy_->PollingInterval());
}

// Calls UpdatePollingInterval twice with unchanged scan results and then once
// with differing results. Verifies that the default interval is used.
TEST_F(GeolocationWifiPollingPolicyTest, UpdatePollingIntervalResultsDiffer) {
  polling_policy_->UpdatePollingInterval(false);
  polling_policy_->UpdatePollingInterval(false);
  polling_policy_->UpdatePollingInterval(true);
  EXPECT_EQ(kDefaultIntervalMillis, polling_policy_->PollingInterval());
}

TEST_F(GeolocationWifiPollingPolicyTest, ShorterInterval) {
  // Ask for a polling interval.
  EXPECT_EQ(kDefaultIntervalMillis, polling_policy_->PollingInterval());

  // Now ask for a no-wifi interval, which is shorter. The returned interval
  // must be no longer than the shorter of the two intervals.
  int interval = polling_policy_->NoWifiInterval();
  EXPECT_GT(interval, 0);
  EXPECT_LE(interval, kNoWifiIntervalMillis);
}

TEST_F(GeolocationWifiPollingPolicyTest, LongerInterval) {
  // Ask for a no-wifi interval.
  EXPECT_EQ(kNoWifiIntervalMillis, polling_policy_->NoWifiInterval());

  // Now ask for a polling interval, which is longer. The returned interval
  // must be no longer than the shorter of the two intervals.
  int interval = polling_policy_->PollingInterval();
  EXPECT_GT(interval, 0);
  EXPECT_LE(interval, kNoWifiIntervalMillis);
}

}  // namespace device
