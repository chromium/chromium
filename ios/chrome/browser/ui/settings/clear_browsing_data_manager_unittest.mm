// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/clear_browsing_data_manager.h"

#include "components/browser_sync/profile_sync_service_mock.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/signin/core/browser/fake_signin_manager.h"
#include "components/sync_preferences/pref_service_mock_factory.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/browsing_data/cache_counter.h"
#include "ios/chrome/browser/experimental_flags.h"
#include "ios/chrome/browser/pref_names.h"
#include "ios/chrome/browser/prefs/browser_prefs.h"
#include "ios/chrome/browser/signin/fake_oauth2_token_service_builder.h"
#include "ios/chrome/browser/signin/fake_signin_manager_builder.h"
#include "ios/chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "ios/chrome/browser/signin/signin_manager_factory.h"
#include "ios/chrome/browser/sync/ios_chrome_profile_sync_test_util.h"
#include "ios/chrome/browser/sync/profile_sync_service_factory.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_model.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using testing::Return;

@interface ClearBrowsingDataManager (ExposedForTesting)
- (void)loadModel:(ListModel*)model;
@end

namespace {

class ClearBrowsingDataManagerTest : public PlatformTest {
 public:
  ClearBrowsingDataManagerTest() {
    sync_preferences::PrefServiceMockFactory factory;
    scoped_refptr<user_prefs::PrefRegistrySyncable> registry(
        new user_prefs::PrefRegistrySyncable);
    std::unique_ptr<sync_preferences::PrefServiceSyncable> prefs =
        factory.CreateSyncable(registry.get());
    RegisterBrowserStatePrefs(registry.get());

    // Setup identity services.
    TestChromeBrowserState::Builder builder;
    builder.SetPrefService(std::move(prefs));
    builder.AddTestingFactory(
        ProfileSyncServiceFactory::GetInstance(),
        base::BindRepeating(&BuildMockProfileSyncService));
    builder.AddTestingFactory(
        ProfileOAuth2TokenServiceFactory::GetInstance(),
        base::BindRepeating(&BuildFakeOAuth2TokenService));
    builder.AddTestingFactory(
        ios::SigninManagerFactory::GetInstance(),
        base::BindRepeating(&ios::BuildFakeSigninManager));
    browser_state_ = builder.Build();

    model_ = [[CollectionViewModel alloc] init];
    manager_ = [[ClearBrowsingDataManager alloc]
        initWithBrowserState:browser_state_.get()
                    listType:ClearBrowsingDataListType::
                                 kListTypeCollectionView];

    signin_manager_ =
        ios::SigninManagerFactory::GetForBrowserState(browser_state_.get());
    mock_sync_service_ = static_cast<browser_sync::ProfileSyncServiceMock*>(
        ProfileSyncServiceFactory::GetForBrowserState(browser_state_.get()));
  }

 protected:
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  CollectionViewModel* model_;
  ClearBrowsingDataManager* manager_;
  SigninManagerBase* signin_manager_;
  browser_sync::ProfileSyncServiceMock* mock_sync_service_;
  web::TestWebThreadBundle thread_bundle_;
};

// Tests model is set up with all appropriate items and sections.
TEST_F(ClearBrowsingDataManagerTest, TestModel) {
  [manager_ loadModel:model_];

  int section_offset = 0;
  if (experimental_flags::IsNewClearBrowsingDataUIEnabled()) {
    EXPECT_EQ(4, [model_ numberOfSections]);
    EXPECT_EQ(1, [model_ numberOfItemsInSection:0]);
    section_offset = 1;
  } else {
    EXPECT_EQ(3, [model_ numberOfSections]);
  }
  EXPECT_EQ(5, [model_ numberOfItemsInSection:0 + section_offset]);
  EXPECT_EQ(1, [model_ numberOfItemsInSection:1 + section_offset]);
  EXPECT_EQ(1, [model_ numberOfItemsInSection:2 + section_offset]);
}

// Tests model is set up with correct number of items and sections if signed in
// but sync is off.
TEST_F(ClearBrowsingDataManagerTest, TestModelSignedInSyncOff) {
  // Ensure that sync is not running.
  EXPECT_CALL(*mock_sync_service_, GetDisableReasons())
      .WillRepeatedly(Return(syncer::SyncService::DISABLE_REASON_USER_CHOICE));

  signin_manager_->SetAuthenticatedAccountInfo("12345", "syncuser@example.com");

  [manager_ loadModel:model_];

  int section_offset = 0;
  if (experimental_flags::IsNewClearBrowsingDataUIEnabled()) {
    EXPECT_EQ(5, [model_ numberOfSections]);
    EXPECT_EQ(1, [model_ numberOfItemsInSection:0]);
    section_offset = 1;
  } else {
    EXPECT_EQ(4, [model_ numberOfSections]);
  }

  EXPECT_EQ(5, [model_ numberOfItemsInSection:0 + section_offset]);
  EXPECT_EQ(1, [model_ numberOfItemsInSection:1 + section_offset]);
  EXPECT_EQ(1, [model_ numberOfItemsInSection:2 + section_offset]);
  EXPECT_EQ(1, [model_ numberOfItemsInSection:3 + section_offset]);
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
  if (!experimental_flags::IsNewClearBrowsingDataUIEnabled()) {
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

}  // namespace
