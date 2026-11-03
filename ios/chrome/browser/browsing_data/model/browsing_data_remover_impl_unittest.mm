// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/browsing_data/model/browsing_data_remover_impl.h"

#import <memory>

#import "base/check_op.h"
#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/json/values_util.h"
#import "base/run_loop.h"
#import "base/scoped_observation.h"
#import "base/test/ios/wait_util.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/simple_test_clock.h"
#import "components/content_settings/core/browser/host_content_settings_map.h"
#import "components/content_settings/core/common/content_settings.h"
#import "components/content_settings/core/common/content_settings_types.h"
#import "components/open_from_clipboard/clipboard_recent_content.h"
#import "components/open_from_clipboard/fake_clipboard_recent_content.h"
#import "components/prefs/scoped_user_pref_update.h"
#import "components/translate/core/browser/translate_pref_names.h"
#import "components/translate/core/browser/translate_prefs.h"
#import "ios/chrome/browser/browsing_data/model/browsing_data_remover_observer.h"
#import "ios/chrome/browser/content_settings/model/host_content_settings_map_factory.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/translate/model/chrome_ios_translate_client.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

using base::test::ios::kWaitForActionTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

namespace {

// Flags passed when calling Remove(). Clear as much data as possible, avoiding
// using services that are not created for TestProfileIOS.
constexpr BrowsingDataRemoveMask kRemoveMask =
    BrowsingDataRemoveMask::REMOVE_CACHE |
    BrowsingDataRemoveMask::REMOVE_COOKIES |
    BrowsingDataRemoveMask::REMOVE_FORM_DATA |
    BrowsingDataRemoveMask::REMOVE_HISTORY |
    BrowsingDataRemoveMask::REMOVE_INDEXEDDB |
    BrowsingDataRemoveMask::REMOVE_LOCAL_STORAGE |
    BrowsingDataRemoveMask::REMOVE_PASSWORDS |
    BrowsingDataRemoveMask::REMOVE_WEBSQL |
    BrowsingDataRemoveMask::REMOVE_CACHE_STORAGE |
    BrowsingDataRemoveMask::REMOVE_VISITED_LINKS |
    BrowsingDataRemoveMask::REMOVE_LAST_USER_ACCOUNT |
    BrowsingDataRemoveMask::REMOVE_ORIGIN_PRIVATE_FILE_SYSTEM |
    BrowsingDataRemoveMask::REMOVE_SERVICE_WORKERS;

const char kFullDeletionHistogram[] =
    "History.ClearBrowsingData.Duration.FullDeletion";
const char kTimeRangeDeletionHistogram[] =
    "History.ClearBrowsingData.Duration.TimeRangeDeletion";

// URLs used to create site settings exceptions.
const char kRecentSiteURL[] = "https://recent.example.com";
const char kOldSiteURL[] = "https://old.example.com";

// Age of a site settings exception that falls outside of the last hour.
constexpr base::TimeDelta kOldSiteSettingAge = base::Days(2);

// Hosts used for "never translate" entries.
const char kRecentNeverTranslateSite[] = "recent.example.com";
const char kOldNeverTranslateSite[] = "old.example.com";

// Observer used to validate that BrowsingDataRemoverImpl notifies its
// observers.
class TestBrowsingDataRemoverObserver : public BrowsingDataRemoverObserver {
 public:
  TestBrowsingDataRemoverObserver() = default;

  TestBrowsingDataRemoverObserver(const TestBrowsingDataRemoverObserver&) =
      delete;
  TestBrowsingDataRemoverObserver& operator=(
      const TestBrowsingDataRemoverObserver&) = delete;

  ~TestBrowsingDataRemoverObserver() override = default;

  // BrowsingDataRemoverObserver implementation.
  void OnBrowsingDataRemoved(BrowsingDataRemover* remover,
                             BrowsingDataRemoveMask mask) override;

  // Returns the `mask` value passed to the last call of OnBrowsingDataRemoved.
  // Returns BrowsingDataRemoveMask::REMOVE_NOTHING if it has not been called.
  BrowsingDataRemoveMask last_remove_mask() const { return last_remove_mask_; }

 private:
  BrowsingDataRemoveMask last_remove_mask_ =
      BrowsingDataRemoveMask::REMOVE_NOTHING;
};

void TestBrowsingDataRemoverObserver::OnBrowsingDataRemoved(
    BrowsingDataRemover* remover,
    BrowsingDataRemoveMask mask) {
  DCHECK(mask != BrowsingDataRemoveMask::REMOVE_NOTHING);
  last_remove_mask_ = mask;
}

}  // namespace

