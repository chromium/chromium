// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/site_settings/coordinator/site_settings_category_detail_mediator.h"

#import "components/content_settings/core/browser/host_content_settings_map.h"
#import "components/content_settings/core/common/content_settings.h"
#import "components/content_settings/core/common/content_settings_pattern.h"
#import "components/content_settings/core/common/content_settings_types.h"
#import "ios/chrome/browser/content_settings/model/host_content_settings_map_factory.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/settings/site_settings/ui/site_settings_category_detail_consumer.h"
#import "ios/chrome/browser/settings/site_settings/ui/site_settings_site_exception.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

// Fake consumer for SiteSettingsCategoryDetailMediator.
@interface FakeSiteSettingsCategoryDetailConsumer
    : NSObject <SiteSettingsCategoryDetailConsumer>
@property(nonatomic, assign) ContentSetting defaultSetting;
@property(nonatomic, strong) NSArray<SiteSettingsSiteException*>* allowedSites;
@property(nonatomic, strong)
    NSArray<SiteSettingsSiteException*>* notAllowedSites;
@property(nonatomic, assign) NSUInteger setAllowedSitesCallCount;
@end

@implementation FakeSiteSettingsCategoryDetailConsumer

- (instancetype)init {
  self = [super init];
  if (self) {
    _defaultSetting = CONTENT_SETTING_DEFAULT;
    _allowedSites = @[];
    _notAllowedSites = @[];
  }
  return self;
}

- (void)setDefaultSetting:(ContentSetting)setting {
  _defaultSetting = setting;
}

- (void)setAllowedSites:(NSArray<SiteSettingsSiteException*>*)allowedSites
        notAllowedSites:(NSArray<SiteSettingsSiteException*>*)notAllowedSites {
  _allowedSites = [allowedSites copy];
  _notAllowedSites = [notAllowedSites copy];
  _setAllowedSitesCallCount++;
}

@end

// Test fixture for SiteSettingsCategoryDetailMediator.
class SiteSettingsCategoryDetailMediatorTest : public PlatformTest {
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

// Tests that the consumer receives the initial default setting and empty lists.
TEST_F(SiteSettingsCategoryDetailMediatorTest, TestInitialLoad) {
  SiteSettingsCategoryDetailMediator* mediator =
      [[SiteSettingsCategoryDetailMediator alloc]
          initWithHostContentSettingsMap:settings_map_.get()
                           faviconLoader:favicon_loader_
                     contentSettingsType:ContentSettingsType::MEDIASTREAM_MIC];

  FakeSiteSettingsCategoryDetailConsumer* consumer =
      [[FakeSiteSettingsCategoryDetailConsumer alloc] init];
  mediator.consumer = consumer;

  EXPECT_EQ(CONTENT_SETTING_ASK, consumer.defaultSetting);
  EXPECT_EQ(0u, consumer.allowedSites.count);
  EXPECT_EQ(0u, consumer.notAllowedSites.count);

  [mediator disconnect];
}

// Tests that site exceptions are partitioned correctly into allowed and not
// allowed lists.
TEST_F(SiteSettingsCategoryDetailMediatorTest, TestPartitionExceptions) {
  GURL allowedUrl("https://allowed.com");
  GURL blockedUrl("https://blocked.com");

  settings_map_->SetContentSettingDefaultScope(
      allowedUrl, allowedUrl, ContentSettingsType::MEDIASTREAM_MIC,
      CONTENT_SETTING_ALLOW);
  settings_map_->SetContentSettingDefaultScope(
      blockedUrl, blockedUrl, ContentSettingsType::MEDIASTREAM_MIC,
      CONTENT_SETTING_BLOCK);

  SiteSettingsCategoryDetailMediator* mediator =
      [[SiteSettingsCategoryDetailMediator alloc]
          initWithHostContentSettingsMap:settings_map_.get()
                           faviconLoader:favicon_loader_
                     contentSettingsType:ContentSettingsType::MEDIASTREAM_MIC];

  FakeSiteSettingsCategoryDetailConsumer* consumer =
      [[FakeSiteSettingsCategoryDetailConsumer alloc] init];
  mediator.consumer = consumer;

  EXPECT_EQ(1u, consumer.allowedSites.count);
  EXPECT_NSEQ(@"allowed.com", consumer.allowedSites[0].formattedTitle);

  EXPECT_EQ(1u, consumer.notAllowedSites.count);
  EXPECT_NSEQ(@"blocked.com", consumer.notAllowedSites[0].formattedTitle);

  [mediator disconnect];
}

// Tests that mutating the default setting updates HostContentSettingsMap.
TEST_F(SiteSettingsCategoryDetailMediatorTest, TestSetDefaultSetting) {
  SiteSettingsCategoryDetailMediator* mediator =
      [[SiteSettingsCategoryDetailMediator alloc]
          initWithHostContentSettingsMap:settings_map_.get()
                           faviconLoader:favicon_loader_
                     contentSettingsType:ContentSettingsType::MEDIASTREAM_MIC];

  FakeSiteSettingsCategoryDetailConsumer* consumer =
      [[FakeSiteSettingsCategoryDetailConsumer alloc] init];
  mediator.consumer = consumer;

  EXPECT_EQ(CONTENT_SETTING_ASK, consumer.defaultSetting);

  [mediator setDefaultSetting:CONTENT_SETTING_BLOCK];

  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            settings_map_->GetDefaultContentSetting(
                ContentSettingsType::MEDIASTREAM_MIC, nullptr));
  EXPECT_EQ(CONTENT_SETTING_BLOCK, consumer.defaultSetting);

  [mediator disconnect];
}

