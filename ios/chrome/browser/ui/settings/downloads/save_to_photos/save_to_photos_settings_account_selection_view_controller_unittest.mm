// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/downloads/save_to_photos/save_to_photos_settings_account_selection_view_controller.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/account_picker/ui_bundled/account_picker_selection/account_picker_selection_screen_identity_item_configurator.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_image_item.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller_test.h"
#import "ios/chrome/browser/ui/authentication/cells/table_view_identity_item.h"
#import "ios/chrome/browser/ui/settings/downloads/save_to_photos/fake_save_to_photos_settings_mutator.h"
#import "ios/chrome/browser/ui/settings/downloads/save_to_photos/save_to_photos_settings_account_selection_view_controller_action_delegate.h"
#import "ios/chrome/browser/ui/settings/downloads/save_to_photos/save_to_photos_settings_account_selection_view_controller_presentation_delegate.h"
#import "ios/chrome/browser/ui/settings/downloads/save_to_photos/save_to_photos_settings_mutator.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest_mac.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// Helper to create AccountPickerSelectionScreenIdentityItemConfigurator.
AccountPickerSelectionScreenIdentityItemConfigurator* CreateConfigurator(
    NSString* gaiaID,
    NSString* name,
    NSString* email,
    UIImage* avatar,
    BOOL selected) {
  AccountPickerSelectionScreenIdentityItemConfigurator* configurator =
      [[AccountPickerSelectionScreenIdentityItemConfigurator alloc] init];
  configurator.gaiaID = gaiaID;
  configurator.name = name;
  configurator.email = email;
  configurator.avatar = avatar;
  configurator.selected = selected;
  return configurator;
}

}  // namespace

// Fake presentation delegate.
@interface FakeSaveToPhotosSettingsAccountSelectionViewControllerPresentationDelegate
    : NSObject <
          SaveToPhotosSettingsAccountSelectionViewControllerPresentationDelegate>

@property(nonatomic, assign) BOOL wasRemovedCalled;

@end

@implementation
    FakeSaveToPhotosSettingsAccountSelectionViewControllerPresentationDelegate

- (void)saveToPhotosSettingsAccountSelectionViewControllerWasRemoved {
  self.wasRemovedCalled = YES;
}

@end

// Fake action delegate.
@interface FakeSaveToPhotosSettingsAccountSelectionViewControllerActionDelegate
    : NSObject <
          SaveToPhotosSettingsAccountSelectionViewControllerActionDelegate>

@property(nonatomic, assign) BOOL addAccountCalled;

@end

@implementation
    FakeSaveToPhotosSettingsAccountSelectionViewControllerActionDelegate

- (void)saveToPhotosSettingsAccountSelectionViewControllerAddAccount {
  self.addAccountCalled = YES;
}

@end

class SaveToPhotosSettingsAccountSelectionViewControllerTest
    : public LegacyChromeTableViewControllerTest {
 protected:
  void SetUp() final {
    LegacyChromeTableViewControllerTest::SetUp();

    mutator_ = [[FakeSaveToPhotosSettingsMutator alloc] init];
    presentation_delegate_ =
        [[FakeSaveToPhotosSettingsAccountSelectionViewControllerPresentationDelegate
            alloc] init];
    action_delegate_ =
        [[FakeSaveToPhotosSettingsAccountSelectionViewControllerActionDelegate
            alloc] init];

    CreateController();
  }

  void TearDown() final { LegacyChromeTableViewControllerTest::TearDown(); }

  LegacyChromeTableViewController* InstantiateController() final {
    SaveToPhotosSettingsAccountSelectionViewController* controller =
        [[SaveToPhotosSettingsAccountSelectionViewController alloc] init];
    controller.mutator = mutator_;
    controller.presentationDelegate = presentation_delegate_;
    controller.actionDelegate = action_delegate_;
    return controller;
  }

  FakeSaveToPhotosSettingsMutator* mutator_;
  FakeSaveToPhotosSettingsAccountSelectionViewControllerPresentationDelegate*
      presentation_delegate_;
  FakeSaveToPhotosSettingsAccountSelectionViewControllerActionDelegate*
      action_delegate_;
};