class BrowsingDataRemoverImplTest : public PlatformTest {
 public:
  BrowsingDataRemoverImplTest()
      : profile_(TestProfileIOS::Builder().Build()),
        browsing_data_remover_(profile_.get()) {
    GetHostContentSettingsMap()->SetClockForTesting(&clock_);
    DCHECK_EQ(ClipboardRecentContent::GetInstance(), nullptr);
    ClipboardRecentContent::SetInstance(
        std::make_unique<FakeClipboardRecentContent>());
  }

  BrowsingDataRemoverImplTest(const BrowsingDataRemoverImplTest&) = delete;
  BrowsingDataRemoverImplTest& operator=(const BrowsingDataRemoverImplTest&) =
      delete;

  ~BrowsingDataRemoverImplTest() override {
    DCHECK_NE(ClipboardRecentContent::GetInstance(), nullptr);
    ClipboardRecentContent::SetInstance(nullptr);
    browsing_data_remover_.Shutdown();
  }

 protected:
  // Returns the `HostContentSettingsMap` of `profile_`.
  HostContentSettingsMap* GetHostContentSettingsMap() {
    return ios::HostContentSettingsMapFactory::GetForProfile(profile_.get())
        .get();
  }

  // Sets the camera permission for `url` to `setting`, recorded as modified at
  // `modified_time`.
  void SetCameraSetting(const GURL& url,
                        ContentSetting setting,
                        base::Time modified_time) {
    clock_.SetNow(modified_time);
    GetHostContentSettingsMap()->SetContentSettingDefaultScope(
        url, url, ContentSettingsType::MEDIASTREAM_CAMERA, setting);
  }

  // Returns the camera permission for `url`.
  ContentSetting GetCameraSetting(const GURL& url) {
    return GetHostContentSettingsMap()->GetContentSetting(
        url, url, ContentSettingsType::MEDIASTREAM_CAMERA);
  }

  // Returns the `TranslatePrefs` of `profile_`.
  std::unique_ptr<translate::TranslatePrefs> GetTranslatePrefs() {
    return ChromeIOSTranslateClient::CreateTranslatePrefs(profile_->GetPrefs());
  }

  // Adds `site` to the "never translate" list, recorded as added at
  // `added_time`. Writes the pref directly because
  // `TranslatePrefs::AddSiteToNeverPromptList()` always uses the current time.
  void AddNeverTranslateSite(std::string_view site, base::Time added_time) {
    ScopedDictPrefUpdate update(
        profile_->GetPrefs(), translate::prefs::kPrefNeverPromptSitesWithTime);
    update->Set(site, base::TimeToValue(added_time));
  }

  // Removes `mask` for `time_period` and waits for the removal to complete.
  void RemoveAndWait(browsing_data::TimePeriod time_period,
                     BrowsingDataRemoveMask mask) {
    base::RunLoop run_loop;
    browsing_data_remover_.Remove(time_period, mask, run_loop.QuitClosure());
    run_loop.Run();
  }

  web::WebTaskEnvironment task_environment_;
  // Declared before `profile_` so that it outlives the
  // `HostContentSettingsMap` that references it.
  base::SimpleTestClock clock_;
  std::unique_ptr<ProfileIOS> profile_;
  BrowsingDataRemoverImpl browsing_data_remover_;
};

// Tests that BrowsingDataRemoverImpl::Remove() invokes the observers.
TEST_F(BrowsingDataRemoverImplTest, InvokesObservers) {
  TestBrowsingDataRemoverObserver observer;
  ASSERT_TRUE(observer.last_remove_mask() != kRemoveMask);

  base::ScopedObservation<BrowsingDataRemover, BrowsingDataRemoverObserver>
      scoped_observer(&observer);
  scoped_observer.Observe(&browsing_data_remover_);

  browsing_data_remover_.Remove(browsing_data::TimePeriod::ALL_TIME,
                                kRemoveMask, base::DoNothing());

  TestBrowsingDataRemoverObserver* observer_ptr = &observer;
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^{
    // Spin the RunLoop as WaitUntilConditionOrTimeout doesn't.
    base::RunLoop().RunUntilIdle();
    return observer_ptr->last_remove_mask() == kRemoveMask;
  }));
}

// Tests that BrowsingDataRemoverImpl::Remove() can be called multiple times.
TEST_F(BrowsingDataRemoverImplTest, SerializeRemovals) {
  __block int remaining_calls = 2;
  browsing_data_remover_.Remove(browsing_data::TimePeriod::ALL_TIME,
                                kRemoveMask, base::BindOnce(^{
                                  --remaining_calls;
                                }));
  browsing_data_remover_.Remove(browsing_data::TimePeriod::ALL_TIME,
                                kRemoveMask, base::BindOnce(^{
                                  --remaining_calls;
                                }));

  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^{
    // Spin the RunLoop as WaitUntilConditionOrTimeout doesn't.
    base::RunLoop().RunUntilIdle();
    return remaining_calls == 0;
  }));
}

