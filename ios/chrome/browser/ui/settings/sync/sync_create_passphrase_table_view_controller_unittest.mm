// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/sync/sync_create_passphrase_table_view_controller.h"

#import <UIKit/UIKit.h>

#include "base/compiler_specific.h"
#import "base/test/ios/wait_util.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/driver/mock_sync_service.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/ui/settings/cells/byo_textfield_item.h"
#import "ios/chrome/browser/ui/settings/cells/passphrase_error_item.h"
#import "ios/chrome/browser/ui/settings/passphrase_table_view_controller_test.h"
#include "ios/chrome/grit/ios_strings.h"
#include "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

using testing::_;
using testing::AtLeast;
using testing::Return;

class SyncCreatePassphraseTableViewControllerTest
    : public PassphraseTableViewControllerTest {
 public:
  SyncCreatePassphraseTableViewControllerTest() {}

 protected:
  void TearDown() override {
    [SyncController() stopObserving];
    PassphraseTableViewControllerTest::TearDown();
  }

  ChromeTableViewController* InstantiateController() override {
    return [[SyncCreatePassphraseTableViewController alloc]
        initWithBrowserState:chrome_browser_state_.get()];
  }

  SyncCreatePassphraseTableViewController* SyncController() {
    return static_cast<SyncCreatePassphraseTableViewController*>(controller());
  }
};

TEST_F(SyncCreatePassphraseTableViewControllerTest, TestConstructorDestructor) {
  CreateController();
  CheckController();
  EXPECT_CALL(*fake_sync_service_->GetMockUserSettings(),
              SetEncryptionPassphrase(_))
      .Times(0);
  // Simulate the view appearing.
  [controller() viewDidAppear:YES];
}

TEST_F(SyncCreatePassphraseTableViewControllerTest, TestModel) {
  SyncCreatePassphraseTableViewController* controller = SyncController();
  EXPECT_EQ(1, NumberOfSections());
  NSInteger const kSection = 0;
  EXPECT_EQ(2, NumberOfItemsInSection(kSection));
  NSString* expectedTitle =
      l10n_util::GetNSString(IDS_IOS_SYNC_ENCRYPTION_CREATE_PASSPHRASE);
  CheckTitle(expectedTitle);
  // Passphrase item.
  BYOTextFieldItem* passphraseItem = GetTableViewItem(kSection, 0);
  EXPECT_NSEQ(controller.passphrase, passphraseItem.textField);
  // Confirm passphrase item.
  BYOTextFieldItem* confirmPassphraseItem = GetTableViewItem(kSection, 1);
  EXPECT_NSEQ(controller.confirmPassphrase, confirmPassphraseItem.textField);
}

TEST_F(SyncCreatePassphraseTableViewControllerTest, TestAllFieldsFilled) {
  SyncCreatePassphraseTableViewController* sync_controller = SyncController();
  // Both text fields empty should return false.
  EXPECT_FALSE([sync_controller areAllFieldsFilled]);
  // One text field empty should return false.
  [[sync_controller passphrase] setText:@"decodeme"];
  EXPECT_FALSE([sync_controller areAllFieldsFilled]);
  [[sync_controller passphrase] setText:@""];
  [[sync_controller confirmPassphrase] setText:@"decodeme"];
  EXPECT_FALSE([sync_controller areAllFieldsFilled]);
  [[sync_controller passphrase] setText:@"decodeme"];
  // Neither text field empty ("decodeme") should return true.
  EXPECT_TRUE([sync_controller areAllFieldsFilled]);
}

TEST_F(SyncCreatePassphraseTableViewControllerTest, TestCredentialsOkPressed) {
  SyncCreatePassphraseTableViewController* sync_controller = SyncController();
  EXPECT_CALL(*fake_sync_service_->GetMockUserSettings(),
              SetEncryptionPassphrase(_))
      .Times(0);
  EXPECT_FALSE([[sync_controller navigationItem].rightBarButtonItem isEnabled]);
  [sync_controller signInPressed];
}

TEST_F(SyncCreatePassphraseTableViewControllerTest, TestNextTextField) {
  SyncCreatePassphraseTableViewController* sync_controller = SyncController();
  // The second call to -nextTextField is the same as hitting 'ok'.
  // With matching text, this should cause an attempt to set the passphrase.
  EXPECT_CALL(*fake_sync_service_, AddObserver(_)).Times(AtLeast(1));
  EXPECT_CALL(*fake_sync_service_, RemoveObserver(_)).Times(AtLeast(1));
  EXPECT_CALL(*fake_sync_service_->GetMockUserSettings(),
              SetEncryptionPassphrase("decodeme"));
  [[sync_controller passphrase] setText:@"decodeme"];
  [sync_controller textFieldDidChange:[sync_controller passphrase]];
  [sync_controller textFieldDidEndEditing:[sync_controller passphrase]];
  [[sync_controller confirmPassphrase] setText:@"decodeme"];
  [sync_controller textFieldDidChange:[sync_controller confirmPassphrase]];
  [sync_controller textFieldDidEndEditing:[sync_controller confirmPassphrase]];
}