// Tests that deleting a site exception clears it from HostContentSettingsMap.
TEST_F(SiteSettingsCategoryDetailMediatorTest, TestDeleteSettingForSite) {
  GURL allowedUrl("https://allowed.com");
  settings_map_->SetContentSettingDefaultScope(
      allowedUrl, allowedUrl, ContentSettingsType::MEDIASTREAM_MIC,
      CONTENT_SETTING_ALLOW);

  SiteSettingsCategoryDetailMediator* mediator =
      [[SiteSettingsCategoryDetailMediator alloc]
          initWithHostContentSettingsMap:settings_map_.get()
                           faviconLoader:favicon_loader_
                     contentSettingsType:ContentSettingsType::MEDIASTREAM_MIC];

  FakeSiteSettingsCategoryDetailConsumer* consumer =
      [[FakeSiteSettingsCategoryDetailConsumer alloc] init];
  mediator.consumer = consumer;

  ASSERT_EQ(1u, consumer.allowedSites.count);
  SiteSettingsSiteException* exception = consumer.allowedSites[0];

  [mediator deleteSettingForSite:exception];

  // Deleting the custom scope resets it to default, removing the exception.
  EXPECT_EQ(0u, consumer.allowedSites.count);

  [mediator disconnect];
}

// Tests that bulk-deleting multiple site exceptions clears all of them and
// reloads the consumer only once.
TEST_F(SiteSettingsCategoryDetailMediatorTest, TestDeleteSettingsForSites) {
  GURL allowedUrl("https://allowed.com");
  GURL blockedUrl("https://blocked.com");
  settings_map_->SetContentSettingDefaultScope(
      allowedUrl, allowedUrl, ContentSettingsType::MEDIASTREAM_MIC,
      CONTENT_SETTING_ALLOW);
  settings_map_->SetContentSettingDefaultScope(
      blockedUrl, blockedUrl, ContentSettingsType::MEDIASTREAM_MIC,
      CONTENT_SETTING_BLOCK);

  SiteSettingsCategoryDetailMediator* mediator =
      [[SiteSettingsCategoryDetailMediator alloc]
          initWithHostContentSettingsMap:settings_map_.get()
                           faviconLoader:favicon_loader_
                     contentSettingsType:ContentSettingsType::MEDIASTREAM_MIC];

  FakeSiteSettingsCategoryDetailConsumer* consumer =
      [[FakeSiteSettingsCategoryDetailConsumer alloc] init];
  mediator.consumer = consumer;

  ASSERT_EQ(1u, consumer.allowedSites.count);
  ASSERT_EQ(1u, consumer.notAllowedSites.count);
  NSUInteger initialCallCount = consumer.setAllowedSitesCallCount;

  [mediator deleteSettingsForSites:@[
    consumer.allowedSites[0], consumer.notAllowedSites[0]
  ]];

  EXPECT_EQ(0u, consumer.allowedSites.count);
  EXPECT_EQ(0u, consumer.notAllowedSites.count);
  EXPECT_EQ(initialCallCount + 1, consumer.setAllowedSitesCallCount);

  [mediator disconnect];
}

// Tests that updating a site exception setting moves it between allowed and not
// allowed lists.
TEST_F(SiteSettingsCategoryDetailMediatorTest, TestSetSettingForSite) {
  GURL siteUrl("https://example.com");
  settings_map_->SetContentSettingDefaultScope(
      siteUrl, siteUrl, ContentSettingsType::MEDIASTREAM_MIC,
      CONTENT_SETTING_ALLOW);

  SiteSettingsCategoryDetailMediator* mediator =
      [[SiteSettingsCategoryDetailMediator alloc]
          initWithHostContentSettingsMap:settings_map_.get()
                           faviconLoader:favicon_loader_
                     contentSettingsType:ContentSettingsType::MEDIASTREAM_MIC];

  FakeSiteSettingsCategoryDetailConsumer* consumer =
      [[FakeSiteSettingsCategoryDetailConsumer alloc] init];
  mediator.consumer = consumer;

  ASSERT_EQ(1u, consumer.allowedSites.count);
  ASSERT_EQ(0u, consumer.notAllowedSites.count);

  [mediator setSetting:CONTENT_SETTING_BLOCK forSite:consumer.allowedSites[0]];

  EXPECT_EQ(0u, consumer.allowedSites.count);
  ASSERT_EQ(1u, consumer.notAllowedSites.count);
  EXPECT_NSEQ(@"example.com", consumer.notAllowedSites[0].formattedTitle);

  [mediator setSetting:CONTENT_SETTING_ALLOW
               forSite:consumer.notAllowedSites[0]];

  ASSERT_EQ(1u, consumer.allowedSites.count);
  EXPECT_EQ(0u, consumer.notAllowedSites.count);
  EXPECT_NSEQ(@"example.com", consumer.allowedSites[0].formattedTitle);

  [mediator disconnect];
}

// Tests that disconnecting clears observer and consumer.
TEST_F(SiteSettingsCategoryDetailMediatorTest, TestDisconnect) {
  SiteSettingsCategoryDetailMediator* mediator =
      [[SiteSettingsCategoryDetailMediator alloc]
          initWithHostContentSettingsMap:settings_map_.get()
                           faviconLoader:favicon_loader_
                     contentSettingsType:ContentSettingsType::MEDIASTREAM_MIC];

  FakeSiteSettingsCategoryDetailConsumer* consumer =
      [[FakeSiteSettingsCategoryDetailConsumer alloc] init];
  mediator.consumer = consumer;
  [mediator disconnect];

  EXPECT_EQ(nil, mediator.consumer);

  // Changes after disconnect should not update consumer.
  settings_map_->SetDefaultContentSetting(ContentSettingsType::MEDIASTREAM_MIC,
                                          CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_ASK, consumer.defaultSetting);
}