// Tests that the table loads the expected model after content is pushed through
// the consumer interface. Also tests that the mutator is called when an
// identity is selected, the action delegate is called when the user selects
// "Add account" and the presentation delegate is called when the view
// controller is removed.
TEST_F(SaveToPhotosSettingsAccountSelectionViewControllerTest,
       CanSelectAndAddAccount) {
  // Populate the table with three accounts.
  NSArray<AccountPickerSelectionScreenIdentityItemConfigurator*>*
      configurators = @[
        CreateConfigurator(@"gaiaID1", @"Person One", @"user1@example.com", nil,
                           YES),
        CreateConfigurator(@"gaiaID2", @"Person Two", @"user2@example.com", nil,
                           NO),
        CreateConfigurator(@"gaiaID3", @"Person Three", @"user3@example.com",
                           nil, NO),
      ];
  SaveToPhotosSettingsAccountSelectionViewController*
      accountSelectionController = base::apple::ObjCCast<
          SaveToPhotosSettingsAccountSelectionViewController>(controller());
  ASSERT_TRUE(accountSelectionController);
  [accountSelectionController populateAccountsOnDevice:configurators];

  // Test that there are two section and the expected title.
  EXPECT_EQ(2, NumberOfSections());
  CheckTitleWithId(IDS_IOS_SETTINGS_DOWNLOADS_SAVE_TO_PHOTOS_HEADER);

  // Test account selection section has expected content.
  CheckSectionHeaderWithId(IDS_IOS_SETTINGS_DOWNLOADS_ACCOUNT_SELECTION_HEADER,
                           0);
  for (size_t i = 0; i < configurators.count; i++) {
    TableViewIdentityItem* item =
        base::apple::ObjCCast<TableViewIdentityItem>(GetTableViewItem(0, i));
    EXPECT_TRUE(item);
    EXPECT_NSEQ(configurators[i].gaiaID, item.gaiaID);
    EXPECT_NSEQ(configurators[i].name, item.name);
    EXPECT_NSEQ(configurators[i].email, item.email);
    EXPECT_NSEQ(configurators[i].avatar, item.avatar);
    EXPECT_EQ(configurators[i].selected, item.selected);
    EXPECT_EQ(IdentityViewStyleConsistency, item.identityViewStyle);
  }

  // Test that selecting an identity in the table calls the mutator.
  EXPECT_FALSE(mutator_.selectedIdentityGaiaID);
  [accountSelectionController tableView:controller().tableView
                didSelectRowAtIndexPath:[NSIndexPath indexPathForItem:1
                                                            inSection:0]];
  EXPECT_NSEQ(@"gaiaID2", mutator_.selectedIdentityGaiaID);
  [accountSelectionController tableView:controller().tableView
                didSelectRowAtIndexPath:[NSIndexPath indexPathForItem:0
                                                            inSection:0]];
  EXPECT_NSEQ(@"gaiaID1", mutator_.selectedIdentityGaiaID);

  // Test Add account section has expected content.
  TableViewImageItem* addAccountItem =
      base::apple::ObjCCast<TableViewImageItem>(GetTableViewItem(1, 0));
  EXPECT_TRUE(addAccountItem);
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_CONSISTENCY_PROMO_ADD_ACCOUNT),
              addAccountItem.title);
  EXPECT_NSEQ([UIColor colorNamed:kBlueColor], addAccountItem.textColor);

  // Test that tapping the Add account button calls the action delegate.
  EXPECT_FALSE(action_delegate_.addAccountCalled);
  [accountSelectionController tableView:controller().tableView
                didSelectRowAtIndexPath:[NSIndexPath indexPathForItem:0
                                                            inSection:1]];
  EXPECT_TRUE(action_delegate_.addAccountCalled);

  // Test that removing the controller calls the presentation delegate.
  EXPECT_FALSE(presentation_delegate_.wasRemovedCalled);
  [controller() didMoveToParentViewController:nil];
  EXPECT_TRUE(presentation_delegate_.wasRemovedCalled);
}
