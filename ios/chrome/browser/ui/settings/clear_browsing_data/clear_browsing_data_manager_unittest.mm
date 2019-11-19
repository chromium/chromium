// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/clear_browsing_data/clear_browsing_data_manager.h"

#include "base/bind.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/sync/driver/test_sync_service.h"
#include "components/sync_preferences/pref_service_mock_factory.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/browsing_data/browsing_data_features.h"
#include "ios/chrome/browser/browsing_data/cache_counter.h"
#include "ios/chrome/browser/browsing_data/fake_browsing_data_remover.h"
#include "ios/chrome/browser/pref_names.h"
#include "ios/chrome/browser/prefs/browser_prefs.h"
#include "ios/chrome/browser/signin/authentication_service.h"
#include "ios/chrome/browser/signin/authentication_service_delegate_fake.h"
#include "ios/chrome/browser/signin/authentication_service_factory.h"
#include "ios/chrome/browser/sync/profile_sync_service_factory.h"
#include "ios/chrome/browser/sync/sync_setup_service_factory.h"
#include "ios/chrome/browser/sync/sync_setup_service_mock.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/fake_browsing_data_counter_wrapper_producer.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/browser/ui/table_view/table_view_model.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/public/provider/chrome/browser/signin/fake_chrome_identity_service.h"
#include "ios/web/public/test/web_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

std::unique_ptr<KeyedService> CreateTestSyncService(
    web::BrowserState* context) {
  return std::make_unique<syncer::TestSyncService>();
}

std::unique_ptr<KeyedService> BuildMockSyncSetupService(
    web::BrowserState* context) {
  ios::ChromeBrowserState* browser_state =
      ios::ChromeBrowserState::FromBrowserState(context);
  return std::make_unique<SyncSetupServiceMock>(
      ProfileSyncServiceFactory::GetForBrowserState(browser_state));
}

class ClearBrowsingDataManagerTest : public PlatformTest {
 public:
  ClearBrowsingDataManagerTest() {
    sync_preferences::PrefServiceMockFactory factory;
    scoped_refptr<user_prefs::PrefRegistrySyncable> registry(
        new user_prefs::PrefRegistrySyncable);
    std::unique_ptr<sync_preferences::PrefServiceSyncable> prefs =
        factory.CreateSyncable(registry.get());
    RegisterBrowserStatePrefs(registry.get());

    TestChromeBrowserState::Builder builder;
    builder.AddTestingFactory(ProfileSyncServiceFactory::GetInstance(),
                              base::BindRepeating(&CreateTestSyncService));
    builder.AddTestingFactory(SyncSetupServiceFactory::GetInstance(),
                              base::BindRepeating(&BuildMockSyncSetupService));
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    browser_state_ = builder.Build();

    AuthenticationServiceFactory::CreateAndInitializeForBrowserState(
        browser_state_.get(),
        std::make_unique<AuthenticationServiceDelegateFake>());

    ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()
        ->AddIdentities(@[ @"foo" ]);

    model_ = [[TableViewModel alloc] init];
    remover_ = std::make_unique<FakeBrowsingDataRemover>();
    manager_ = [[ClearBrowsingDataManager alloc]
                      initWithBrowserState:browser_state_.get()
                                  listType:ClearBrowsingDataListType::
                                               kListTypeTableView
                       browsingDataRemover:remover_.get()
        browsingDataCounterWrapperProducer:
            [[FakeBrowsingDataCounterWrapperProducer alloc] init]];

    test_sync_service_ = static_cast<syncer::TestSyncService*>(
        ProfileSyncServiceFactory::GetForBrowserState(browser_state_.get()));

    time_range_pref_.Init(browsing_data::prefs::kDeleteTimePeriod,
                          browser_state_->GetPrefs());
  }

