// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/sync/sync_encryption_passphrase_table_view_controller.h"

#import <UIKit/UIKit.h>

#include <memory>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/driver/mock_sync_service.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/sync/profile_sync_service_factory.h"
#include "ios/chrome/browser/sync/sync_setup_service.h"
#include "ios/chrome/browser/sync/sync_setup_service_factory.h"
#include "ios/chrome/browser/sync/sync_setup_service_mock.h"
#import "ios/chrome/browser/ui/settings/cells/byo_textfield_item.h"
#import "ios/chrome/browser/ui/settings/passphrase_table_view_controller_test.h"
#import "ios/chrome/browser/ui/settings/sync/utils/sync_util.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_item.h"
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
using testing::NiceMock;
using testing::Return;

class SyncEncryptionPassphraseTableViewControllerTest
    : public PassphraseTableViewControllerTest {
 public:
  SyncEncryptionPassphraseTableViewControllerTest() {}

  static std::unique_ptr<KeyedService> CreateSyncSetupService(
      web::BrowserState* context) {
    ios::ChromeBrowserState* chrome_browser_state =
        ios::ChromeBrowserState::FromBrowserState(context);
    syncer::SyncService* sync_service =
        ProfileSyncServiceFactory::GetForBrowserState(chrome_browser_state);
    return std::make_unique<SyncSetupServiceMock>(sync_service);
  }

  void TurnSyncPassphraseErrorOn() {
    ON_CALL(*mock_sync_setup_service_, GetSyncServiceState())
        .WillByDefault(Return(SyncSetupService::kSyncServiceNeedsPassphrase));
  }

  void TurnSyncOtherErrorOn(SyncSetupService::SyncServiceState state) {
    ON_CALL(*mock_sync_setup_service_, GetSyncServiceState())
        .WillByDefault(Return(state));
  }

  void TurnSyncErrorOff() {
    ON_CALL(*mock_sync_setup_service_, GetSyncServiceState())
        .WillByDefault(Return(SyncSetupService::kNoSyncServiceError));
  }

 protected:
  void SetUp() override {
    PassphraseTableViewControllerTest::SetUp();
    mock_sync_setup_service_ = static_cast<NiceMock<SyncSetupServiceMock>*>(
        SyncSetupServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            chrome_browser_state_.get(),
            base::BindRepeating(&CreateSyncSetupService)));
    // The other mocked functions of SyncSetupServiceMock return bools, so they
    // will by default return false.  GetSyncServiceState(), however, returns an
    // enum, and thus always needs its default value set.
    TurnSyncErrorOff();
  }

  void TearDown() override {
    [SyncController() stopObserving];
    PassphraseTableViewControllerTest::TearDown();
  }

  ChromeTableViewController* InstantiateController() override {
    return [[SyncEncryptionPassphraseTableViewController alloc]
        initWithBrowserState:chrome_browser_state_.get()];
  }

  SyncEncryptionPassphraseTableViewController* SyncController() {
    return static_cast<SyncEncryptionPassphraseTableViewController*>(
        controller());
  }

  // Weak, owned by |profile_|.
  NiceMock<SyncSetupServiceMock>* mock_sync_setup_service_;
};

TEST_F(SyncEncryptionPassphraseTableViewControllerTest, TestModel) {
  SyncEncryptionPassphraseTableViewController* controller = SyncController();
  EXPECT_EQ(1, NumberOfSections());
  EXPECT_EQ(2, NumberOfItemsInSection(0));
  // Passphrase message item.
  TableViewTextItem* item = GetTableViewItem(0, 0);
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_SYNC_ENTER_GOOGLE_PASSPHRASE_BODY),
              item.text);
  // Passphrase items.
  BYOTextFieldItem* passphraseItem = GetTableViewItem(0, 1);
  EXPECT_NSEQ(controller.passphrase, passphraseItem.textField);
}