// Tests that BrowsingDataRemoverImpl::Remove() Logs the duration to the correct
// histogram for full deletion.
TEST_F(BrowsingDataRemoverImplTest, LogDurationForFullDeletion) {
  base::HistogramTester histogram_tester;
  __block int remaining_calls = 1;
  histogram_tester.ExpectTotalCount(kFullDeletionHistogram, 0);
  histogram_tester.ExpectTotalCount(kTimeRangeDeletionHistogram, 0);

  browsing_data_remover_.Remove(browsing_data::TimePeriod::ALL_TIME,
                                kRemoveMask, base::BindOnce(^{
                                  --remaining_calls;
                                }));
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^{
    // Spin the RunLoop as WaitUntilConditionOrTimeout doesn't.
    base::RunLoop().RunUntilIdle();
    return remaining_calls == 0;
  }));

  histogram_tester.ExpectTotalCount(kFullDeletionHistogram, 1);
  histogram_tester.ExpectTotalCount(kTimeRangeDeletionHistogram, 0);
}

// Tests that BrowsingDataRemoverImpl::Remove() Logs the duration to the correct
// histogram for partial deletion.
TEST_F(BrowsingDataRemoverImplTest, LogDurationForPartialDeletion) {
  base::HistogramTester histogram_tester;
  __block int remaining_calls = 1;
  histogram_tester.ExpectTotalCount(kFullDeletionHistogram, 0);
  histogram_tester.ExpectTotalCount(kTimeRangeDeletionHistogram, 0);

  browsing_data_remover_.Remove(browsing_data::TimePeriod::LAST_HOUR,
                                kRemoveMask, base::BindOnce(^{
                                  --remaining_calls;
                                }));
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^{
    // Spin the RunLoop as WaitUntilConditionOrTimeout doesn't.
    base::RunLoop().RunUntilIdle();
    return remaining_calls == 0;
  }));

  histogram_tester.ExpectTotalCount(kFullDeletionHistogram, 0);
  histogram_tester.ExpectTotalCount(kTimeRangeDeletionHistogram, 1);
}

// Tests that BrowsingDataRemoverImpl::Remove() can finish performing its
// operation even if the BrowserState is destroyed.
TEST_F(BrowsingDataRemoverImplTest, PerformAfterBrowserStateDestruction) {
  __block int remaining_calls = 1;
  browsing_data_remover_.Remove(browsing_data::TimePeriod::ALL_TIME,
                                kRemoveMask, base::BindOnce(^{
                                  --remaining_calls;
                                }));

  // Simulate destruction of BrowserState.
  browsing_data_remover_.Shutdown();

  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^{
    // Spin the RunLoop as WaitUntilConditionOrTimeout doesn't.
    base::RunLoop().RunUntilIdle();
    return remaining_calls == 0;
  }));
}

// Tests that BrowsingDataRemoverImpl::RemoveInRange() invokes the observers.
TEST_F(BrowsingDataRemoverImplTest, InvokesObservers_RemoveInRange) {
  TestBrowsingDataRemoverObserver observer;
  ASSERT_TRUE(observer.last_remove_mask() != kRemoveMask);

  base::ScopedObservation<BrowsingDataRemover, BrowsingDataRemoverObserver>
      scoped_observer(&observer);
  scoped_observer.Observe(&browsing_data_remover_);

  base::Time delete_start_time = base::Time::Now() - base::Hours(1);
  base::Time delete_end_time = base::Time::Now();
  browsing_data_remover_.RemoveInRange(delete_start_time, delete_end_time,
                                       kRemoveMask, base::DoNothing());

  TestBrowsingDataRemoverObserver* observer_ptr = &observer;
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^{
    // Spin the RunLoop as WaitUntilConditionOrTimeout doesn't.
    base::RunLoop().RunUntilIdle();
    return observer_ptr->last_remove_mask() == kRemoveMask;
  }));
}

