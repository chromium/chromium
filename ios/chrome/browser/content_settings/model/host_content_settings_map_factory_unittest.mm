// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_settings/model/host_content_settings_map_factory.h"

#import <memory>

#import "base/test/simple_test_clock.h"
#import "base/time/time.h"
#import "components/content_settings/core/browser/host_content_settings_map.h"
#import "components/content_settings/core/common/content_settings.h"
#import "components/content_settings/core/common/content_settings_types.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

namespace {

// URL used to create content setting exceptions in tests.
constexpr char kTestURL[] = "https://www.example.com";

// Arbitrary non-null time at which the content setting is modified.
constexpr base::TimeDelta kModifiedTimeSinceEpoch = base::Days(1);

}  // namespace

class HostContentSettingsMapFactoryTest : public PlatformTest {
 protected:
  HostContentSettingsMapFactoryTest()
      : profile_(TestProfileIOS::Builder().Build()) {}

  web::WebTaskEnvironment task_environment_;
  // Declared before `profile_` so that it outlives the
  // `HostContentSettingsMap` that references it.
  base::SimpleTestClock clock_;
  std::unique_ptr<TestProfileIOS> profile_;
};

// Tests that the `HostContentSettingsMap` records the last-modified time of
// content settings set by the user.
TEST_F(HostContentSettingsMapFactoryTest, StoresLastModified) {
  HostContentSettingsMap* map =
      ios::HostContentSettingsMapFactory::GetForProfile(profile_.get()).get();
  ASSERT_TRUE(map);

  clock_.SetNow(base::Time::UnixEpoch() + kModifiedTimeSinceEpoch);
  map->SetClockForTesting(&clock_);

  const GURL url(kTestURL);
  map->SetContentSettingDefaultScope(
      url, url, ContentSettingsType::MEDIASTREAM_CAMERA, CONTENT_SETTING_ALLOW);

  content_settings::SettingInfo info;
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            map->GetContentSetting(
                url, url, ContentSettingsType::MEDIASTREAM_CAMERA, &info));
  EXPECT_EQ(clock_.Now(), info.metadata.last_modified());
}
