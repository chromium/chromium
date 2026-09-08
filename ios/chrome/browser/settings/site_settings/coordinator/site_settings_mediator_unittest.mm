// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/site_settings/coordinator/site_settings_mediator.h"

#import "base/notreached.h"
#import "components/content_settings/core/browser/host_content_settings_map.h"
#import "components/content_settings/core/common/content_settings.h"
#import "components/content_settings/core/common/content_settings_types.h"
#import "ios/chrome/browser/content_settings/model/host_content_settings_map_factory.h"
#import "ios/chrome/browser/settings/site_settings/ui/site_settings_consumer.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

// Fake consumer that records calls to SiteSettingsConsumer.
@interface FakeSiteSettingsConsumer : NSObject <SiteSettingsConsumer>
@property(nonatomic, assign) BOOL locationCategoryEnabled;
@property(nonatomic, assign) ContentSetting micSetting;
@property(nonatomic, assign) ContentSetting cameraSetting;
@property(nonatomic, assign) ContentSetting locationSetting;
@end

@implementation FakeSiteSettingsConsumer

- (instancetype)init {
  self = [super init];
  if (self) {
    _micSetting = CONTENT_SETTING_DEFAULT;
    _cameraSetting = CONTENT_SETTING_DEFAULT;
    _locationSetting = CONTENT_SETTING_DEFAULT;
  }
  return self;
}

- (void)setLocationCategoryEnabled:(BOOL)enabled {
  _locationCategoryEnabled = enabled;
}

- (void)setDefaultSetting:(ContentSetting)setting
                  forType:(ContentSettingsType)type {
  switch (type) {
    case ContentSettingsType::MEDIASTREAM_MIC:
      _micSetting = setting;
      break;
    case ContentSettingsType::MEDIASTREAM_CAMERA:
      _cameraSetting = setting;
      break;
    case ContentSettingsType::GEOLOCATION:
      _locationSetting = setting;
      break;
    default:
      NOTREACHED();
  }
}

@end

// Test fixture for SiteSettingsMediator.
class SiteSettingsMediatorTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    TestProfileIOS::Builder builder;
    profile_ = std::move(builder).Build();
    settings_map_ =
        ios::HostContentSettingsMapFactory::GetForProfile(profile_.get());
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  scoped_refptr<HostContentSettingsMap> settings_map_;
};

// Tests that setting the consumer updates it with default settings and location
// enablement.
TEST_F(SiteSettingsMediatorTest, TestInitialLoad) {
  SiteSettingsMediator* mediator = [[SiteSettingsMediator alloc]
      initWithHostContentSettingsMap:settings_map_.get()];

  FakeSiteSettingsConsumer* consumer = [[FakeSiteSettingsConsumer alloc] init];
  mediator.consumer = consumer;

  EXPECT_FALSE(consumer.locationCategoryEnabled);
  EXPECT_EQ(CONTENT_SETTING_ASK, consumer.micSetting);
  EXPECT_EQ(CONTENT_SETTING_ASK, consumer.cameraSetting);
  EXPECT_EQ(CONTENT_SETTING_DEFAULT, consumer.locationSetting);

  [mediator disconnect];
}

// Tests that changing a default content setting notifies the consumer.
TEST_F(SiteSettingsMediatorTest, TestSettingChange) {
  SiteSettingsMediator* mediator = [[SiteSettingsMediator alloc]
      initWithHostContentSettingsMap:settings_map_.get()];

  FakeSiteSettingsConsumer* consumer = [[FakeSiteSettingsConsumer alloc] init];
  mediator.consumer = consumer;

  EXPECT_EQ(CONTENT_SETTING_ASK, consumer.micSetting);

  settings_map_->SetDefaultContentSetting(ContentSettingsType::MEDIASTREAM_MIC,
                                          CONTENT_SETTING_BLOCK);

  EXPECT_EQ(CONTENT_SETTING_BLOCK, consumer.micSetting);

  [mediator disconnect];
}

// Tests that disconnecting clears observations and stops updates.
TEST_F(SiteSettingsMediatorTest, TestDisconnect) {
  SiteSettingsMediator* mediator = [[SiteSettingsMediator alloc]
      initWithHostContentSettingsMap:settings_map_.get()];

  FakeSiteSettingsConsumer* consumer = [[FakeSiteSettingsConsumer alloc] init];
  mediator.consumer = consumer;
  [mediator disconnect];

  EXPECT_EQ(nil, mediator.consumer);

  // Changing setting after disconnect should not trigger further updates.
  settings_map_->SetDefaultContentSetting(ContentSettingsType::MEDIASTREAM_MIC,
                                          CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_ASK, consumer.micSetting);
}
