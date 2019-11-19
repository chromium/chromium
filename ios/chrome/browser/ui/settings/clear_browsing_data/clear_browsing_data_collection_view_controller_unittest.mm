// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/clear_browsing_data/clear_browsing_data_collection_view_controller.h"

#include <memory>

#include "base/bind.h"
#include "base/mac/foundation_util.h"
#include "base/memory/ptr_util.h"
#include "base/strings/sys_string_conversions.h"
#include "components/browsing_data/core/browsing_data_utils.h"
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
#import "ios/chrome/browser/ui/collection_view/collection_view_controller_test.h"
#import "ios/chrome/browser/ui/settings/cells/settings_text_item.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/clear_browsing_data_manager.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/fake_browsing_data_counter_wrapper_producer.h"
#import "ios/chrome/common/string_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/public/provider/chrome/browser/signin/fake_chrome_identity_service.h"
#include "ios/web/public/test/web_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ClearBrowsingDataCollectionViewController (ExposedForTesting)
- (NSString*)counterTextFromResult:
    (const browsing_data::BrowsingDataCounter::Result&)result;
@end

namespace {

enum ItemEnum {
  kDeleteBrowsingHistoryItem,
  kDeleteCookiesItem,
  kDeleteCacheItem,
  kDeletePasswordsItem,
  kDeleteFormDataItem
};

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

class ClearBrowsingDataCollectionViewControllerTest
    : public CollectionViewControllerTest {
 protected:
  void SetUp() override {
    CollectionViewControllerTest::SetUp();

    // Setup identity services.
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
        ->AddIdentities(@[ @"syncuser@example.com" ]);

    test_sync_service_ = static_cast<syncer::TestSyncService*>(
        ProfileSyncServiceFactory::GetForBrowserState(browser_state_.get()));

    remover_ = std::make_unique<FakeBrowsingDataRemover>();
  }

  std::unique_ptr<sync_preferences::PrefServiceSyncable> CreatePrefService() {
    sync_preferences::PrefServiceMockFactory factory;
    scoped_refptr<user_prefs::PrefRegistrySyncable> registry(
        new user_prefs::PrefRegistrySyncable);
    std::unique_ptr<sync_preferences::PrefServiceSyncable> prefs =
        factory.CreateSyncable(registry.get());
    RegisterBrowserStatePrefs(registry.get());
    return prefs;
  }

  CollectionViewController* InstantiateController() override {
    ClearBrowsingDataManager* manager = [[ClearBrowsingDataManager alloc]
                      initWithBrowserState:browser_state_.get()
                                  listType:ClearBrowsingDataListType::
                                               kListTypeCollectionView
                       browsingDataRemover:remover_.get()
        browsingDataCounterWrapperProducer:
            [[FakeBrowsingDataCounterWrapperProducer alloc] init]];
    return [[ClearBrowsingDataCollectionViewController alloc]
        initWithBrowserState:browser_state_.get()
                     manager:manager];
  }

  void SelectItem(int item, int section) {
    NSIndexPath* indexPath = [NSIndexPath indexPathForItem:item
                                                 inSection:section];
    [controller() collectionView:[controller() collectionView]
        didSelectItemAtIndexPath:indexPath];
  }

  ChromeIdentity* fake_identity() {
    return [ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()
                ->GetAllIdentities() firstObject];
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  syncer::TestSyncService* test_sync_service_;
  std::unique_ptr<BrowsingDataRemover> remover_;
};

// Tests ClearBrowsingDataCollectionViewControllerTest is set up with all
// appropriate items and sections.
TEST_F(ClearBrowsingDataCollectionViewControllerTest, TestModel) {
  test_sync_service_->SetDisableReasons(
      syncer::SyncService::DISABLE_REASON_USER_CHOICE);
  CreateController();
  CheckController();

  int section_offset = 0;
  if (IsNewClearBrowsingDataUIEnabled()) {
    section_offset = 1;
  }

  CheckTextCellTextWithId(IDS_IOS_CLEAR_BROWSING_HISTORY, 0 + section_offset,
                          0);
  CheckAccessoryType(MDCCollectionViewCellAccessoryCheckmark,
                     0 + section_offset, 0);
  CheckTextCellTextWithId(IDS_IOS_CLEAR_COOKIES, 0 + section_offset, 1);
  CheckAccessoryType(MDCCollectionViewCellAccessoryCheckmark,
                     0 + section_offset, 1);
  CheckTextCellTextWithId(IDS_IOS_CLEAR_CACHE, 0 + section_offset, 2);
  CheckAccessoryType(MDCCollectionViewCellAccessoryCheckmark,
                     0 + section_offset, 2);
  CheckTextCellTextWithId(IDS_IOS_CLEAR_SAVED_PASSWORDS, 0 + section_offset, 3);
  CheckAccessoryType(MDCCollectionViewCellAccessoryNone, 0 + section_offset, 3);
  CheckTextCellTextWithId(IDS_IOS_CLEAR_AUTOFILL, 0 + section_offset, 4);
  CheckAccessoryType(MDCCollectionViewCellAccessoryNone, 0 + section_offset, 4);

  CheckTextCellTextWithId(IDS_IOS_CLEAR_BUTTON, 1 + section_offset, 0);

  CheckSectionFooterWithId(IDS_IOS_CLEAR_BROWSING_DATA_FOOTER_SAVED_SITE_DATA,
                           2 + section_offset);
}

TEST_F(ClearBrowsingDataCollectionViewControllerTest,
       TestItemsSignedInSyncOff) {
  AuthenticationServiceFactory::GetForBrowserState(browser_state_.get())
      ->SignIn(fake_identity());

  test_sync_service_->SetDisableReasons(
      syncer::SyncService::DISABLE_REASON_USER_CHOICE);

  CreateController();
  CheckController();

  int section_offset = 0;
  if (IsNewClearBrowsingDataUIEnabled()) {
    EXPECT_EQ(5, NumberOfSections());
    EXPECT_EQ(1, NumberOfItemsInSection(0));
    section_offset = 1;
  } else {
    EXPECT_EQ(4, NumberOfSections());
  }

  EXPECT_EQ(5, NumberOfItemsInSection(0 + section_offset));
  EXPECT_EQ(1, NumberOfItemsInSection(1 + section_offset));

  EXPECT_EQ(1, NumberOfItemsInSection(2 + section_offset));
  CheckSectionFooterWithId(IDS_IOS_CLEAR_BROWSING_DATA_FOOTER_ACCOUNT,
                           2 + section_offset);

  EXPECT_EQ(1, NumberOfItemsInSection(3 + section_offset));
  CheckSectionFooterWithId(IDS_IOS_CLEAR_BROWSING_DATA_FOOTER_SAVED_SITE_DATA,
                           3 + section_offset);
}

TEST_F(ClearBrowsingDataCollectionViewControllerTest,
       TestItemsSignedInSyncActiveHistoryOff) {
  test_sync_service_->SetDisableReasons(
      syncer::SyncService::DISABLE_REASON_NONE);
  test_sync_service_->SetTransportState(
      syncer::SyncService::TransportState::ACTIVE);
  test_sync_service_->SetFirstSetupComplete(true);
  test_sync_service_->SetActiveDataTypes(syncer::ModelTypeSet());
  test_sync_service_->SetIsUsingSecondaryPassphrase(true);

  AuthenticationServiceFactory::GetForBrowserState(browser_state_.get())
      ->SignIn(fake_identity());
  CreateController();
  CheckController();

  int section_offset = 0;
  if (IsNewClearBrowsingDataUIEnabled()) {
    section_offset = 1;
  }

  CheckSectionFooterWithId(IDS_IOS_CLEAR_BROWSING_DATA_FOOTER_ACCOUNT,
                           2 + section_offset);

  CheckSectionFooterWithId(
      IDS_IOS_CLEAR_BROWSING_DATA_FOOTER_CLEAR_SYNC_AND_SAVED_SITE_DATA,
      3 + section_offset);
}

TEST_F(ClearBrowsingDataCollectionViewControllerTest, TestUpdatePrefWithValue) {
  CreateController();
  CheckController();
  PrefService* prefs = browser_state_->GetPrefs();

  const int section_offset = IsNewClearBrowsingDataUIEnabled() ? 1 : 0;

  SelectItem(kDeleteBrowsingHistoryItem, 0 + section_offset);
  EXPECT_FALSE(prefs->GetBoolean(browsing_data::prefs::kDeleteBrowsingHistory));
  SelectItem(kDeleteCookiesItem, 0 + section_offset);
  EXPECT_FALSE(prefs->GetBoolean(browsing_data::prefs::kDeleteCookies));
  SelectItem(kDeleteCacheItem, 0 + section_offset);
  EXPECT_FALSE(prefs->GetBoolean(browsing_data::prefs::kDeleteCache));
  SelectItem(kDeletePasswordsItem, 0 + section_offset);
  EXPECT_TRUE(prefs->GetBoolean(browsing_data::prefs::kDeletePasswords));
  SelectItem(kDeleteFormDataItem, 0 + section_offset);
  EXPECT_TRUE(prefs->GetBoolean(browsing_data::prefs::kDeleteFormData));
}

}  // namespace