TEST_F(SyncCreatePassphraseTableViewControllerTest, TestOneTextFieldEmpty) {
  SyncCreatePassphraseTableViewController* sync_controller = SyncController();
  EXPECT_CALL(*fake_sync_service_->GetMockUserSettings(),
              SetEncryptionPassphrase(_))
      .Times(0);
  [[sync_controller passphrase] setText:@"decodeme"];
  [sync_controller textFieldDidChange:[sync_controller passphrase]];
  // Expect the right button to be visible.
  EXPECT_TRUE([sync_controller navigationItem].rightBarButtonItem);
  // Expect the right button to be disabled.
  EXPECT_FALSE([[sync_controller navigationItem].rightBarButtonItem isEnabled]);
}

// Tests that if user inputs incompatible passwords, an error message will be
// shown. If the user types again, the error messasge will be cleared.
TEST_F(SyncCreatePassphraseTableViewControllerTest, TestTextFieldsDoNotMatch) {
  SyncCreatePassphraseTableViewController* sync_controller = SyncController();
  // Mismatching text fields should not get to the point of trying to set the
  // passphrase and adding the sync observer.
  EXPECT_CALL(*fake_sync_service_, AddObserver(_)).Times(0);
  EXPECT_CALL(*fake_sync_service_->GetMockUserSettings(),
              SetEncryptionPassphrase(_))
      .Times(0);
  [[sync_controller passphrase] setText:@"decodeme"];
  [[sync_controller confirmPassphrase] setText:@"donothing"];
  [sync_controller textFieldDidChange:[sync_controller passphrase]];
  [sync_controller signInPressed];

  // Check the error cell.
  NSInteger const kSection = 0;
  EXPECT_EQ(3, NumberOfItemsInSection(kSection));
  PassphraseErrorItem* item = GetTableViewItem(kSection, 2);
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_SYNC_PASSPHRASE_MISMATCH_ERROR),
              item.text);

  // User types again.
  [sync_controller textFieldDidBeginEditing:sync_controller.confirmPassphrase];

  // Check that error message is cleared.
  EXPECT_EQ(2, NumberOfItemsInSection(kSection));
  EXPECT_FALSE([sync_controller.tableViewModel
      hasItemForItemType:sync_encryption_passphrase::ItemTypeError
       sectionIdentifier:sync_encryption_passphrase::
                             SectionIdentifierPassphrase]);
}

TEST_F(SyncCreatePassphraseTableViewControllerTest, TestTextFieldsMatch) {
  SyncCreatePassphraseTableViewController* sync_controller = SyncController();
  // Matching text should cause an attempt to set it and add a sync observer.
  EXPECT_CALL(*fake_sync_service_, AddObserver(_)).Times(AtLeast(1));
  EXPECT_CALL(*fake_sync_service_, RemoveObserver(_)).Times(AtLeast(1));
  EXPECT_CALL(*fake_sync_service_->GetMockUserSettings(),
              SetEncryptionPassphrase("decodeme"));
  [[sync_controller passphrase] setText:@"decodeme"];
  [[sync_controller confirmPassphrase] setText:@"decodeme"];
  [sync_controller textFieldDidChange:[sync_controller passphrase]];
  [sync_controller signInPressed];
}

TEST_F(SyncCreatePassphraseTableViewControllerTest, TestOnStateChanged) {
  SyncCreatePassphraseTableViewController* sync_controller = SyncController();
  SetUpNavigationController(sync_controller);
  EXPECT_EQ([nav_controller_ topViewController], sync_controller);

  // Set up the fake sync service to have accepted the passphrase.
  ON_CALL(*fake_sync_service_->GetMockUserSettings(), IsPassphraseRequired())
      .WillByDefault(Return(false));
  ON_CALL(*fake_sync_service_->GetMockUserSettings(),
          IsUsingSecondaryPassphrase())
      .WillByDefault(Return(true));
  [sync_controller onSyncStateChanged];
  // Calling -onStateChanged with an accepted secondary passphrase should
  // cause the controller to be popped off the navigation stack.
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool() {
        return [nav_controller_ topViewController] != sync_controller;
      }));
}

// TODO(crbug.com/658269): Re-enable test once it's been deflaked.
// Verifies that sync errors don't make the navigation item disappear.
// Regression test for http://crbug.com/501784.
TEST_F(SyncCreatePassphraseTableViewControllerTest,
       DISABLED_TestOnStateChangedError) {
  SyncCreatePassphraseTableViewController* sync_controller = SyncController();
  SetUpNavigationController(sync_controller);
  EXPECT_EQ([nav_controller_ topViewController], sync_controller);
  EXPECT_NE(nil, sync_controller.title);
  // Install a fake left button item, to check it's not removed.
  UIBarButtonItem* leftBarButtonItem =
      [[UIBarButtonItem alloc] initWithTitle:@"Left"
                                       style:UIBarButtonItemStylePlain
                                      target:nil
                                      action:nil];
  sync_controller.navigationItem.leftBarButtonItem = leftBarButtonItem;

  // Set up the fake sync service to be in a passphrase creation state.
  ON_CALL(*fake_sync_service_->GetMockUserSettings(), IsPassphraseRequired())
      .WillByDefault(Return(false));
  ON_CALL(*fake_sync_service_->GetMockUserSettings(),
          IsUsingSecondaryPassphrase())
      .WillByDefault(Return(false));
  [sync_controller onSyncStateChanged];
  EXPECT_EQ([nav_controller_ topViewController], sync_controller);
  EXPECT_NE(nil, sync_controller.title);
  EXPECT_EQ(leftBarButtonItem,
            sync_controller.navigationItem.leftBarButtonItem);
}

}  // namespace
