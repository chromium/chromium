// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/browsing_data/model/site_settings_counter.h"

#import <memory>
#import <optional>

#import "base/functional/bind.h"
#import "base/json/values_util.h"
#import "base/test/simple_test_clock.h"
#import "base/time/time.h"
#import "components/content_settings/core/browser/host_content_settings_map.h"
#import "components/content_settings/core/common/content_settings.h"
#import "components/content_settings/core/common/content_settings_types.h"
#import "components/prefs/scoped_user_pref_update.h"
#import "components/translate/core/browser/translate_pref_names.h"
#import "ios/chrome/browser/content_settings/model/host_content_settings_map_factory.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

namespace {

// URLs used to create site settings exceptions.
const char kFirstSiteURL[] = "https://first.example.com";
const char kSecondSiteURL[] = "https://second.example.com";

// Hosts of `kFirstSiteURL` and of a site only on the "never translate" list.
const char kFirstSiteHost[] = "first.example.com";
const char kNeverTranslateOnlyHost[] = "translate.example.com";

// Age of a site settings exception that falls outside of the counted period.
constexpr base::TimeDelta kOldSiteSettingAge = base::Days(2);

// Start of the counted period, relative to now.
constexpr base::TimeDelta kCountedPeriod = base::Hours(1);

}  // namespace

class SiteSettingsCounterTest : public PlatformTest {
 protected:
  SiteSettingsCounterTest()
      : profile_(TestProfileIOS::Builder().Build()),
        map_(
            ios::HostContentSettingsMapFactory::GetForProfile(profile_.get())) {
    map_->SetClockForTesting(&clock_);
  }

  // Sets `type` for `url` to `setting`, recorded as modified at
  // `modified_time`.
  void SetSetting(const GURL& url,
                  ContentSettingsType type,
                  ContentSetting setting,
                  base::Time modified_time) {
    clock_.SetNow(modified_time);
    map_->SetContentSettingDefaultScope(url, url, type, setting);
  }

  // Adds `site` to the "never translate" list, recorded as added at
  // `added_time`. Writes the pref directly because
  // `TranslatePrefs::AddSiteToNeverPromptList()` always uses the current time.
  void AddNeverTranslateSite(std::string_view site, base::Time added_time) {
    ScopedDictPrefUpdate update(
        profile_->GetPrefs(), translate::prefs::kPrefNeverPromptSitesWithTime);
    update->Set(site, base::TimeToValue(added_time));
  }

  // Returns the number of sites counted since `begin_time`.
  browsing_data::BrowsingDataCounter::ResultInt CountSince(
      base::Time begin_time) {
    std::optional<browsing_data::BrowsingDataCounter::ResultInt> count;
    SiteSettingsCounter counter(map_, profile_->GetPrefs());
    counter.InitWithoutPref(
        begin_time,
        base::BindRepeating(
            [](std::optional<browsing_data::BrowsingDataCounter::ResultInt>*
                   count,
               std::unique_ptr<browsing_data::BrowsingDataCounter::Result>
                   result) {
              ASSERT_TRUE(result->Finished());
              *count = static_cast<
                           browsing_data::BrowsingDataCounter::FinishedResult*>(
                           result.get())
                           ->Value();
            },
            &count));
    counter.Restart();
    EXPECT_TRUE(count.has_value());
    return count.value_or(-1);
  }

  web::WebTaskEnvironment task_environment_;
  // Declared before `profile_` so that it outlives the
  // `HostContentSettingsMap` that references it.
  base::SimpleTestClock clock_;
  std::unique_ptr<TestProfileIOS> profile_;
  scoped_refptr<HostContentSettingsMap> map_;
};

// Tests that no sites are counted when there are no exceptions.
TEST_F(SiteSettingsCounterTest, CountsNothingWithoutExceptions) {
  EXPECT_EQ(0, CountSince(base::Time()));
}

// Tests that a site with several exceptions is counted once.
TEST_F(SiteSettingsCounterTest, CountsSitesOnce) {
  const base::Time now = base::Time::Now();
  const GURL first_url(kFirstSiteURL);
  SetSetting(first_url, ContentSettingsType::MEDIASTREAM_CAMERA,
             CONTENT_SETTING_ALLOW, now);
  SetSetting(first_url, ContentSettingsType::MEDIASTREAM_MIC,
             CONTENT_SETTING_BLOCK, now);
  SetSetting(GURL(kSecondSiteURL), ContentSettingsType::MEDIASTREAM_CAMERA,
             CONTENT_SETTING_BLOCK, now);

  EXPECT_EQ(2, CountSince(base::Time()));
}

// Tests that only exceptions modified within the period are counted.
TEST_F(SiteSettingsCounterTest, CountsOnlyExceptionsInPeriod) {
  const base::Time now = base::Time::Now();
  SetSetting(GURL(kFirstSiteURL), ContentSettingsType::MEDIASTREAM_CAMERA,
             CONTENT_SETTING_ALLOW, now);
  SetSetting(GURL(kSecondSiteURL), ContentSettingsType::MEDIASTREAM_CAMERA,
             CONTENT_SETTING_BLOCK, now - kOldSiteSettingAge);

  EXPECT_EQ(1, CountSince(now - kCountedPeriod));
  EXPECT_EQ(2, CountSince(base::Time()));
}

// Tests that "never translate" sites are counted, and that a site with both a
// permission exception and a "never translate" entry is counted once.
TEST_F(SiteSettingsCounterTest, CountsNeverTranslateSites) {
  const base::Time now = base::Time::Now();
  SetSetting(GURL(kFirstSiteURL), ContentSettingsType::MEDIASTREAM_CAMERA,
             CONTENT_SETTING_ALLOW, now);
  AddNeverTranslateSite(kFirstSiteHost, now);
  AddNeverTranslateSite(kNeverTranslateOnlyHost, now);

  EXPECT_EQ(2, CountSince(base::Time()));
}

// Tests that only "never translate" sites added within the period are counted.
TEST_F(SiteSettingsCounterTest, CountsOnlyNeverTranslateSitesInPeriod) {
  const base::Time now = base::Time::Now();
  AddNeverTranslateSite(kFirstSiteHost, now);
  AddNeverTranslateSite(kNeverTranslateOnlyHost, now - kOldSiteSettingAge);

  EXPECT_EQ(1, CountSince(now - kCountedPeriod));
  EXPECT_EQ(2, CountSince(base::Time()));
}
