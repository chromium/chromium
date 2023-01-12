// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/sync/sync_encryption_table_view_controller.h"

#import <memory>

#import "base/compiler_specific.h"
#import "base/functional/bind.h"
#import "components/strings/grit/components_strings.h"
#import "components/sync/test/test_sync_service.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/sync/sync_service_factory.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_controller_test.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

class SyncEncryptionTableViewControllerTest
    : public ChromeTableViewControllerTest {
 protected:
  void SetUp() override {
    ChromeTableViewControllerTest::SetUp();

    browser_state_ = TestChromeBrowserState::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(browser_state_.get());
    syncer::TestSyncService* test_sync_service =
        static_cast<syncer::TestSyncService*>(
            SyncServiceFactory::GetForBrowserState(browser_state_.get()));
    test_sync_service->SetIsUsingExplicitPassphrase(true);

    CreateController();
  }

  ChromeTableViewController* InstantiateController() override {
    return [[SyncEncryptionTableViewController alloc]
        initWithBrowser:browser_.get()];
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<TestBrowser> browser_;
};

TEST_F(SyncEncryptionTableViewControllerTest, TestModel) {
  CheckController();
  CheckTitleWithId(IDS_IOS_SYNC_ENCRYPTION_TITLE);

  EXPECT_EQ(1, NumberOfSections());

  NSInteger const kSection = 0;
  EXPECT_EQ(2, NumberOfItemsInSection(kSection));

  TableViewTextItem* accountItem = GetTableViewItem(kSection, 0);
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_SYNC_BASIC_ENCRYPTION_DATA),
              accountItem.text);

  TableViewTextItem* passphraseItem = GetTableViewItem(kSection, 1);
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_SYNC_FULL_ENCRYPTION_DATA),
              passphraseItem.text);
}

}  // namespace
