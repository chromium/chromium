// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/sync/sync_encryption_table_view_controller.h"

#import <memory>

#import "base/apple/foundation_util.h"
#import "base/functional/bind.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller_test.h"
#import "ios/chrome/browser/ui/settings/content_settings/content_settings_table_view_controller.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

class SyncEncryptionTableViewControllerTest
    : public LegacyChromeTableViewControllerTest {
 protected:
  void SetUp() override {
    LegacyChromeTableViewControllerTest::SetUp();

    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());

    CreateController();
  }

  void TearDown() override {
    SyncEncryptionTableViewController* controller_ =
        base::apple::ObjCCastStrict<SyncEncryptionTableViewController>(
            controller());
    if ([controller_ respondsToSelector:@selector(settingsWillBeDismissed)]) {
      [controller_ performSelector:@selector(settingsWillBeDismissed)];
    }
    LegacyChromeTableViewControllerTest::TearDown();
  }

  LegacyChromeTableViewController* InstantiateController() override {
    return [[SyncEncryptionTableViewController alloc]
        initWithBrowser:browser_.get()];
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
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
