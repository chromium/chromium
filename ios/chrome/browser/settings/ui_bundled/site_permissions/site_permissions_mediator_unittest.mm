// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/site_permissions/site_permissions_mediator.h"

#import "components/content_settings/core/browser/host_content_settings_map.h"
#import "components/content_settings/core/common/content_settings.h"
#import "components/content_settings/core/common/content_settings_pattern.h"
#import "components/content_settings/core/common/content_settings_types.h"
#import "ios/chrome/browser/content_settings/model/host_content_settings_map_factory.h"
#import "ios/chrome/browser/favicon/model/favicon_loader.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/settings/ui_bundled/site_permissions/site_permissions_consumer.h"
#import "ios/chrome/browser/settings/ui_bundled/site_permissions/site_permissions_site_item.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "url/gurl.h"

class SitePermissionsMediatorTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    TestProfileIOS::Builder builder;
    profile_ = std::move(builder).Build();
    settings_map_ =
        ios::HostContentSettingsMapFactory::GetForProfile(profile_.get());
    favicon_loader_ =
        IOSChromeFaviconLoaderFactory::GetForProfile(profile_.get());
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  scoped_refptr<HostContentSettingsMap> settings_map_;
  raw_ptr<FaviconLoader> favicon_loader_ = nullptr;
};

// Test that consumer receives empty array when no site permissions exist.
TEST_F(SitePermissionsMediatorTest, TestEmptyPermissions) {
  SitePermissionsMediator* mediator = [[SitePermissionsMediator alloc]
      initWithHostContentSettingsMap:settings_map_.get()
                       faviconLoader:favicon_loader_];

  id consumer = OCMProtocolMock(@protocol(SitePermissionsConsumer));
  OCMExpect([consumer setSitePermissionsSiteItems:@[]]);

  mediator.consumer = consumer;

  EXPECT_OCMOCK_VERIFY(consumer);
  [mediator disconnect];
}

// Test that mediator aggregates origins from mic, camera, and geolocation
// permissions.
TEST_F(SitePermissionsMediatorTest, TestPopulatedPermissions) {
  GURL mic_url("https://mic.example.com");
  GURL camera_url("https://camera.example.com");
  GURL both_url("https://both.example.com");

  settings_map_->SetContentSettingDefaultScope(
      mic_url, mic_url, ContentSettingsType::MEDIASTREAM_MIC,
      CONTENT_SETTING_ALLOW);
  settings_map_->SetContentSettingDefaultScope(
      camera_url, camera_url, ContentSettingsType::MEDIASTREAM_CAMERA,
      CONTENT_SETTING_BLOCK);
  settings_map_->SetContentSettingDefaultScope(
      both_url, both_url, ContentSettingsType::MEDIASTREAM_MIC,
      CONTENT_SETTING_ALLOW);
  settings_map_->SetContentSettingDefaultScope(
      both_url, both_url, ContentSettingsType::MEDIASTREAM_CAMERA,
      CONTENT_SETTING_BLOCK);

  SitePermissionsMediator* mediator = [[SitePermissionsMediator alloc]
      initWithHostContentSettingsMap:settings_map_.get()
                       faviconLoader:favicon_loader_];

  id consumer = OCMProtocolMock(@protocol(SitePermissionsConsumer));
  OCMExpect([consumer
      setSitePermissionsSiteItems:[OCMArg checkWithBlock:^BOOL(NSArray* items) {
        if ([items count] != 3) {
          return NO;
        }
        SitePermissionsSiteItem* item0 = items[0];
        SitePermissionsSiteItem* item1 = items[1];
        SitePermissionsSiteItem* item2 = items[2];
        return [item0.origin isEqualToString:@"https://both.example.com"] &&
               [item1.origin isEqualToString:@"https://camera.example.com"] &&
               [item2.origin isEqualToString:@"https://mic.example.com"];
      }]]);

  mediator.consumer = consumer;

  EXPECT_OCMOCK_VERIFY(consumer);
  [mediator disconnect];
}

// Test that wildcard default patterns are not exposed as site entries.
TEST_F(SitePermissionsMediatorTest, TestWildcardsIgnored) {
  settings_map_->SetDefaultContentSetting(ContentSettingsType::MEDIASTREAM_MIC,
                                          CONTENT_SETTING_BLOCK);

  SitePermissionsMediator* mediator = [[SitePermissionsMediator alloc]
      initWithHostContentSettingsMap:settings_map_.get()
                       faviconLoader:favicon_loader_];

  id consumer = OCMProtocolMock(@protocol(SitePermissionsConsumer));
  OCMExpect([consumer setSitePermissionsSiteItems:@[]]);

  mediator.consumer = consumer;

  EXPECT_OCMOCK_VERIFY(consumer);
  [mediator disconnect];
}

// Test that changing content settings notifies consumer with updated items.
TEST_F(SitePermissionsMediatorTest, TestObserverUpdates) {
  SitePermissionsMediator* mediator = [[SitePermissionsMediator alloc]
      initWithHostContentSettingsMap:settings_map_.get()
                       faviconLoader:favicon_loader_];

  id consumer = OCMProtocolMock(@protocol(SitePermissionsConsumer));
  OCMExpect([consumer setSitePermissionsSiteItems:@[]]);

  mediator.consumer = consumer;
  EXPECT_OCMOCK_VERIFY(consumer);

  GURL new_site("https://new.example.com");
  OCMExpect([consumer
      setSitePermissionsSiteItems:[OCMArg checkWithBlock:^BOOL(NSArray* items) {
        if ([items count] != 1) {
          return NO;
        }
        SitePermissionsSiteItem* item = items[0];
        return [item.origin isEqualToString:@"https://new.example.com"];
      }]]);

  settings_map_->SetContentSettingDefaultScope(
      new_site, new_site, ContentSettingsType::MEDIASTREAM_MIC,
      CONTENT_SETTING_ALLOW);

  EXPECT_OCMOCK_VERIFY(consumer);
  [mediator disconnect];
}
