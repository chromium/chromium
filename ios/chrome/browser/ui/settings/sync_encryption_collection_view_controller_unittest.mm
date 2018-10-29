// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/sync_encryption_collection_view_controller.h"

#include <memory>

#include "base/compiler_specific.h"
#include "components/browser_sync/profile_sync_service_mock.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/sync/ios_chrome_profile_sync_test_util.h"
#include "ios/chrome/browser/sync/profile_sync_service_factory.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_controller_test.h"
#import "ios/chrome/browser/ui/settings/cells/encryption_item.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

using testing::NiceMock;
using testing::Return;

std::unique_ptr<KeyedService> CreateNiceProfileSyncServiceMock(
    web::BrowserState* context) {
  browser_sync::ProfileSyncService::InitParams init_params =
      CreateProfileSyncServiceParamsForTest(
          nullptr, ios::ChromeBrowserState::FromBrowserState(context));
  return std::make_unique<NiceMock<browser_sync::ProfileSyncServiceMock>>(
      &init_params);
}

class SyncEncryptionCollectionViewControllerTest
    : public CollectionViewControllerTest {
 protected:
  void SetUp() override {
    TestChromeBrowserState::Builder test_cbs_builder;
    test_cbs_builder.AddTestingFactory(
        ProfileSyncServiceFactory::GetInstance(),
        base::BindRepeating(&CreateNiceProfileSyncServiceMock));
    chrome_browser_state_ = test_cbs_builder.Build();
    CollectionViewControllerTest::SetUp();

    mock_profile_sync_service_ =
        static_cast<browser_sync::ProfileSyncServiceMock*>(
            ProfileSyncServiceFactory::GetForBrowserState(
                chrome_browser_state_.get()));
    ON_CALL(*mock_profile_sync_service_, GetTransportState())
        .WillByDefault(Return(syncer::SyncService::TransportState::ACTIVE));
    ON_CALL(*mock_profile_sync_service_, IsUsingSecondaryPassphrase())
        .WillByDefault(Return(true));

    CreateController();
  }

  CollectionViewController* InstantiateController() override {
    return [[SyncEncryptionCollectionViewController alloc]
        initWithBrowserState:chrome_browser_state_.get()];
  }

  web::TestWebThreadBundle thread_bundle_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  // Weak, owned by |chrome_browser_state_|.
  browser_sync::ProfileSyncServiceMock* mock_profile_sync_service_;
};

TEST_F(SyncEncryptionCollectionViewControllerTest, TestModel) {
  CheckController();
  CheckTitleWithId(IDS_IOS_SYNC_ENCRYPTION_TITLE);

  EXPECT_EQ(2, NumberOfSections());

  NSInteger const kSection = 0;
  EXPECT_EQ(2, NumberOfItemsInSection(kSection));

  EncryptionItem* accountItem = GetCollectionViewItem(kSection, 0);
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_SYNC_BASIC_ENCRYPTION_DATA),
              accountItem.text);

  EncryptionItem* passphraseItem = GetCollectionViewItem(kSection, 1);
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_SYNC_FULL_ENCRYPTION_DATA),
              passphraseItem.text);
}

}  // namespace
