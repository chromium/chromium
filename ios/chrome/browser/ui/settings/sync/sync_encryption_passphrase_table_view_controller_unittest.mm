// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/sync/sync_encryption_passphrase_table_view_controller.h"

#import <UIKit/UIKit.h>

#import <memory>

#import "base/functional/bind.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "components/strings/grit/components_strings.h"
#import "components/sync/test/mock_sync_service.h"
#import "ios/chrome/browser/settings/model/sync/utils/sync_util.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/browser/ui/settings/cells/byo_textfield_item.h"
#import "ios/chrome/browser/ui/settings/passphrase_table_view_controller_test.h"
#import "ios/chrome/browser/ui/settings/settings_navigation_controller.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

using testing::AtLeast;
using testing::NiceMock;
using testing::Return;

class SyncEncryptionPassphraseTableViewControllerTest
    : public PassphraseTableViewControllerTest {
 public:
  SyncEncryptionPassphraseTableViewControllerTest() = default;

  void TurnSyncPassphraseErrorOn() {
    ON_CALL(*fake_sync_service_, GetUserActionableError())
        .WillByDefault(
            Return(syncer::SyncService::UserActionableError::kNeedsPassphrase));
    ON_CALL(*fake_sync_service_->GetMockUserSettings(), IsPassphraseRequired)
        .WillByDefault(Return(true));
  }

  void TurnSyncOtherErrorOn(syncer::SyncService::UserActionableError error) {
    ON_CALL(*fake_sync_service_, GetUserActionableError())
        .WillByDefault(Return(error));
    ON_CALL(*fake_sync_service_->GetMockUserSettings(), IsPassphraseRequired)
        .WillByDefault(Return(false));
  }

  void TurnSyncErrorOff() {
    ON_CALL(*fake_sync_service_, GetUserActionableError())
        .WillByDefault(Return(syncer::SyncService::UserActionableError::kNone));
    ON_CALL(*fake_sync_service_->GetMockUserSettings(), IsPassphraseRequired)
        .WillByDefault(Return(false));
  }

 protected:
  void SetUp() override {
    PassphraseTableViewControllerTest::SetUp();
    ON_CALL(*fake_sync_service_, GetTransportState)
        .WillByDefault(Return(syncer::SyncService::TransportState::ACTIVE));
    ON_CALL(*fake_sync_service_->GetMockUserSettings(),
            IsUsingExplicitPassphrase)
        .WillByDefault(Return(true));
    TurnSyncErrorOff();
  }

  LegacyChromeTableViewController* InstantiateController() override {
    return [[SyncEncryptionPassphraseTableViewController alloc]
        initWithBrowser:browser_.get()];
  }

  SyncEncryptionPassphraseTableViewController* SyncController() {
    return static_cast<SyncEncryptionPassphraseTableViewController*>(
        controller());
  }
};

TEST_F(SyncEncryptionPassphraseTableViewControllerTest, TestModel) {
  TurnSyncPassphraseErrorOn();
  SyncEncryptionPassphraseTableViewController* controller = SyncController();
  EXPECT_EQ(1, NumberOfSections());
  EXPECT_EQ(2, NumberOfItemsInSection(0));
  // Passphrase message item.
  NSString* userEmail =
      AuthenticationServiceFactory::GetForProfile(profile_.get())
          ->GetPrimaryIdentity(signin::ConsentLevel::kSignin)
          .userEmail;
  EXPECT_NSEQ(
      l10n_util::GetNSStringF(IDS_IOS_SYNC_ENTER_PASSPHRASE_BODY_WITH_EMAIL,
                              base::SysNSStringToUTF16(userEmail)),
      [GetTableViewItem(0, 0) text]);
  // Passphrase item.
  EXPECT_NSEQ(controller.passphrase, [GetTableViewItem(0, 1) textField]);
}

TEST_F(SyncEncryptionPassphraseTableViewControllerTest,
       TestConstructorDestructor) {
  CreateController();
  CheckController();
  EXPECT_CALL(*fake_sync_service_->GetMockUserSettings(),
              SetDecryptionPassphrase)
      .Times(0);
  // Simulate the view appearing.
  [controller() viewWillAppear:YES];
  [controller() viewDidAppear:YES];
}

TEST_F(SyncEncryptionPassphraseTableViewControllerTest, TestDecryptButton) {
  SyncEncryptionPassphraseTableViewController* sync_controller =
      SyncController();
  [sync_controller textFieldDidChange:[sync_controller passphrase]];
  EXPECT_FALSE([[sync_controller navigationItem].rightBarButtonItem isEnabled]);
  [[sync_controller passphrase] setText:@"decodeme"];
  [sync_controller textFieldDidChange:[sync_controller passphrase]];
  EXPECT_TRUE([[sync_controller navigationItem].rightBarButtonItem isEnabled]);
}

