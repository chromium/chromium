// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/clear_browsing_data/coordinator/quick_delete_util.h"

#import "base/apple/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/browsing_data/core/browsing_data_utils.h"
#import "components/browsing_data/core/pref_names.h"
#import "components/search_engines/search_engines_test_environment.h"
#import "components/search_engines/template_url.h"
#import "components/strings/grit/components_strings.h"
#import "components/sync_preferences/pref_service_syncable.h"
#import "ios/chrome/browser/browsing_data/model/cache_counter.h"
#import "ios/chrome/browser/browsing_data/model/tabs_counter.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/sessions/model/session_restoration_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"

class QuickDeleteUtilTest : public PlatformTest {
 public:
  QuickDeleteUtilTest() {
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        ios::TemplateURLServiceFactory::GetInstance(),
        ios::TemplateURLServiceFactory::GetDefaultFactory());

    profile_ = std::move(builder).Build();
    template_url_service_ =
        search_engines_test_environment_.template_url_service();
  }

  // Sets the default search engine to not be Google.
  void SetDseToNonGoogle() {
    TemplateURLData non_google_provider_data;
    non_google_provider_data.SetURL(
        "https://www.nongoogle.com/?q={searchTerms}");
    non_google_provider_data.suggestions_url =
        "https://www.nongoogle.com/suggest/?q={searchTerms}";

    auto* non_google_provider = template_url_service_->Add(
        std::make_unique<TemplateURL>(non_google_provider_data));
    template_url_service_->SetUserSelectedDefaultSearchProvider(
        non_google_provider);
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  search_engines::SearchEnginesTestEnvironment search_engines_test_environment_;
  raw_ptr<TemplateURLService> template_url_service_;
};

// Tests the construction of the counter string for cache in for the all time
// period range.
TEST_F(QuickDeleteUtilTest, TestCacheCounterFormattingForAllTime) {
  PrefService* prefs = profile_->GetPrefs();
  prefs->SetInteger(browsing_data::prefs::kDeleteTimePeriod,
                    static_cast<int>(browsing_data::TimePeriod::ALL_TIME));
  CacheCounter counter(profile_.get());

  NSByteCountFormatter* formatter = [[NSByteCountFormatter alloc] init];
  formatter.allowedUnits = NSByteCountFormatterUseAll &
                           (~NSByteCountFormatterUseBytes) &
                           (~NSByteCountFormatterUseKB);
  formatter.countStyle = NSByteCountFormatterCountStyleMemory;

  NSString* almost_empty =
      l10n_util::GetNSString(IDS_DEL_CACHE_COUNTER_ALMOST_EMPTY);
  NSString* format_1_mb = [formatter stringFromByteCount:(1 << 20)];
  NSString* format_1_5_mb =
      [formatter stringFromByteCount:(1 << 20) + (1 << 19)];
  NSString* format_2_mb = [formatter stringFromByteCount:(1 << 21)];
  NSString* format_1_gb = [formatter stringFromByteCount:(1 << 30)];

  // Test multiple possible types of formatting.
  // clang-format off
    const struct TestCase {
        int cache_size;
        NSString* expected_output;
    } kTestCases[] = {
        {0, almost_empty},
        {(1 << 20) - 1, almost_empty},
        {(1 << 20), format_1_mb},
        {(1 << 20) + (1 << 19), format_1_5_mb},
        {(1 << 21) - 10, format_2_mb},
        {(1 << 21), format_2_mb},
        {(1 << 30), format_1_gb}
    };
  // clang-format on

  for (const TestCase& test_case : kTestCases) {
    browsing_data::BrowsingDataCounter::FinishedResult result(
        &counter, test_case.cache_size);
    NSString* output = quick_delete_util::GetCounterTextFromResult(
        result, browsing_data::TimePeriod::ALL_TIME);
    EXPECT_NSEQ(test_case.expected_output, output);
  }
}