  ChromeIdentity* fake_identity() {
    return [ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()
                ->GetAllIdentities() firstObject];
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  TableViewModel* model_;
  std::unique_ptr<BrowsingDataRemover> remover_;
  ClearBrowsingDataManager* manager_;
  syncer::TestSyncService* test_sync_service_;
  IntegerPrefMember time_range_pref_;
};

// Tests model is set up with all appropriate items and sections.
TEST_F(ClearBrowsingDataManagerTest, TestModel) {
  [manager_ loadModel:model_];

  EXPECT_EQ(3, [model_ numberOfSections]);
  if (IsNewClearBrowsingDataUIEnabled()) {
    // Time Range selector.
    EXPECT_EQ(1, [model_ numberOfItemsInSection:0]);
    EXPECT_EQ(5, [model_ numberOfItemsInSection:1]);
  } else {
    EXPECT_EQ(5, [model_ numberOfItemsInSection:0]);
    // CBD button.
    EXPECT_EQ(1, [model_ numberOfItemsInSection:1]);
  }
  EXPECT_EQ(1, [model_ numberOfItemsInSection:2]);
}

// Tests model is set up with correct number of items and sections if signed in
// but sync is off.
TEST_F(ClearBrowsingDataManagerTest, TestModelSignedInSyncOff) {
  // Ensure that sync is not running.
  test_sync_service_->SetDisableReasons(
      syncer::SyncService::DISABLE_REASON_USER_CHOICE);

  AuthenticationServiceFactory::GetForBrowserState(browser_state_.get())
      ->SignIn(fake_identity());

  [manager_ loadModel:model_];

  EXPECT_EQ(4, [model_ numberOfSections]);
  if (IsNewClearBrowsingDataUIEnabled()) {
    // Time Range selector.
    EXPECT_EQ(1, [model_ numberOfItemsInSection:0]);
    EXPECT_EQ(5, [model_ numberOfItemsInSection:1]);
  } else {
    EXPECT_EQ(5, [model_ numberOfItemsInSection:0]);
    // CBD button.
    EXPECT_EQ(1, [model_ numberOfItemsInSection:1]);
  }
  EXPECT_EQ(1, [model_ numberOfItemsInSection:2]);
  EXPECT_EQ(1, [model_ numberOfItemsInSection:3]);
}

TEST_F(ClearBrowsingDataManagerTest, TestCacheCounterFormattingForAllTime) {
  ASSERT_EQ("en", GetApplicationContext()->GetApplicationLocale());
  PrefService* prefs = browser_state_->GetPrefs();
  prefs->SetInteger(browsing_data::prefs::kDeleteTimePeriod,
                    static_cast<int>(browsing_data::TimePeriod::ALL_TIME));
  CacheCounter counter(browser_state_.get());

  // Test multiple possible types of formatting.
  // clang-format off
    const struct TestCase {
        int cache_size;
        NSString* expected_output;
    } kTestCases[] = {
        {0, @"Less than 1 MB"},
        {(1 << 20) - 1, @"Less than 1 MB"},
        {(1 << 20), @"1 MB"},
        {(1 << 20) + (1 << 19), @"1.5 MB"},
        {(1 << 21), @"2 MB"},
        {(1 << 30), @"1 GB"}
    };
  // clang-format on

  for (const TestCase& test_case : kTestCases) {
    browsing_data::BrowsingDataCounter::FinishedResult result(
        &counter, test_case.cache_size);
    NSString* output = [manager_ counterTextFromResult:result];
    EXPECT_NSEQ(test_case.expected_output, output);
  }
}

TEST_F(ClearBrowsingDataManagerTest,
       TestCacheCounterFormattingForLessThanAllTime) {
  ASSERT_EQ("en", GetApplicationContext()->GetApplicationLocale());

  // If the new UI is not enabled then the pref value for the time period
  // is ignored and the time period defaults to ALL_TIME.
  if (!IsNewClearBrowsingDataUIEnabled()) {
    return;
  }
  PrefService* prefs = browser_state_->GetPrefs();
  prefs->SetInteger(browsing_data::prefs::kDeleteTimePeriod,
                    static_cast<int>(browsing_data::TimePeriod::LAST_HOUR));
  CacheCounter counter(browser_state_.get());

  // Test multiple possible types of formatting.
  // clang-format off
    const struct TestCase {
        int cache_size;
        NSString* expected_output;
    } kTestCases[] = {
        {0, @"Less than 1 MB"},
        {(1 << 20) - 1, @"Less than 1 MB"},
        {(1 << 20), @"Less than 1 MB"},
        {(1 << 20) + (1 << 19), @"Less than 1.5 MB"},
        {(1 << 21), @"Less than 2 MB"},
        {(1 << 30), @"Less than 1 GB"}
    };
  // clang-format on

  for (const TestCase& test_case : kTestCases) {
    browsing_data::BrowsingDataCounter::FinishedResult result(
        &counter, test_case.cache_size);
    NSString* output = [manager_ counterTextFromResult:result];
    EXPECT_NSEQ(test_case.expected_output, output);
  }
}

TEST_F(ClearBrowsingDataManagerTest, TestOnPreferenceChanged) {
  // Only works with new UI
  if (!IsNewClearBrowsingDataUIEnabled()) {
    return;
  }
  [manager_ loadModel:model_];
  NSArray* timeRangeItems =
      [model_ itemsInSectionWithIdentifier:SectionIdentifierTimeRange];
  ASSERT_EQ(1UL, timeRangeItems.count);
  TableViewDetailIconItem* timeRangeItem = timeRangeItems.firstObject;
  ASSERT_TRUE([timeRangeItem isKindOfClass:[TableViewDetailIconItem class]]);

  // Changes of Time Range should trigger updates on Time Range item's
  // detailText.
  time_range_pref_.SetValue(2);
  EXPECT_NSEQ(l10n_util::GetNSString(
                  IDS_IOS_CLEAR_BROWSING_DATA_TIME_RANGE_OPTION_PAST_WEEK),
              timeRangeItem.detailText);
  time_range_pref_.SetValue(3);
  EXPECT_NSEQ(
      l10n_util::GetNSString(
          IDS_IOS_CLEAR_BROWSING_DATA_TIME_RANGE_OPTION_LAST_FOUR_WEEKS),
      timeRangeItem.detailText);
}

}  // namespace