TEST_F(SyncEncryptionPassphraseTableViewControllerTest,
       TestDecryptWrongPassphrase) {
  SyncEncryptionPassphraseTableViewController* sync_controller =
      SyncController();
  EXPECT_CALL(*fake_sync_service_, AddObserver).Times(AtLeast(1));
  EXPECT_CALL(*fake_sync_service_, RemoveObserver).Times(AtLeast(1));
  EXPECT_CALL(*fake_sync_service_->GetMockUserSettings(),
              SetDecryptionPassphrase);
  [[sync_controller passphrase] setText:@"decodeme"];
  // Set the return value for setting the passphrase to failure.
  ON_CALL(*fake_sync_service_->GetMockUserSettings(), SetDecryptionPassphrase)
      .WillByDefault(Return(false));
  [sync_controller signInPressed];
}

TEST_F(SyncEncryptionPassphraseTableViewControllerTest,
       TestDecryptCorrectPassphrase) {
  SyncEncryptionPassphraseTableViewController* sync_controller =
      SyncController();
  EXPECT_CALL(*fake_sync_service_, AddObserver).Times(AtLeast(1));
  EXPECT_CALL(*fake_sync_service_, RemoveObserver).Times(AtLeast(1));
  EXPECT_CALL(*fake_sync_service_->GetMockUserSettings(),
              SetDecryptionPassphrase);
  [[sync_controller passphrase] setText:@"decodeme"];
  // Set the return value for setting the passphrase to success.
  ON_CALL(*fake_sync_service_->GetMockUserSettings(), SetDecryptionPassphrase)
      .WillByDefault(Return(true));
  [sync_controller signInPressed];
}

TEST_F(SyncEncryptionPassphraseTableViewControllerTest,
       TestOnStateChangedWrongPassphrase) {
  SyncEncryptionPassphraseTableViewController* sync_controller =
      SyncController();
  // Sets up a UINavigationController that has `controller_` as the second view
  // controller on the navigation stack.
  SetUpNavigationController(sync_controller);
  EXPECT_EQ([nav_controller_ topViewController], sync_controller);

  // Set up the fake sync service to require the passphrase.
  TurnSyncPassphraseErrorOn();
  [sync_controller onSyncStateChanged];
  // The controller should only reload. Because there is text in the passphrase
  // field, the 'ok' button should be enabled.
  EXPECT_EQ([nav_controller_ topViewController], sync_controller);
}

TEST_F(SyncEncryptionPassphraseTableViewControllerTest,
       TestOnStateChangedCorrectPassphrase) {
  SyncEncryptionPassphraseTableViewController* sync_controller =
      SyncController();
  // Sets up a UINavigationController that has `controller_` as the second view
  // controller on the navigation stack.
  SetUpNavigationController(sync_controller);
  EXPECT_EQ([nav_controller_ topViewController], sync_controller);

  // Set up the fake sync service to have accepted the passphrase.
  TurnSyncErrorOff();
  [sync_controller onSyncStateChanged];
  // Calling -onStateChanged with an accepted explicit passphrase should
  // cause the controller to be popped off the navigation stack.
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool() {
        return [nav_controller_ topViewController] != sync_controller;
      }));
}

TEST_F(SyncEncryptionPassphraseTableViewControllerTest, TestMessage) {
  SyncEncryptionPassphraseTableViewController* sync_controller =
      SyncController();
  // Default.
  EXPECT_FALSE([sync_controller syncErrorMessage]);

  syncer::SyncService::UserActionableError otherError =
      syncer::SyncService::UserActionableError::kSignInNeedsUpdate;

  // With a custom message.
  [sync_controller setSyncErrorMessage:@"message"];
  EXPECT_NSEQ(@"message", [sync_controller syncErrorMessage]);
  TurnSyncPassphraseErrorOn();
  EXPECT_NSEQ(@"message", [sync_controller syncErrorMessage]);
  TurnSyncOtherErrorOn(otherError);
  EXPECT_NSEQ(@"message", [sync_controller syncErrorMessage]);
  TurnSyncErrorOff();
  EXPECT_NSEQ(@"message", [sync_controller syncErrorMessage]);

  // With no custom message.
  [sync_controller setSyncErrorMessage:nil];
  EXPECT_FALSE([sync_controller syncErrorMessage]);
  TurnSyncPassphraseErrorOn();
  EXPECT_FALSE([sync_controller syncErrorMessage]);
  TurnSyncOtherErrorOn(otherError);
  EXPECT_NSEQ(GetSyncErrorMessageForProfile(profile_.get()),
              [sync_controller syncErrorMessage]);
  TurnSyncErrorOff();
  EXPECT_FALSE([sync_controller syncErrorMessage]);
}

}  // namespace