// Tests that BrowsingDataRemoverImpl::RemoveInRange() can be called multiple
// times.
TEST_F(BrowsingDataRemoverImplTest, SerializeRemovals_RemoveInRange) {
  __block int remaining_calls = 2;
  base::Time delete_start_time = base::Time::Now() - base::Hours(1);
  base::Time delete_end_time = base::Time::Now();
  browsing_data_remover_.RemoveInRange(delete_start_time, delete_end_time,
                                       kRemoveMask, base::BindOnce(^{
                                         --remaining_calls;
                                       }));

  browsing_data_remover_.RemoveInRange(delete_start_time, delete_end_time,
                                       kRemoveMask, base::BindOnce(^{
                                         --remaining_calls;
                                       }));

  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^{
    // Spin the RunLoop as WaitUntilConditionOrTimeout doesn't.
    base::RunLoop().RunUntilIdle();
    return remaining_calls == 0;
  }));
}

// Tests that BrowsingDataRemoverImpl::RemoveInRange() can finish performing its
// operation even if the BrowserState is destroyed.
TEST_F(BrowsingDataRemoverImplTest,
       PerformAfterBrowserStateDestruction_RemoveInRange) {
  __block int remaining_calls = 1;
  base::Time delete_start_time = base::Time::Now() - base::Hours(1);
  base::Time delete_end_time = base::Time::Now();
  browsing_data_remover_.RemoveInRange(delete_start_time, delete_end_time,
                                       kRemoveMask, base::BindOnce(^{
                                         --remaining_calls;
                                       }));

  // Simulate destruction of BrowserState.
  browsing_data_remover_.Shutdown();

  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^{
    // Spin the RunLoop as WaitUntilConditionOrTimeout doesn't.
    base::RunLoop().RunUntilIdle();
    return remaining_calls == 0;
  }));
}

// Tests that site settings are removed for all time when requested.
TEST_F(BrowsingDataRemoverImplTest, RemovesSiteSettings) {
  const GURL recent_url(kRecentSiteURL);
  const GURL old_url(kOldSiteURL);
  const base::Time now = base::Time::Now();
  SetCameraSetting(recent_url, CONTENT_SETTING_ALLOW, now);
  SetCameraSetting(old_url, CONTENT_SETTING_BLOCK, now - kOldSiteSettingAge);

  RemoveAndWait(browsing_data::TimePeriod::ALL_TIME,
                BrowsingDataRemoveMask::REMOVE_SITE_SETTINGS);

  EXPECT_EQ(CONTENT_SETTING_ASK, GetCameraSetting(recent_url));
  EXPECT_EQ(CONTENT_SETTING_ASK, GetCameraSetting(old_url));
}

// Tests that only site settings modified within the deletion period are
// removed.
TEST_F(BrowsingDataRemoverImplTest, RemovesSiteSettingsInTimePeriod) {
  const GURL recent_url(kRecentSiteURL);
  const GURL old_url(kOldSiteURL);
  const base::Time now = base::Time::Now();
  SetCameraSetting(recent_url, CONTENT_SETTING_ALLOW, now);
  SetCameraSetting(old_url, CONTENT_SETTING_BLOCK, now - kOldSiteSettingAge);

  RemoveAndWait(browsing_data::TimePeriod::LAST_HOUR,
                BrowsingDataRemoveMask::REMOVE_SITE_SETTINGS);

  EXPECT_EQ(CONTENT_SETTING_ASK, GetCameraSetting(recent_url));
  EXPECT_EQ(CONTENT_SETTING_BLOCK, GetCameraSetting(old_url));
}

// Tests that site settings are kept when they are not part of the removal mask.
TEST_F(BrowsingDataRemoverImplTest, KeepsSiteSettingsWhenNotRequested) {
  const GURL url(kRecentSiteURL);
  SetCameraSetting(url, CONTENT_SETTING_ALLOW, base::Time::Now());

  RemoveAndWait(browsing_data::TimePeriod::ALL_TIME, kRemoveMask);

  EXPECT_EQ(CONTENT_SETTING_ALLOW, GetCameraSetting(url));
}

// Tests that "never translate" sites added within the deletion period are
// removed with site settings.
TEST_F(BrowsingDataRemoverImplTest, RemovesNeverTranslateSitesInTimePeriod) {
  const base::Time now = base::Time::Now();
  AddNeverTranslateSite(kRecentNeverTranslateSite, now);
  AddNeverTranslateSite(kOldNeverTranslateSite, now - kOldSiteSettingAge);

  RemoveAndWait(browsing_data::TimePeriod::LAST_HOUR,
                BrowsingDataRemoveMask::REMOVE_SITE_SETTINGS);

  std::unique_ptr<translate::TranslatePrefs> translate_prefs =
      GetTranslatePrefs();
  EXPECT_FALSE(
      translate_prefs->IsSiteOnNeverPromptList(kRecentNeverTranslateSite));
  EXPECT_TRUE(translate_prefs->IsSiteOnNeverPromptList(kOldNeverTranslateSite));
}