TEST_F(SyncEncryptionPassphraseTableViewControllerTest,
       TestConstructorDestructor) {
  CreateController();
  CheckController();
  EXPECT_CALL(*fake_sync_service_->GetMockUserSettings(),
              SetDecryptionPassphrase(_))
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
  EXPECT_CALL(*fake_sync_service_, AddObserver(_)).Times(AtLeast(1));
  EXPECT_CALL(*fake_sync_service_, RemoveObserver(_)).Times(AtLeast(1));
  EXPECT_CALL(*fake_sync_service_->GetMockUserSettings(),
              SetDecryptionPassphrase(_));
  [[sync_controller passphrase] setText:@"decodeme"];
  // Set the return value for setting the passphrase to failure.
  ON_CALL(*fake_sync_service_->GetMockUserSettings(),
          SetDecryptionPassphrase(_))
      .WillByDefault(Return(false));
  [sync_controller signInPressed];
}

TEST_F(SyncEncryptionPassphraseTableViewControllerTest,
       TestDecryptCorrectPassphrase) {
  SyncEncryptionPassphraseTableViewController* sync_controller =
      SyncController();
  EXPECT_CALL(*fake_sync_service_, AddObserver(_)).Times(AtLeast(1));
  EXPECT_CALL(*fake_sync_service_, RemoveObserver(_)).Times(AtLeast(1));
  EXPECT_CALL(*fake_sync_service_->GetMockUserSettings(),
              SetDecryptionPassphrase(_));
  [[sync_controller passphrase] setText:@"decodeme"];
  // Set the return value for setting the passphrase to success.
  ON_CALL(*fake_sync_service_->GetMockUserSettings(),
          SetDecryptionPassphrase(_))
      .WillByDefault(Return(true));
  [sync_controller signInPressed];
}

TEST_F(SyncEncryptionPassphraseTableViewControllerTest,
       TestOnStateChangedWrongPassphrase) {
  SyncEncryptionPassphraseTableViewController* sync_controller =
      SyncController();
  // Sets up a UINavigationController that has |controller_| as the second view
  // controller on the navigation stack.
  SetUpNavigationController(sync_controller);
  EXPECT_EQ([nav_controller_ topViewController], sync_controller);

  // Set up the fake sync service to still require the passphrase.
  ON_CALL(*fake_sync_service_->GetMockUserSettings(), IsPassphraseRequired())
      .WillByDefault(Return(true));
  [sync_controller onSyncStateChanged];
  // The controller should only reload. Because there is text in the passphrase
  // field, the 'ok' button should be enabled.
  EXPECT_EQ([nav_controller_ topViewController], sync_controller);
}

TEST_F(SyncEncryptionPassphraseTableViewControllerTest,
       TestOnStateChangedCorrectPassphrase) {
  SyncEncryptionPassphraseTableViewController* sync_controller =
      SyncController();
  // Sets up a UINavigationController that has |controller_| as the second view
  // controller on the navigation stack.
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

TEST_F(SyncEncryptionPassphraseTableViewControllerTest, TestMessage) {
  SyncEncryptionPassphraseTableViewController* sync_controller =
      SyncController();
  // Default.
  EXPECT_FALSE([sync_controller syncErrorMessage]);

  SyncSetupService::SyncServiceState otherState =
      SyncSetupService::kSyncServiceSignInNeedsUpdate;

  // With a custom message.
  [sync_controller setSyncErrorMessage:@"message"];
  EXPECT_NSEQ(@"message", [sync_controller syncErrorMessage]);
  TurnSyncPassphraseErrorOn();
  EXPECT_NSEQ(@"message", [sync_controller syncErrorMessage]);
  TurnSyncOtherErrorOn(otherState);
  EXPECT_NSEQ(@"message", [sync_controller syncErrorMessage]);
  TurnSyncErrorOff();
  EXPECT_NSEQ(@"message", [sync_controller syncErrorMessage]);

  // With no custom message.
  [sync_controller setSyncErrorMessage:nil];
  EXPECT_FALSE([sync_controller syncErrorMessage]);
  TurnSyncPassphraseErrorOn();
  EXPECT_FALSE([sync_controller syncErrorMessage]);
  TurnSyncOtherErrorOn(otherState);
  EXPECT_NSEQ(GetSyncErrorMessageForBrowserState(chrome_browser_state_.get()),
              [sync_controller syncErrorMessage]);
  TurnSyncErrorOff();
  EXPECT_FALSE([sync_controller syncErrorMessage]);
}

}  // namespace
