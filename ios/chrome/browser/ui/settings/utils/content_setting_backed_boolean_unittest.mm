// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/utils/content_setting_backed_boolean.h"

#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/content_settings/host_content_settings_map_factory.h"
#import "ios/chrome/browser/ui/settings/utils/fake_observable_boolean.h"
#include "ios/web/public/test/web_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

const ContentSettingsType kTestContentSettingID = ContentSettingsType::POPUPS;

class ContentSettingBackedBooleanTest : public PlatformTest {
 public:
  void SetUp() override {
    TestChromeBrowserState::Builder test_cbs_builder;
    chrome_browser_state_ = test_cbs_builder.Build();
    observable_boolean_ = [[ContentSettingBackedBoolean alloc]
        initWithHostContentSettingsMap:SettingsMap()
                             settingID:kTestContentSettingID
                              inverted:NO];
  }

 protected:
  bool GetSetting() {
    ContentSetting setting =
        SettingsMap()->GetDefaultContentSetting(kTestContentSettingID, NULL);
    return setting == CONTENT_SETTING_ALLOW;
  }

  void SetSetting(bool booleanValue) {
    ContentSetting value =
        booleanValue ? CONTENT_SETTING_ALLOW : CONTENT_SETTING_BLOCK;
    SettingsMap()->SetDefaultContentSetting(kTestContentSettingID, value);
  }

  HostContentSettingsMap* SettingsMap() {
    return ios::HostContentSettingsMapFactory::GetForBrowserState(
        chrome_browser_state_.get());
  }

  sync_preferences::TestingPrefServiceSyncable* PrefService() {
    return chrome_browser_state_->GetTestingPrefService();
  }

  ContentSettingBackedBoolean* GetObservableBoolean() {
    return observable_boolean_;
  }

  void SetUpInvertedContentSettingBackedBoolean() {
    observable_boolean_ = [[ContentSettingBackedBoolean alloc]
        initWithHostContentSettingsMap:SettingsMap()
                             settingID:kTestContentSettingID
                              inverted:YES];
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  ContentSettingBackedBoolean* observable_boolean_;
};

TEST_F(ContentSettingBackedBooleanTest, ReadFromSettings) {
  SetSetting(false);
  EXPECT_FALSE(GetObservableBoolean().value);

  SetSetting(true);
  EXPECT_TRUE(GetObservableBoolean().value);
}

TEST_F(ContentSettingBackedBooleanTest, WriteToSettings) {
  GetObservableBoolean().value = YES;
  EXPECT_TRUE(GetSetting());

  GetObservableBoolean().value = NO;
  EXPECT_FALSE(GetSetting());
}

TEST_F(ContentSettingBackedBooleanTest, InvertedReadFromSettings) {
  SetUpInvertedContentSettingBackedBoolean();
  SetSetting(false);
  EXPECT_TRUE(GetObservableBoolean().value);

  SetSetting(true);
  EXPECT_FALSE(GetObservableBoolean().value);
}

TEST_F(ContentSettingBackedBooleanTest, InvertedWriteToSettings) {
  SetUpInvertedContentSettingBackedBoolean();
  GetObservableBoolean().value = YES;
  EXPECT_FALSE(GetSetting());

  GetObservableBoolean().value = NO;
  EXPECT_TRUE(GetSetting());
}

TEST_F(ContentSettingBackedBooleanTest, ObserverUpdates) {
  SetSetting(false);
  TestBooleanObserver* observer = [[TestBooleanObserver alloc] init];
  GetObservableBoolean().observer = observer;
  EXPECT_EQ(0, observer.updateCount);

  SetSetting(true);
  EXPECT_EQ(1, observer.updateCount) << "Changing value should update observer";

  SetSetting(true);
  EXPECT_EQ(2, observer.updateCount) << "ContentSettingBackedBoolean "
                                        "should update observer even "
                                        "when resetting the same value";
}

}  // namespace