// Tests the construction of the counter string for cache in less than all time
// time period range.
TEST_F(QuickDeleteUtilTest, TestCacheCounterFormattingForLessThanAllTime) {
  PrefService* prefs = profile_->GetPrefs();
  prefs->SetInteger(browsing_data::prefs::kDeleteTimePeriod,
                    static_cast<int>(browsing_data::TimePeriod::LAST_HOUR));
  CacheCounter counter(profile_.get());

  NSByteCountFormatter* formatter = [[NSByteCountFormatter alloc] init];
  formatter.allowedUnits = NSByteCountFormatterUseAll &
                           (~NSByteCountFormatterUseBytes) &
                           (~NSByteCountFormatterUseKB);
  formatter.countStyle = NSByteCountFormatterCountStyleMemory;

  NSString* almost_empty =
      l10n_util::GetNSString(IDS_DEL_CACHE_COUNTER_ALMOST_EMPTY);
  NSString* format_1_mb = [formatter stringFromByteCount:(1 << 20)];
  NSString* less_than_1_mb =
      l10n_util::GetNSStringF(IDS_DEL_CACHE_COUNTER_UPPER_ESTIMATE,
                              base::SysNSStringToUTF16(format_1_mb));
  NSString* format_1_5_mb =
      [formatter stringFromByteCount:(1 << 20) + (1 << 19)];
  NSString* less_than_1_5_mb =
      l10n_util::GetNSStringF(IDS_DEL_CACHE_COUNTER_UPPER_ESTIMATE,
                              base::SysNSStringToUTF16(format_1_5_mb));
  NSString* format_2_mb = [formatter stringFromByteCount:(1 << 21)];
  NSString* less_than_2_mb =
      l10n_util::GetNSStringF(IDS_DEL_CACHE_COUNTER_UPPER_ESTIMATE,
                              base::SysNSStringToUTF16(format_2_mb));
  NSString* format_1_gb = [formatter stringFromByteCount:(1 << 30)];
  NSString* less_than_1_gb =
      l10n_util::GetNSStringF(IDS_DEL_CACHE_COUNTER_UPPER_ESTIMATE,
                              base::SysNSStringToUTF16(format_1_gb));

  // Test multiple possible types of formatting.
  // clang-format off
    const struct TestCase {
        int cache_size;
        NSString* expected_output;
    } kTestCases[] = {
        {0, almost_empty},
        {(1 << 20) - 1, almost_empty},
        {(1 << 20), less_than_1_mb},
        {(1 << 20) + (1 << 19), less_than_1_5_mb},
        {(1 << 21) - 10, less_than_2_mb},
        {(1 << 21), less_than_2_mb},
        {(1 << 30), less_than_1_gb}
    };
  // clang-format on

  for (const TestCase& test_case : kTestCases) {
    browsing_data::BrowsingDataCounter::FinishedResult result(
        &counter, test_case.cache_size);
    NSString* output = quick_delete_util::GetCounterTextFromResult(
        result, browsing_data::TimePeriod::LAST_HOUR);
    EXPECT_NSEQ(test_case.expected_output, output);
  }
}

// Tests the construction of the counter string for tabs in single window and
// multiwindow formats.
TEST_F(QuickDeleteUtilTest, TestTabsCounter) {
  PrefService* prefs = profile_->GetPrefs();
  prefs->SetInteger(browsing_data::prefs::kDeleteTimePeriod,
                    static_cast<int>(browsing_data::TimePeriod::LAST_HOUR));
  std::u16string two_tabs = l10n_util::GetPluralStringFUTF16(IDS_TABS_COUNT, 2);
  std::u16string two_windows =
      l10n_util::GetPluralStringFUTF16(IDS_WINDOWS_COUNT, 2);

  const struct TestCase {
    int num_tabs;
    int num_windows;
    NSString* expected_output;
  } kTestCases[] = {
      {0, 1, l10n_util::GetPluralNSStringF(IDS_DEL_TABS_COUNTER, 0)},
      {0, 2, l10n_util::GetPluralNSStringF(IDS_DEL_TABS_COUNTER, 0)},
      {1, 1, l10n_util::GetPluralNSStringF(IDS_DEL_TABS_COUNTER, 1)},
      {2, 1, l10n_util::GetPluralNSStringF(IDS_DEL_TABS_COUNTER, 2)},
      {2, 2,
       l10n_util::GetNSStringF(IDS_DEL_TABS_MULTIWINDOW_COUNTER, two_tabs,
                               two_windows)},
  };

  TabsCounter counter(
      BrowserListFactory::GetForProfile(profile_.get()),
      SessionRestorationServiceFactory::GetForProfile(profile_.get()));

  for (const TestCase& test_case : kTestCases) {
    const TabsCounter::TabsResult result(&counter, test_case.num_tabs,
                                         test_case.num_windows, {});
    NSString* output = quick_delete_util::GetCounterTextFromResult(
        result, browsing_data::TimePeriod::LAST_HOUR);
    EXPECT_NSEQ(test_case.expected_output, output);
  }
}

// Tests that `GetDefaultSearchEngineState` returns the correct
// DefaultSearchEngineState when the DSE is Google.
TEST_F(QuickDeleteUtilTest, TestDseStateWhenDseIsGoogle) {
  EXPECT_EQ(
      quick_delete_util::GetDefaultSearchEngineState(template_url_service_),
      quick_delete_util::DefaultSearchEngineState::kGoogle);
}

// Tests that `GetDefaultSearchEngineState` returns the correct
// DefaultSearchEngineState when the DSE is not Google.
TEST_F(QuickDeleteUtilTest, TestDseStateWhenDseIsNotGoogle) {
  SetDseToNonGoogle();
  EXPECT_EQ(
      quick_delete_util::GetDefaultSearchEngineState(template_url_service_),
      quick_delete_util::DefaultSearchEngineState::kNotGoogle);
}

// Tests that `GetDefaultSearchEngineState` returns the correct
// DefaultSearchEngineState when the DSE is null.
TEST_F(QuickDeleteUtilTest, TestDseStateWhenDseIsNull) {
  EXPECT_EQ(quick_delete_util::GetDefaultSearchEngineState(
                /*template_url_service=*/nullptr),
            quick_delete_util::DefaultSearchEngineState::kError);
}
