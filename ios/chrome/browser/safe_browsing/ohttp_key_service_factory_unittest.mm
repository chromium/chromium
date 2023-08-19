// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/ohttp_key_service_factory.h"

#import "base/test/scoped_feature_list.h"
#import "components/safe_browsing/core/browser/hashprefix_realtime/hash_realtime_utils.h"
#import "components/safe_browsing/core/common/features.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

class OhttpKeyServiceFactoryTest : public PlatformTest {
 protected:
  OhttpKeyServiceFactoryTest() = default;

  base::test::ScopedFeatureList feature_list_;
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<ChromeBrowserState> browser_state_;

 private:
  safe_browsing::hash_realtime_utils::GoogleChromeBrandingPretenderForTesting
      apply_branding_;
};

// Checks that OhttpKeyServiceFactory returns a null for an
// off-the-record browser state, but returns a non-null instance for a regular
// browser state, when both hash-prefix real-time lookups and hash-real-time
// over-Ohttp lookups are enabled.
TEST_F(OhttpKeyServiceFactoryTest, BothFeaturesEnabled) {
  feature_list_.InitWithFeatures(
      /*enabled_features=*/{safe_browsing::kHashRealTimeOverOhttp,
                            safe_browsing::kHashPrefixRealTimeLookups},
      /*disabled_features=*/{});
  browser_state_ = TestChromeBrowserState::Builder().Build();

  // The factory should return null for an off-the-record browser state.
  EXPECT_FALSE(OhttpKeyServiceFactory::GetForBrowserState(
      browser_state_->GetOffTheRecordChromeBrowserState()));

  // There should be a non-null instance for a regular browser state.
  EXPECT_TRUE(OhttpKeyServiceFactory::GetForBrowserState(browser_state_.get()));
}

// Checks that OhttpKeyServiceFactory returns a null for an
// off-the-record browser state, but returns a non-null instance for a regular
// browser state, when hash-prefix real-time lookups are enabled.
TEST_F(OhttpKeyServiceFactoryTest, HashPrefixRealTimeLookupsEnabled) {
  feature_list_.InitWithFeatures(
      /*enabled_features=*/{safe_browsing::kHashPrefixRealTimeLookups},
      /*disabled_features=*/{});
  browser_state_ = TestChromeBrowserState::Builder().Build();

  // The factory should return null for an off-the-record browser state.
  EXPECT_FALSE(OhttpKeyServiceFactory::GetForBrowserState(
      browser_state_->GetOffTheRecordChromeBrowserState()));

  // There should be a non-null instance for a regular browser state.
  EXPECT_TRUE(OhttpKeyServiceFactory::GetForBrowserState(browser_state_.get()));
}

// Checks that OhttpKeyServiceFactory returns a null for an
// off-the-record browser state, but returns a non-null instance for a regular
// browser state, when hash-real-time-lookups-over-Ohttp are enabled.
TEST_F(OhttpKeyServiceFactoryTest, HashRealTimeOverOhttpEnabled) {
  feature_list_.InitWithFeatures(
      /*enabled_features=*/{safe_browsing::kHashRealTimeOverOhttp},
      /*disabled_features=*/{});
  browser_state_ = TestChromeBrowserState::Builder().Build();

  // The factory should return null for an off-the-record browser state.
  EXPECT_FALSE(OhttpKeyServiceFactory::GetForBrowserState(
      browser_state_->GetOffTheRecordChromeBrowserState()));

  // There should be a non-null instance for a regular browser state.
  EXPECT_TRUE(OhttpKeyServiceFactory::GetForBrowserState(browser_state_.get()));
}

// Checks that OhttpKeyServiceFactory returns a null for both
// off-the-record and regular browser states, when both hash-prefix real-time
// lookups and hash-real-time-over-Ohttp lookups are disabled.
TEST_F(OhttpKeyServiceFactoryTest, BothFeaturesDisabled) {
  feature_list_.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{safe_browsing::kHashRealTimeOverOhttp,
                             safe_browsing::kHashPrefixRealTimeLookups});
  browser_state_ = TestChromeBrowserState::Builder().Build();

  // The factory should return null for an off-the-record browser state.
  EXPECT_FALSE(OhttpKeyServiceFactory::GetForBrowserState(
      browser_state_->GetOffTheRecordChromeBrowserState()));

  // The factory should return null for a regular browser state.
  EXPECT_FALSE(
      OhttpKeyServiceFactory::GetForBrowserState(browser_state_.get()));
}
