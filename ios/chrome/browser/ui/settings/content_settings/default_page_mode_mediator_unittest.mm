// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/content_settings/default_page_mode_mediator.h"

#import "components/content_settings/core/browser/host_content_settings_map.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/test/mock_tracker.h"
#import "ios/chrome/browser/content_settings/host_content_settings_map_factory.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/ui/settings/content_settings/default_page_mode_consumer.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

class DefaultPageModeMediatorTest : public PlatformTest {
 protected:
  DefaultPageModeMediatorTest() {
    TestChromeBrowserState::Builder test_cbs_builder;
    chrome_browser_state_ = test_cbs_builder.Build();
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
};

// Tests that the pref and the mediator are playing nicely together.
TEST_F(DefaultPageModeMediatorTest, TestPref) {
  feature_engagement::test::MockTracker tracker;
  scoped_refptr<HostContentSettingsMap> settings_map(
      ios::HostContentSettingsMapFactory::GetForBrowserState(
          chrome_browser_state_.get()));
  settings_map->SetContentSettingCustomScope(
      ContentSettingsPattern::Wildcard(), ContentSettingsPattern::Wildcard(),
      ContentSettingsType::REQUEST_DESKTOP_SITE, CONTENT_SETTING_BLOCK);

  DefaultPageModeMediator* mediator =
      [[DefaultPageModeMediator alloc] initWithSettingsMap:settings_map.get()
                                                   tracker:&tracker];

  // Check that the consumer is correctly updated when set.
  id consumer = OCMProtocolMock(@protocol(DefaultPageModeConsumer));
  OCMExpect([consumer setDefaultPageMode:DefaultPageModeMobile]);

  mediator.consumer = consumer;

  EXPECT_OCMOCK_VERIFY(consumer);

  // Check that the consumer is correctly updated when the pref is changed.
  OCMExpect([consumer setDefaultPageMode:DefaultPageModeDesktop]);

  settings_map->SetContentSettingCustomScope(
      ContentSettingsPattern::Wildcard(), ContentSettingsPattern::Wildcard(),
      ContentSettingsType::REQUEST_DESKTOP_SITE, CONTENT_SETTING_ALLOW);

  EXPECT_OCMOCK_VERIFY(consumer);

  // Check that the pref is correctly updated when the mediator is asked.
  ASSERT_EQ(CONTENT_SETTING_ALLOW,
            settings_map->GetContentSetting(
                GURL(), GURL(), ContentSettingsType::REQUEST_DESKTOP_SITE));
  EXPECT_CALL(tracker,
              NotifyEvent(feature_engagement::events::kDefaultSiteViewUsed))
      .Times(1);
  [mediator didSelectMode:DefaultPageModeMobile];

  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            settings_map->GetContentSetting(
                GURL(), GURL(), ContentSettingsType::REQUEST_DESKTOP_SITE));
}
